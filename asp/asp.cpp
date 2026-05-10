#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "../shared/game_state.h"
#include "../resources/shared_mem_abs.h"
#include "../Characters/Enemy.h"

struct ASPContext;

struct EnemyThreadCtx
{
    int        enemyIndex;
    SharedMemoryBlock* shm;
    ASPContext* ctx;
};

struct ASPContext
{
    SharedMemoryBlock* shm;
    int                         numEnemies;
    int                         running;
    pthread_mutex_t             running_mutex;
    std::vector<pthread_t>      enemyTids;
    std::vector<EnemyThreadCtx> enemyCtxs;
};

// ─────────────────────────────────────────────────────────────────────────────
//  running helpers
// ─────────────────────────────────────────────────────────────────────────────
static void set_running(ASPContext* ctx, int val)
{
    pthread_mutex_lock(&ctx->running_mutex);
    ctx->running = val;
    pthread_mutex_unlock(&ctx->running_mutex);
}

static int get_running(ASPContext* ctx)
{
    pthread_mutex_lock(&ctx->running_mutex);
    int val = ctx->running;
    pthread_mutex_unlock(&ctx->running_mutex);
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Globals for signal handlers
// ─────────────────────────────────────────────────────────────────────────────
static ASPContext* g_ctx = nullptr;

static void onSigusr1(int)
{
    if (g_ctx) {
        // Lock to prevent "Lost Wakeup" when applying Stun
        pthread_mutex_lock(&g_ctx->shm->global_mutex);
        pthread_cond_broadcast(&g_ctx->shm->turn_condition);
        pthread_mutex_unlock(&g_ctx->shm->global_mutex);
    }
}

static void onSigterm(int)
{
    if (g_ctx)
    {
        std::cout << "\n[ASP] Received SIGTERM from Arbiter. Shutting down enemy threads...\n";
        set_running(g_ctx, 0);

        // FIX: Lock the mutex before broadcasting to prevent "Lost Wakeup" race conditions
        // ensuring threads gracefully exit rather than hanging forever!
        pthread_mutex_lock(&g_ctx->shm->global_mutex);
        pthread_cond_broadcast(&g_ctx->shm->turn_condition);
        pthread_mutex_unlock(&g_ctx->shm->global_mutex);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  decide_action — Enemy AI
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
//  decide_action — Smart Heuristic Enemy AI
// ─────────────────────────────────────────────────────────────────────────────
static void decide_action(int enemyIndex, SharedMemoryBlock* shm)
{
    // 1. Identify available targets and find the weakest link
    int numPlayers = shm->state.num_active_players;
    int alivePlayers[4];
    int aliveCount = 0;

    int weakestPlayerIdx = -1;
    int lowestHP = 999999;

    for (int i = 0; i < numPlayers; i++) {
        if (shm->state.players[i].isAlive()) {
            alivePlayers[aliveCount++] = i;
            if (shm->state.players[i].getHp() < lowestHP) {
                lowestHP = shm->state.players[i].getHp();
                weakestPlayerIdx = i;
            }
        }
    }

    // Edge case: everyone is dead
    if (aliveCount == 0) {
        shm->asp_mailbox.action_type          = Action::SKIP;
        shm->asp_mailbox.requesting_entity_id = enemyIndex;
        shm->asp_mailbox.is_ready             = true;
        return;
    }

    // 2. TACTIC: Actively Hunt for Artifacts!
    // If we don't have an artifact, check if any of the 3 are lying on the ground.
    if (shm->state.enemies_artifact_state[enemyIndex].holding_artifact_idx == -1) {
        int desired_artifact = -1;
        // Check backwards (2 to 0) so Eclipse Relic is highest priority
        for (int a = 2; a >= 0; --a) {
            if (shm->state.artifacts[a].isAvailable()) {
                desired_artifact = a;
                break;
            }
        }

        // 80% chance to drop everything and grab the artifact if it's there
        if (desired_artifact != -1 && (rand() % 100 < 80)) {
            shm->asp_mailbox.action_type          = Action::GET_ARTIFACT;
            shm->asp_mailbox.requesting_entity_id = enemyIndex;
            shm->asp_mailbox.weapon_id            = shm->state.artifacts[desired_artifact].getWeaponId();
            shm->asp_mailbox.is_ready             = true;
            std::cout << "[ASP] TACTIC: Enemy " << enemyIndex << " is lunging for an Artifact!\n";
            return;
        }
    }

    // 3. TACTIC: Stand Guard / Hesitate
    // 10% chance to just guard (SKIP) to preserve 50% stamina and act again faster
    if (rand() % 100 < 10) {
        shm->asp_mailbox.action_type          = Action::SKIP;
        shm->asp_mailbox.requesting_entity_id = enemyIndex;
        shm->asp_mailbox.is_ready             = true;
        std::cout << "[ASP] TACTIC: Enemy " << enemyIndex << " stands its ground (SKIP).\n";
        return;
    }

    // 4. TACTIC: Execute Combat
    // Default to striking the weakest player to eliminate threats quickly.
    int target = weakestPlayerIdx;

    // Occasionally (30% chance) strike a random player to add unpredictability
    if (rand() % 100 < 30) {
        target = alivePlayers[rand() % aliveCount];
    }

    shm->asp_mailbox.action_type          = Action::STRIKE;
    shm->asp_mailbox.requesting_entity_id = enemyIndex;
    shm->asp_mailbox.target_id            = target;
    shm->asp_mailbox.is_ready             = true;
}
// ─────────────────────────────────────────────────────────────────────────────
//  enemyThreadFunc — one per enemy
// ─────────────────────────────────────────────────────────────────────────────
static void* enemyThreadFunc(void* arg)
{
    EnemyThreadCtx* ctx = (EnemyThreadCtx*)arg;
    SharedMemoryBlock* shm = ctx->shm;
    const int          me  = ctx->enemyIndex;

    while (get_running(ctx->ctx))
    {
        // ── PHASE 1: Wait until it is MY turn ────────────────────────────
        pthread_mutex_lock(&shm->global_mutex);

        while (get_running(ctx->ctx))
        {
            if (!shm->state.enemies[me].isAlive()) break;

            int  owner  = shm->state.current_turn_owner_id;
            bool myTurn = !shm->state.is_player_turn &&
                          (owner == shm->state.num_active_players + me);

            if (myTurn) break;
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        }

        // ── PHASE 2: Exit if dead or shutting down ────────────────────────
        if (!get_running(ctx->ctx) || !shm->state.enemies[me].isAlive())
        {
            pthread_mutex_unlock(&shm->global_mutex);
            std::cout << "[ASP] Enemy " << me << " has fallen. Thread terminating.\n";
            break;
        }

        // ── PHASE 3: Decide and post action ──────────────────────────────
        if (shm->state.enemies[me].isStunned())
        {
            std::cout << "[ASP] Enemy " << me << " is STUNNED — forced SKIP.\n";
            shm->asp_mailbox.action_type          = Action::SKIP;
            shm->asp_mailbox.requesting_entity_id = me;
            shm->asp_mailbox.is_ready             = true;
        }
        else
        {
            decide_action(me, shm);
        }

        pthread_cond_broadcast(&shm->turn_condition);
        pthread_mutex_unlock(&shm->global_mutex);

        // ── PHASE 4: CONSUMED GATE ────────────────────────────────────────
        // Prevents the "hundreds of SKIPs" infinite loop race condition
        pthread_mutex_lock(&shm->global_mutex);
        while (shm->asp_mailbox.is_ready && get_running(ctx->ctx))
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        pthread_mutex_unlock(&shm->global_mutex);
    }

    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  spawnEnemyThreads / joinEnemyThreads
// ─────────────────────────────────────────────────────────────────────────────
static void spawnEnemyThreads(ASPContext* ctx)
{
    int n = ctx->shm->state.num_active_enemies;
    ctx->numEnemies = n;
    ctx->enemyTids.resize(n);
    ctx->enemyCtxs.resize(n);

    for (int i = 0; i < n; i++)
    {
        EnemyThreadCtx& ectx = ctx->enemyCtxs[i];
        ectx.enemyIndex = i;
        ectx.shm        = ctx->shm;
        ectx.ctx        = ctx;

        int rc = pthread_create(&ctx->enemyTids[i], nullptr,
                                enemyThreadFunc, &ectx);
        if (rc != 0)
            std::cerr << "[ASP] pthread_create enemy "
                      << i << " failed: " << strerror(rc) << "\n";
        else
            std::cout << "[ASP] Enemy thread " << i << " spawned\n";
    }
}

static void joinEnemyThreads(ASPContext* ctx)
{
    for (int i = 0; i < ctx->numEnemies; i++)
        pthread_join(ctx->enemyTids[i], nullptr);

    ctx->enemyTids.clear();
    ctx->enemyCtxs.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    if (argc < 2) { std::cerr << "[ASP] Usage: asp <shm_name>\n"; return 1; }

    const char* shmName = argv[1];

    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = onSigusr1;
    sigaction(SIGUSR1, &sa, nullptr);
    sa.sa_handler = onSigterm;
    sigaction(SIGTERM, &sa, nullptr);

    int fd = shm_open(shmName, O_RDWR, 0666);
    if (fd < 0) { perror("[ASP] shm_open"); return 1; }

    SharedMemoryBlock* shm = (SharedMemoryBlock*)mmap(
        nullptr, sizeof(SharedMemoryBlock),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) { perror("[ASP] mmap"); return 1; }

    std::cout << "[ASP] Attached to shared memory\n";

    ASPContext ctx;
    ctx.shm     = shm;
    ctx.running = 1;
    pthread_mutex_init(&ctx.running_mutex, nullptr);
    g_ctx = &ctx;

    pthread_mutex_lock(&shm->global_mutex);
    while (shm->state.num_active_enemies == 0 && get_running(&ctx))
        pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
    pthread_mutex_unlock(&shm->global_mutex);

    while (get_running(&ctx))
    {
        spawnEnemyThreads(&ctx);
        std::cout << "[ASP] " << ctx.numEnemies << " enemy threads running\n";

        joinEnemyThreads(&ctx);

        if (!get_running(&ctx)) break;

        pthread_mutex_lock(&shm->global_mutex);
        while (shm->state.num_active_enemies == 0 && get_running(&ctx))
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        pthread_mutex_unlock(&shm->global_mutex);
    }

    pthread_mutex_destroy(&ctx.running_mutex);
    munmap(shm, sizeof(SharedMemoryBlock));
    return 0;
}
