#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
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

struct HIPContext
{
    SharedMemoryBlock*        shm;
    Renderer*                 renderer;
    int                       numPlayers;
    int                       running;
    pthread_mutex_t           running_mutex;
    std::array<PlayerType, 4> playerTypes;
    std::vector<pid_t>        playerPids;
    char                      shmName[64];
};

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

static void submitAction(HIPContext* ctx, Action action, int targetIdx, int weaponIdx = -1)
{
    pthread_mutex_lock(&ctx->shm->global_mutex);

    bool isPlayerTurn = ctx->shm->state.is_player_turn;
    int  active       = ctx->shm->state.current_turn_owner_id;

    if (!isPlayerTurn || active < 0 || active >= ctx->numPlayers)
    {
        pthread_mutex_unlock(&ctx->shm->global_mutex);
        return;
    }

    ctx->shm->hip_mailbox.requesting_entity_id = active;
    ctx->shm->hip_mailbox.action_type          = action;
    ctx->shm->hip_mailbox.target_id            = targetIdx;
    ctx->shm->hip_mailbox.weapon_id            = weaponIdx;
    ctx->shm->hip_mailbox.is_ready             = true;

    pushLog(ctx->shm, "Player %d act=%d tgt=%d wpn=%d",
            active, (int)action, targetIdx, weaponIdx);

    pthread_cond_broadcast(&ctx->shm->turn_condition);
    pthread_mutex_unlock(&ctx->shm->global_mutex);
}

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

static void spawnPlayerProcesses(HIPContext* ctx)
{
    ctx->playerPids.resize(ctx->numPlayers);

    for (int i = 0; i < ctx->numPlayers; i++)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            char idxStr[16];
            snprintf(idxStr, sizeof(idxStr), "%d", i);

            execl("./player_process",
                  "player_process",
                  idxStr,
                  ctx->shmName,
                  nullptr);

            perror("[HIP] execl failed");
            exit(1);
        }

        ctx->playerPids[i] = pid;
        std::cout << "[HIP] Player process " << i
                  << " exec'd (PID=" << pid << ")\n";
    }
}

static void joinAndCleanup(HIPContext* ctx)
{
    for (int i = 0; i < ctx->numPlayers; i++)
    {
        if (ctx->playerPids[i] > 0)
        {
            kill(ctx->playerPids[i], SIGTERM);
            waitpid(ctx->playerPids[i], nullptr, 0);
            std::cout << "[HIP] Player process " << i << " joined\n";
        }
    }
}

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

int main(int argc, char* argv[])
{
    if (argc < 2) { std::cerr << "[HIP] Usage: hip <shm_name>\n"; return 1; }

    int fd = shm_open(argv[1], O_RDWR, 0666);
    SharedMemoryBlock* shm = (SharedMemoryBlock*)mmap(
        nullptr, sizeof(SharedMemoryBlock),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    sf::RenderWindow window(
        sf::VideoMode((unsigned)WIN_W, (unsigned)WIN_H),
        "Chrono Rift", sf::Style::Titlebar | sf::Style::Close);

    HIPContext ctx;
    ctx.shm        = shm;
    ctx.running    = 1;
    ctx.numPlayers = 0;
    strncpy(ctx.shmName, argv[1], sizeof(ctx.shmName) - 1);
    ctx.shmName[sizeof(ctx.shmName) - 1] = '\0';
    pthread_mutex_init(&ctx.running_mutex, nullptr);

    Map map(0.0f, 0.0f, 860, 800);
    map.loadScreens({ "../MapsNScreen/Fiaona'aForest_Lvl_tile1.png" });

    Renderer renderer(shm, &map, &window);
    ctx.renderer = &renderer;

    renderer.setActionCallback([&ctx](Action act, int tgt, int wpn)
    {
        submitAction(&ctx, act, tgt, wpn);
    });

    renderer.setPartyReadyCallback([&ctx](const PartyConfig& party)
    {
        ctx.numPlayers = party.numPlayers();
        for (int i = 0; i < ctx.numPlayers; i++)
            ctx.playerTypes[i] = party.players[i];

        pthread_t setupTid;
        pthread_create(&setupTid, nullptr, setupThread, &ctx);
        pthread_join(setupTid, nullptr);
        std::cout << "[HIP] Setup complete\n";

        spawnPlayerProcesses(&ctx);
    });

    renderer.run();

    // ── Cleanup ───────────────────────────────────────────────────────────
    set_running(&ctx, 0);

    pthread_mutex_lock(&shm->global_mutex);
    shm->state.game_running = false;          // ← Option B: no stop_flag needed
    pthread_cond_broadcast(&shm->turn_condition);
    pthread_mutex_unlock(&shm->global_mutex);

    joinAndCleanup(&ctx);
    pthread_mutex_destroy(&ctx.running_mutex);
    munmap(shm, sizeof(SharedMemoryBlock));
    std::cout << "[HIP] Clean exit\n";

    kill(getppid(), SIGTERM);
    return 0;
}
