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
#include <SFML/Graphics.hpp>
#include "../shared/game_state.h"
#include "../resources/shared_mem_abs.h"
#include "../DisplayRendering/render.h"
#include "../DisplayRendering/Menu.h"

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
    ActionSlot*        slot;
    HIPContext*        ctx;
};

struct HIPContext
{
    SharedMemoryBlock*           shm;
    Renderer*                    renderer;
    int                          numPlayers;
    int                          running;
    pthread_mutex_t              running_mutex;
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
//  setupThread
// ─────────────────────────────────────────────────────────────────────────────
static void* setupThread(void* args)
{
    HIPContext*        ctx   = (HIPContext*)args;
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
static void waitForAction(ActionSlot* slot, HIPContext* ctx,
                          Action& action, int& targetIdx, int& weaponIdx)
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
    PlayerThreadCtx*   pctx = (PlayerThreadCtx*)arg;
    HIPContext*        ctx  = pctx->ctx;
    SharedMemoryBlock* shm  = ctx->shm;
    const int          me   = pctx->playerIndex;

    while (get_running(ctx))
    {
        // 1. Wait until it is MY turn
        pthread_mutex_lock(&shm->global_mutex);
        while (get_running(ctx))
        {
            if (shm->state.is_player_turn &&
                shm->state.current_turn_owner_id == me) break;
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
        int    targetIdx, weaponIdx;
        waitForAction(pctx->slot, ctx, action, targetIdx, weaponIdx);
        if (!get_running(ctx)) break;

        // 4. Post to hip_mailbox and wake Arbiter
        pthread_mutex_lock(&shm->global_mutex);
        shm->hip_mailbox.requesting_entity_id = me;
        shm->hip_mailbox.action_type          = action;
        shm->hip_mailbox.target_id            = targetIdx;
        shm->hip_mailbox.weapon_id            = weaponIdx;
        shm->hip_mailbox.is_ready             = true;
        pushLog(shm, "Player %d act=%d tgt=%d wpn=%d",
                me, static_cast<int>(action), targetIdx, weaponIdx);
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
        slot.ready     = false;
        slot.action    = Action::SKIP;
        slot.targetIdx = -1;
        slot.weaponIdx = -1;

        PlayerThreadCtx& pctx = ctx->playerCtxs[i];
        pctx.playerIndex = i;
        pctx.shm         = ctx->shm;
        pctx.slot        = &slot;
        pctx.ctx         = ctx;

        int rc = pthread_create(&ctx->playerTids[i], nullptr,
                                playerThreadFunc, &pctx);
        if (rc != 0)
            std::cerr << "[HIP] pthread_create player " << i
                      << " failed: " << strerror(rc) << "\n";
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
    ctx->renderer->run();                          // blocks until window closes

    set_running(ctx, 0);

    // wake any player thread stuck waiting on its slot
    for (int i = 0; i < ctx->numPlayers; i++)
        pthread_cond_broadcast(&ctx->slots[i].cond);

    // wake any player thread stuck waiting on its turn
    pthread_mutex_lock(&ctx->shm->global_mutex);
    pthread_cond_broadcast(&ctx->shm->turn_condition);
    pthread_mutex_unlock(&ctx->shm->global_mutex);

    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  joinAndCleanup
// ─────────────────────────────────────────────────────────────────────────────
static void joinAndCleanup(HIPContext* ctx)
{
    for (int i = 0; i < ctx->numPlayers; i++)
    {
        pthread_join(ctx->playerTids[i], nullptr);
        pthread_mutex_destroy(&ctx->slots[i].mutex);
        pthread_cond_destroy (&ctx->slots[i].cond);
    }
    std::cout << "[HIP] All player threads joined\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  typeName — readable PlayerType for terminal verification
// ─────────────────────────────────────────────────────────────────────────────
static const char* typeName(PlayerType t)
{
    switch (t)
    {
        case PlayerType::CHRONO: return "CHRONO";
        case PlayerType::FROG:   return "FROG";
        case PlayerType::MARLE:  return "MARLE";
        case PlayerType::MAGUS:  return "MAGUS";
        default:                 return "NONE";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    if (argc < 2) { std::cerr << "[HIP] Usage: hip <shm_name>\n"; return 1; }

    const char* shmName = argv[1];

    // ── PHASE 1: Attach shared memory ────────────────────────────────────────
    int fd = shm_open(shmName, O_RDWR, 0666);
    if (fd < 0) { perror("[HIP] shm_open"); return 1; }

    SharedMemoryBlock* shm = (SharedMemoryBlock*)mmap(
        nullptr, sizeof(SharedMemoryBlock),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) { perror("[HIP] mmap"); return 1; }

    std::cout << "[HIP] Attached to shared memory\n";

    // ── PHASE 2: Run menu — blocks until user confirms party ─────────────────
    sf::RenderWindow menuWindow(
        sf::VideoMode((unsigned)MENU_WIN_W, (unsigned)MENU_WIN_H),
        "Chrono Rift");

    GameMenu menu(menuWindow,
                  "../MapsNScreen/MenuScreen.jpg",
                  "../MapsNScreen/Map_Overlay.png");

    PartyConfig party = menu.run();   // blocks until DONE or window closed

    if (!party.valid())
    {
        std::cout << "[HIP] No party selected — exiting\n";
        munmap(shm, sizeof(SharedMemoryBlock));
        return 0;
    }

    // menuWindow destructs here — SFML closes it before renderer opens

    // ── Terminal verification ─────────────────────────────────────────────────
    std::cout << "[HIP] Party confirmed: " << party.numPlayers() << " players\n";
    for (int i = 0; i < party.numPlayers(); i++)
        std::cout << "  Player " << i << " = " << typeName(party.players[i]) << "\n";
    std::cout << "  Level selected = " << party.selectedLevel << "\n";

    // ── PHASE 3: Build HIPContext from party ──────────────────────────────────
    HIPContext ctx;
    ctx.shm        = shm;
    ctx.renderer   = nullptr;
    ctx.numPlayers = party.numPlayers();
    ctx.running    = 1;
    pthread_mutex_init(&ctx.running_mutex, nullptr);

    for (int i = 0; i < ctx.numPlayers; i++)
        ctx.playerTypes[i] = party.players[i];

    // ── PHASE 4: Handshake with Arbiter ──────────────────────────────────────
    pthread_t setupTid;
    pthread_create(&setupTid, nullptr, setupThread, &ctx);
    pthread_join(setupTid, nullptr);
    std::cout << "[HIP] Setup complete — Arbiter acknowledged\n";

    // ── PHASE 5: Open game window + spawn player threads ─────────────────────
    Map map(0.0f, 0.0f, 800, 800);
    map.loadScreens({ "../MapsNScreen/Fiaona'aForest_Lvl_tile1.png" });

    Renderer renderer(shm, &map);
    ctx.renderer = &renderer;

    renderer.setActionCallback([&ctx](Action act, int tgt, int wpn)
    {
        submitAction(&ctx, act, tgt, wpn);
    });

    spawnPlayerThreads(&ctx);

    pthread_t renderTid;
    pthread_create(&renderTid, nullptr, renderThread, &ctx);
    pthread_join(renderTid, nullptr);   // blocks until window closes

    // ── PHASE 6: Cleanup ──────────────────────────────────────────────────────
    joinAndCleanup(&ctx);
    pthread_mutex_destroy(&ctx.running_mutex);
    munmap(shm, sizeof(SharedMemoryBlock));

    std::cout << "[HIP] Clean exit\n";
    return 0;
}
