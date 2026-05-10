#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include "../shared/game_state.h"
#include "../resources/shared_mem_abs.h"

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "[PlayerProc] Usage: player_process <playerIdx> <shm_name>\n";
        return 1;
    }

    int         me      = std::atoi(argv[1]);
    const char* shmName = argv[2];

    int fd = shm_open(shmName, O_RDWR, 0666);
    if (fd < 0) { perror("[PlayerProc] shm_open"); return 1; }

    SharedMemoryBlock* shm = (SharedMemoryBlock*)mmap(
        nullptr, sizeof(SharedMemoryBlock),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (shm == MAP_FAILED) { perror("[PlayerProc] mmap"); return 1; }

    std::cout << "[PlayerProc] Player " << me
              << " attached to SHM (PID=" << getpid() << ")\n";

    while (true)
    {
        // 1. Wait until it's MY turn
        pthread_mutex_lock(&shm->global_mutex);
        while (true)
        {
            if (!shm->state.game_running) break;           // ← no stop_flag
            if (shm->state.is_player_turn &&
                shm->state.current_turn_owner_id == me) break;
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        }

        if (!shm->state.game_running)
        {
            pthread_mutex_unlock(&shm->global_mutex);
            std::cout << "[PlayerProc] Player " << me << " exiting\n";
            break;
        }
        pthread_mutex_unlock(&shm->global_mutex);

        // 2. Wait for HIP to post an action into hip_mailbox
        pthread_mutex_lock(&shm->global_mutex);
        while (!shm->hip_mailbox.is_ready)
        {
            if (!shm->state.game_running) break;           // ← no stop_flag
            pthread_cond_wait(&shm->turn_condition, &shm->global_mutex);
        }

        if (!shm->hip_mailbox.is_ready)
        {
            pthread_mutex_unlock(&shm->global_mutex);
            std::cout << "[PlayerProc] Player " << me << " exiting cleanly (Mailbox aborted)\n";
            break;
        }

        // 3. Consume the mailbox
        Action action    = shm->hip_mailbox.action_type;
        int    targetIdx = shm->hip_mailbox.target_id;
        int    weaponIdx = shm->hip_mailbox.weapon_id;
        shm->hip_mailbox.is_ready = false;

        // 4. Signal arbiter — action is ready
        pthread_cond_broadcast(&shm->turn_condition);
        pthread_mutex_unlock(&shm->global_mutex);

        std::cout << "[PlayerProc] Player " << me
                  << " act=" << (int)action
                  << " tgt=" << targetIdx
                  << " wpn=" << weaponIdx << "\n";
    }

    munmap(shm, sizeof(SharedMemoryBlock));
    return 0;
}
