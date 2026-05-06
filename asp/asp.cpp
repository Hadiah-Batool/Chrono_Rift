#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "../shared/shared_types.h"
#include "../resources/shared_mem_abs.h"
#include "../Characters/Enemy.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ASPContext — forward declared so EnemyThreadCtx can point to it
// ─────────────────────────────────────────────────────────────────────────────
struct ASPContext;

// ─────────────────────────────────────────────────────────────────────────────
//  Per-enemy thread context
// ─────────────────────────────────────────────────────────────────────────────
struct EnemyThreadCtx
{
    int        enemyIndex;
    SharedMemoryBlock* shm;
    ASPContext* ctx;        // pointer back to parent for running flag
};

// ─────────────────────────────────────────────────────────────────────────────
//  ASPContext
// ─────────────────────────────────────────────────────────────────────────────
struct ASPContext
{
    SharedMemoryBlock*          shm;
    int                         numEnemies;
    int                         running;        // 1 = alive, 0 = stop
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
    // stun timestamp already written into shm by arbiter
    // enemy threads check isStunned() themselves
}

static void onSigterm(int)
{
    if (g_ctx)
    {
        set_running(g_ctx, 0);
        // wake all enemy threads so they can exit their cond_wait
        pthread_cond_broadcast(&g_ctx->shm->turn_condition);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  decide_action — simple enemy AI
// ─────────────────────────────────────────────────────────────────────────────
static void decide_action(int enemyIndex, SharedMemoryBlock* shm)
{
    int numPlayers = shm->state.num_active_players;

    // collect alive players
    int alivePlayers[4];
    int aliveCount = 0;
    for (int i = 0; i < numPlayers; i++)
        if (shm->state.players[i].isAlive())
            alivePlayers[aliveCount++] = i;

    if (aliveCount == 0) return;

    int target = alivePlayers[rand() % aliveCount];

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
    EnemyThreadCtx*    ctx = (EnemyThreadCtx*)arg;
    SharedMemoryBlock* shm = ctx->shm;
    const int          me  = ctx->enemyIndex;

    while (get_running(ctx->ctx))
    {
        // 1. Wait until it is MY turn
        pthread_mutex_lock(&shm->global_mutex);

        while (get_running(ctx->ctx))
        {
            int  owner  = shm->state.current_turn_owner_id;
            bool myTurn = !shm->state.is_player_turn &&
                          (owner == shm->state.num_active_players + me);

            if (myTurn) break;
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        }

        if (!get_running(ctx->ctx))
        {
            pthread_mutex_unlock(&shm->global_mutex);
            break;
        }

        // 2. If stunned — release lock, arbiter will timeout and SKIP us
        if (shm->state.enemies[me].isStunned())
        {
            pthread_mutex_unlock(&shm->global_mutex);
            continue;
        }

        // 3. Decide action and post to mailbox
        decide_action(me, shm);

        pthread_cond_broadcast(&shm->turn_condition);  // wake arbiter
        pthread_mutex_unlock(&shm->global_mutex);
    }

    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  spawnEnemyThreads
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
//  main — argv[1] = shm_name
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "[ASP] Usage: asp <shm_name>\n";
        return 1;
    }

    const char* shmName = argv[1];

    // ── Signal handlers ───────────────────────────────────────────────────
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = onSigusr1;
    sigaction(SIGUSR1, &sa, nullptr);

    sa.sa_handler = onSigterm;
    sigaction(SIGTERM, &sa, nullptr);

    // SIGSTOP and SIGCONT need no handler —
    // arbiter sends SIGSTOP to freeze entire ASP process for Ultimate Ability
    // kernel handles it, SIGCONT resumes automatically

    // ── Attach shared memory ──────────────────────────────────────────────
    int fd = shm_open(shmName, O_RDWR, 0666);
    if (fd < 0) { perror("[ASP] shm_open"); return 1; }

    SharedMemoryBlock* shm = (SharedMemoryBlock*)mmap(
        nullptr, sizeof(SharedMemoryBlock),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) { perror("[ASP] mmap"); return 1; }

    std::cout << "[ASP] Attached to shared memory\n";

    // ── Build context ─────────────────────────────────────────────────────
    ASPContext ctx;
    ctx.shm     = shm;
    ctx.running = 1;
    pthread_mutex_init(&ctx.running_mutex, nullptr);
    g_ctx = &ctx;

    // ── Wait for arbiter to populate enemies after setup ──────────────────
    pthread_mutex_lock(&shm->global_mutex);
    while (shm->state.num_active_enemies == 0 && get_running(&ctx))
        pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
    pthread_mutex_unlock(&shm->global_mutex);

    // ── Game loop — respawn threads when new wave of enemies arrives ──────
    while (get_running(&ctx))
    {
        spawnEnemyThreads(&ctx);
        std::cout << "[ASP] " << ctx.numEnemies << " enemy threads running\n";

        joinEnemyThreads(&ctx);  // blocks until all enemies die or stop

        if (!get_running(&ctx)) break;

        // all enemies dead — wait for arbiter to spawn next wave
        pthread_mutex_lock(&shm->global_mutex);
        while (shm->state.num_active_enemies == 0 && get_running(&ctx))
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        pthread_mutex_unlock(&shm->global_mutex);
    }

    pthread_mutex_destroy(&ctx.running_mutex);
    munmap(shm, sizeof(SharedMemoryBlock));
    return 0;
}