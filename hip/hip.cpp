#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <algorithm>
#include <iostream>
#include <vector>
#include <signal.h>
#include <functional>
#include "../shared/game_state.h"
#include "../resources/shared_mem_abs.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ActionSlot — one per player
// ─────────────────────────────────────────────────────────────────────────────
struct ActionSlot
{
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool            ready;
    Action          action;
    int             targetIdx;
    int             weaponIdx;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Contexts
// ─────────────────────────────────────────────────────────────────────────────
struct HIPContext;

struct PlayerThreadCtx
{
    int                playerIndex;
    SharedMemoryBlock* shm;
    ActionSlot* slot;
    HIPContext* ctx;
};

struct HIPContext
{
    SharedMemoryBlock* shm;
    int                          numPlayers;
    int                          running;        // Pure int!
    pthread_mutex_t              running_mutex;  // POSIX Mutex!
    std::vector<ActionSlot>      slots;
    std::vector<pthread_t>       playerTids;
    std::vector<PlayerThreadCtx> playerCtxs;
    std::array<PlayerType, 4>    playerTypes;
};

// ─────────────────────────────────────────────────────────────────────────────
//  running helpers (POSIX strictly)
// ─────────────────────────────────────────────────────────────────────────────
static void set_running(HIPContext* ctx, int val)
{
    pthread_mutex_lock(&ctx->running_mutex);
    ctx->running = val;
    pthread_mutex_unlock(&ctx->running_mutex);
}

static int get_running(HIPContext* ctx)
{
    pthread_mutex_lock(&ctx->running_mutex);
    int val = ctx->running;
    pthread_mutex_unlock(&ctx->running_mutex);
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
//  pushLog
// ─────────────────────────────────────────────────────────────────────────────
static void pushLog(SharedMemoryBlock* shm, const char* fmt, ...)
{
    char buf[ACTION_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ActionLog& log = shm->state.action_log;
    strncpy(log.messages[log.head], buf, ACTION_MSG_LEN - 1);
    log.messages[log.head][ACTION_MSG_LEN - 1] = '\0';
    log.head  = (log.head + 1) % ACTION_LOG_SIZE;
    log.count = std::min(log.count + 1, ACTION_LOG_SIZE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  submitAction
// ─────────────────────────────────────────────────────────────────────────────
static void submitAction(HIPContext* ctx, Action action, int targetIdx, int weaponIdx = -1)
{
    pthread_mutex_lock(&ctx->shm->global_mutex);
    bool isPlayerTurn = ctx->shm->state.is_player_turn;
    int  active       = ctx->shm->state.current_turn_owner_id;
    pthread_mutex_unlock(&ctx->shm->global_mutex);
    usleep(10000);

    if (!isPlayerTurn || active < 0 || active >= ctx->numPlayers) return;

    ActionSlot* slot = &ctx->slots[active];
    pthread_mutex_lock(&slot->mutex);
    slot->action    = action;
    slot->targetIdx = targetIdx;
    slot->weaponIdx = weaponIdx;
    slot->ready     = true;
    pthread_cond_signal(&slot->cond);
    pthread_mutex_unlock(&slot->mutex);
}

static void* setupThread(void* args)
{
    HIPContext* ctx   = (HIPContext*)args;
    SharedMemoryBlock* block = ctx->shm;

    pthread_mutex_lock(&block->global_mutex);
    while (block->state.current_turn_owner_id != -2)
        pthread_cond_wait(&block->turn_condition, &block->global_mutex);

    block->hip_mailbox.action_type          = Action::SETUP_GAME;
    block->hip_mailbox.requesting_entity_id = -1;
    block->hip_mailbox.target_id            = ctx->numPlayers;

    for (int i = 0; i < ctx->numPlayers; i++)
        block->hip_mailbox.types[i] = ctx->playerTypes[i];

    block->hip_mailbox.is_ready = true;

    pushLog(block, "[HIP] Setup sent: %d players", ctx->numPlayers);
    pthread_cond_broadcast(&block->turn_condition);
    pthread_mutex_unlock(&block->global_mutex);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  waitForAction
// ─────────────────────────────────────────────────────────────────────────────
static void waitForAction(ActionSlot* slot, HIPContext* ctx, Action& action, int& targetIdx, int& weaponIdx)
{
    pthread_mutex_lock(&slot->mutex);
    while (!slot->ready && get_running(ctx))
        pthread_cond_wait(&slot->cond, &slot->mutex);

    action    = slot->action;
    targetIdx = slot->targetIdx;
    weaponIdx = slot->weaponIdx;
    slot->ready = false;
    pthread_mutex_unlock(&slot->mutex);
}

// ─────────────────────────────────────────────────────────────────────────────
//  playerThreadFunc
// ─────────────────────────────────────────────────────────────────────────────
static void* playerThreadFunc(void* arg)
{
    PlayerThreadCtx* pctx = (PlayerThreadCtx*)arg;
    HIPContext* ctx  = pctx->ctx;
    SharedMemoryBlock* shm  = ctx->shm;
    const int          me   = pctx->playerIndex;

    while (get_running(ctx))
    {
        // 1. Wait until it is MY turn
        pthread_mutex_lock(&shm->global_mutex);
        while (get_running(ctx))
        {
            if (shm->state.is_player_turn && shm->state.current_turn_owner_id == me) break;
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        }
        pthread_mutex_unlock(&shm->global_mutex);
        if (!get_running(ctx)) break;

        // 2. Drain stale slot
        pthread_mutex_lock(&pctx->slot->mutex);
        pctx->slot->ready = false;
        pthread_mutex_unlock(&pctx->slot->mutex);

        // 3. Wait for renderer keypress
        Action action;
        int targetIdx, weaponIdx;
        waitForAction(pctx->slot, ctx, action, targetIdx, weaponIdx);
        if (!get_running(ctx)) break;

        // 4. Post to hip_mailbox and wake Arbiter
        pthread_mutex_lock(&shm->global_mutex);
        shm->hip_mailbox.requesting_entity_id = me;
        shm->hip_mailbox.action_type          = action;
        shm->hip_mailbox.target_id            = targetIdx;
        shm->hip_mailbox.weapon_id            = weaponIdx;
        shm->hip_mailbox.is_ready             = true;
        pushLog(shm, "Player %d act=%d tgt=%d wpn=%d", me, static_cast<int>(action), targetIdx, weaponIdx);
        pthread_cond_broadcast(&shm->turn_condition);
        pthread_mutex_unlock(&shm->global_mutex);
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  spawnPlayerThreads
// ─────────────────────────────────────────────────────────────────────────────
static void spawnPlayerThreads(HIPContext* ctx)
{
    ctx->slots.resize(ctx->numPlayers);
    ctx->playerCtxs.resize(ctx->numPlayers);
    ctx->playerTids.resize(ctx->numPlayers);

    for (int i = 0; i < ctx->numPlayers; i++)
    {
        ActionSlot& slot = ctx->slots[i];
        pthread_mutex_init(&slot.mutex, nullptr);
        pthread_cond_init (&slot.cond,  nullptr);
        slot.ready = false; slot.action = Action::SKIP;
        slot.targetIdx = -1; slot.weaponIdx = -1;

        PlayerThreadCtx& pctx = ctx->playerCtxs[i];
        pctx.playerIndex = i;
        pctx.shm         = ctx->shm;
        pctx.slot        = &slot;
        pctx.ctx         = ctx;

        int rc = pthread_create(&ctx->playerTids[i], nullptr, playerThreadFunc, &pctx);
        if (rc != 0) std::cerr << "[HIP] pthread_create player " << i << " failed: " << strerror(rc) << "\n";
        else std::cout << "[HIP] Player thread " << i << " spawned\n";
    }
}

static void joinAndCleanup(HIPContext* ctx)
{
    for (int i = 0; i < ctx->numPlayers; i++)
    {
        pthread_join(ctx->playerTids[i], nullptr);
        pthread_mutex_destroy(&ctx->slots[i].mutex);
        pthread_cond_destroy (&ctx->slots[i].cond);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    if (argc < 2) { std::cerr << "[HIP] Usage: hip <shm_name>\n"; return 1; }

    const char* shmName = argv[1];

    int fd = shm_open(shmName, O_RDWR, 0666);
    if (fd < 0) { perror("[HIP] shm_open"); return 1; }
    SharedMemoryBlock* shm = (SharedMemoryBlock*)mmap(
        nullptr, sizeof(SharedMemoryBlock),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) { perror("[HIP] mmap"); return 1; }
    std::cout << "[HIP] Attached to shared memory\n";

    HIPContext ctx;
    ctx.shm        = shm;
    ctx.numPlayers = 2;
    ctx.running    = 1;
    pthread_mutex_init(&ctx.running_mutex, nullptr);
    ctx.playerTypes[0] = PlayerType::CHRONO;
    ctx.playerTypes[1] = PlayerType::FROG;

    pthread_t setupTid;
    pthread_create(&setupTid, nullptr, setupThread, &ctx);
    pthread_join(setupTid, nullptr);
    std::cout << "[HIP] Setup complete — spawning player threads\n";

    spawnPlayerThreads(&ctx);

    std::cout << "[HIP] Terminal mode. Commands: s=strike, h=heal, k=skip, g=get_artifact, r=release, q=quit\n";
    std::cout << "      Format: <command> <target_enemy_index_OR_artifact_id>\n";
    std::cout << "      Example: s 0   (strike enemy 0) OR g 1 (get artifact 1)\n";

    while (get_running(&ctx))
    {
        char cmd;
        int  target = 0;
        std::cout << "> ";
        std::cin >> cmd >> target;

        Action action;
        int target_id = -1;
        int weapon_id = -1;

        switch(cmd) {
            case 's': action = Action::STRIKE;  target_id = target; break;
            case 'h': action = Action::HEAL;    target_id = target; break;
            case 'k': action = Action::SKIP;    target_id = target; break;
            case 'g': action = Action::GET_ARTIFACT; weapon_id = target; break;
            case 'r': action = Action::RELEASE_ARTIFACT; weapon_id = target; break;
            case 'q': set_running(&ctx, 0); continue;
            default: std::cout << "Unknown command\n"; continue;
        }
        submitAction(&ctx, action, target_id, weapon_id);
    }

    joinAndCleanup(&ctx);
    pthread_mutex_destroy(&ctx.running_mutex);
    munmap(shm, sizeof(SharedMemoryBlock));
    return 0;
}
