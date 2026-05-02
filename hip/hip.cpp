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
#include <atomic>           
#include "../shared/shared_types.h"
#include "../resources/shared_mem_abs.h"
#include "../DisplayRendering/render.h"


enum class Action {
    STRIKE      = 0,
    EXHAUST     = 1,
    USE_WEAPON  = 2,
    SWAP_IN     = 3,
    HEAL        = 4,
    SKIP        = 5
};

// ─────────────────────────────────────────────────────────────────────────────
//  ActionSlot — one per player
//  Renderer PRODUCES (on keypress), player thread CONSUMES (on its turn)
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
//  Per-player thread context
// ─────────────────────────────────────────────────────────────────────────────
struct PlayerThreadCtx
{
    int                playerIndex;
    SharedMemoryBlock* shm;
    ActionSlot*        slot;
    std::atomic<bool>* running;     // FIX: atomic so cross-thread reads are safe
};

// ─────────────────────────────────────────────────────────────────────────────
//  HIPContext
// ─────────────────────────────────────────────────────────────────────────────
struct HIPContext
{
    SharedMemoryBlock*           shm;
    Renderer*                    renderer;   // pointer only — owned on stack in main
    int                          numPlayers;
    std::atomic<bool>            running;    // FIX: atomic
    std::vector<ActionSlot>      slots;
    std::vector<pthread_t>       playerTids;
    std::vector<PlayerThreadCtx> playerCtxs;
};

// ─────────────────────────────────────────────────────────────────────────────
//  pushLog — writes into shm action_log
//  Caller must already hold global_mutex
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
//  Called by renderer on keypress — posts to the ACTIVE player's slot only.
// ─────────────────────────────────────────────────────────────────────────────
static void submitAction(HIPContext* ctx, int action, int targetIdx, int weaponIdx = -1)
{
    pthread_mutex_lock(&ctx->shm->global_mutex);
    bool isPlayerTurn = ctx->shm->state.is_player_turn;
    int  active       = ctx->shm->state.current_turn_owner_id;
    pthread_mutex_unlock(&ctx->shm->global_mutex);

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

// ─────────────────────────────────────────────────────────────────────────────
//  waitForAction
// ─────────────────────────────────────────────────────────────────────────────
static void waitForAction(ActionSlot* slot, std::atomic<bool>* running,
                          int& action, int& targetIdx, int& weaponIdx)
{
    pthread_mutex_lock(&slot->mutex);
    while (!slot->ready && running->load())
        pthread_cond_wait(&slot->cond, &slot->mutex);

    action    = slot->action;
    targetIdx = slot->targetIdx;
    weaponIdx = slot->weaponIdx;
    slot->ready = false;
    pthread_mutex_unlock(&slot->mutex);
}

// ─────────────────────────────────────────────────────────────────────────────
//  playerThreadFunc  (one per player — spec)
// ─────────────────────────────────────────────────────────────────────────────
static void* playerThreadFunc(void* arg)
{
    PlayerThreadCtx*   ctx = (PlayerThreadCtx*)arg;
    SharedMemoryBlock* shm = ctx->shm;
    const int          me  = ctx->playerIndex;

    while (ctx->running->load())
    {
        // 1. Wait until it is MY turn
        pthread_mutex_lock(&shm->global_mutex);
        while (ctx->running->load())
        {
            if (shm->state.is_player_turn &&
                shm->state.current_turn_owner_id == me)
                break;
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        }
        pthread_mutex_unlock(&shm->global_mutex);
        if (!ctx->running->load()) break;

        // 2. Drain stale slot
        pthread_mutex_lock(&ctx->slot->mutex);
        ctx->slot->ready = false;
        pthread_mutex_unlock(&ctx->slot->mutex);

        // 3. Wait for renderer keypress
        int action, targetIdx, weaponIdx;
        waitForAction(ctx->slot, ctx->running, action, targetIdx, weaponIdx);
        if (!ctx->running->load()) break;

        // 4. Post to hip_mailbox and wake Arbiter
        pthread_mutex_lock(&shm->global_mutex);
        shm->hip_mailbox.requesting_entity_id = me;
        shm->hip_mailbox.action_type          = action;
        shm->hip_mailbox.target_id            = targetIdx;
        shm->hip_mailbox.weapon_id            = weaponIdx;
        shm->hip_mailbox.is_ready             = true;
        pushLog(shm, "Player %d act=%d tgt=%d wpn=%d", me, action, targetIdx, weaponIdx);
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
        slot.ready = false; slot.action = ACTION_SKIP;
        slot.targetIdx = -1; slot.weaponIdx = -1;

        PlayerThreadCtx& pctx = ctx->playerCtxs[i];
        pctx.playerIndex = i;
        pctx.shm         = ctx->shm;
        pctx.slot        = &slot;
        pctx.running     = &ctx->running;

        int rc = pthread_create(&ctx->playerTids[i], nullptr, playerThreadFunc, &pctx);
        if (rc != 0)
            std::cerr << "[HIP] pthread_create player " << i << " failed: " << strerror(rc) << "\n";
        else
            std::cout << "[HIP] Player thread " << i << " spawned\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  renderThread
// ─────────────────────────────────────────────────────────────────────────────
static void* renderThread(void* arg)
{
    HIPContext* ctx = (HIPContext*)arg;
    ctx->renderer->run();
    ctx->running.store(false);
    for (int i = 0; i < ctx->numPlayers; i++)
        pthread_cond_broadcast(&ctx->slots[i].cond);
    pthread_mutex_lock(&ctx->shm->global_mutex);
    pthread_cond_broadcast(&ctx->shm->turn_condition);
    pthread_mutex_unlock(&ctx->shm->global_mutex);
    return nullptr;
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

static pid_t g_arbiterPid = -1;
static void onSigterm(int) { if (g_arbiterPid > 0) kill(g_arbiterPid, SIGTERM); }

// ─────────────────────────────────────────────────────────────────────────────
//  main
//  argv[1] shm_name  argv[2] num_players  argv[3] arbiter_pid
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "[HIP] Usage: hip <shm_name> <num_players> <arbiter_pid>\n";
        return 1;
    }

    const char* shmName    = argv[1];
    int         numPlayers = std::atoi(argv[2]);
    g_arbiterPid           = (pid_t)std::atoi(argv[3]);

    if (numPlayers < 1 || numPlayers > 4)
    {
        std::cerr << "[HIP] num_players must be 1-4\n";
        return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = onSigterm;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, nullptr);

    // Attach to shared memory
    int fd = shm_open(shmName, O_RDWR, 0666);
    if (fd < 0) { perror("[HIP] shm_open"); return 1; }
    SharedMemoryBlock* shm = (SharedMemoryBlock*)mmap(
        nullptr, sizeof(SharedMemoryBlock), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) { perror("[HIP] mmap"); return 1; }

    std::cout << "[HIP] Attached. Players: " << numPlayers << "\n";

    HIPContext ctx;
    ctx.shm        = shm;
    ctx.numPlayers = numPlayers;
    ctx.running.store(true);

    // Renderer and Map live on the stack — do NOT heap-allocate or delete them
    Map map(0.0f, 0.0f, 800, 800);
    map.loadScreens({ "../MapsNScreen/FioanaForest_Lvl_tile1.png" });
    Renderer renderer(shm, &map);
    ctx.renderer = &renderer;   // FIX: raw pointer to stack — never call delete on this

    // Wire keypress callback
    // Renderer::run() calls this lambda on each confirmed keypress:
    //   Space      → ACTION_STRIKE / ACTION_USE_WEAPON (if weapon selected)
    //   H          → ACTION_HEAL
    //   Esc        → ACTION_SKIP
    //   W          → ACTION_USE_WEAPON
    //   S          → ACTION_SWAP_IN
    //   Left/Right → renderer cycles enemy targets  (tracks internally)
    //   Up/Down    → renderer cycles weapon slots   (tracks internally)
    renderer.setActionCallback([&ctx](int act, int tgt, int wpn)
    {
        submitAction(&ctx, act, tgt, wpn);
    });

    spawnPlayerThreads(&ctx);

    pthread_t renderTid;
    pthread_create(&renderTid, nullptr, renderThread, &ctx);
    pthread_join(renderTid, nullptr);

    ctx.running.store(false);
    joinAndCleanup(&ctx);

  
    munmap(shm, sizeof(SharedMemoryBlock));
    return 0;
}