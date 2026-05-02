#include <pthread.h>
#include <semaphore>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <array>
#include <queue>
#include <signal.h>
#include <iostream>
#include "../resources/shared_mem_abs.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include "../Characters/Player.h"
#include "../Characters/Enemy.h"
#include "../Weapons/Weapons.h"
#include <fcntl.h>      // for shm_open
#include <sys/mman.h>   // for mmap
#include <cstring>      // for strerror
#include "../shared/shared_types.h"

using std::vector;
using std::cout;
using std::endl;
using std::mutex;
using std::condition_variable;
using std::array;




class Arbiter {
private:
    // The Arbiter only holds a pointer to the Master Block. No ghost data!
    SharedMemoryBlock* shared_block;

public:
    // Bind the Arbiter to the shared memory upon creation
    Arbiter(SharedMemoryBlock* block)
     {
        this->shared_block = block;
    }

    // --- 1. Initialization (Run before forking children) ---
    void initialize_entities(int seed_roll_full, int seed_last_dig, int seed_last_two) {
        // Randomly set 2 to 9 enemies
        shared_block->state.num_active_enemies = (rand() % 8) + 2;

        for (int i = 0; i < shared_block->state.num_active_enemies; ++i) {
            shared_block->state.enemies[i].setRollNumber(seed_roll_full, seed_last_dig, seed_last_two);
            shared_block->state.enemies[i].initRollStats();
            shared_block->state.enemies[i].setAlive(true);
            shared_block->state.enemies[i].clearStun();
            shared_block->state.enemies[i].ResetStamina();
        }

        // TODO: Do the same for players based on user input
    }

    // --- 2. The Scheduler Logic ---
    void find_turn(int* out_turn_index, bool* out_turn) {
        *out_turn_index = -1; // Default to no one ready

        bool check_players_first = rand() % 2 == 0;
        int num_players = shared_block->state.num_active_players;
        int num_enemies = shared_block->state.num_active_enemies;

        if(check_players_first) {
            for(int i = 0; i < num_players; ++i) {
                // Skip dead or stunned players
                if (!shared_block->state.players[i].isAlive() || shared_block->state.players[i].isStunned()) continue;

                if(shared_block->state.players[i].getStamina() >= shared_block->state.players[i].getMaxStamina()) {
                    *out_turn_index = i;
                    *out_turn = true;
                    return;
                }
            }
            for(int i = 0; i < num_enemies; ++i) {
                if (!shared_block->state.enemies[i].isAlive() || shared_block->state.enemies[i].isStunned()) continue;

                if(shared_block->state.enemies[i].getStamina() >= shared_block->state.enemies[i].getMaxStamina()) {
                    *out_turn_index = num_players + i;
                    *out_turn = false;
                    return;
                }
            }
        } else {
            // Check enemies first
            for(int i = 0; i < num_enemies; ++i) {
                if (!shared_block->state.enemies[i].isAlive() || shared_block->state.enemies[i].isStunned()) continue;

                if(shared_block->state.enemies[i].getStamina() >= shared_block->state.enemies[i].getMaxStamina()) {
                    *out_turn_index = num_players + i;
                    *out_turn = false;
                    return;
                }
            }
            for(int i = 0; i < num_players; ++i) {
                if (!shared_block->state.players[i].isAlive() || shared_block->state.players[i].isStunned()) continue;

                if(shared_block->state.players[i].getStamina() >= shared_block->state.players[i].getMaxStamina()) {
                    *out_turn_index = i;
                    *out_turn = true;
                    return;
                }
            }
        }
    }

    // --- 3. Combat & Mechanic Handlers ---
    void apply_stun_to_enemy(int enemy_index, int duration_seconds, pid_t asp_pid) {
        std::cout << "[ARBITER] Applying stun to Enemy " << enemy_index << " for " << duration_seconds << " seconds." << std::endl;

        // Update the timestamp in shared memory
        shared_block->state.enemies[enemy_index].applyStun(duration_seconds);

        // Fire the asynchronous interrupt to the ASP
        kill(asp_pid, SIGUSR1);
    }
};

// 1. Updated Stamina Thread
void* stamina_recovery(void* arg){
    // Cast to the Master Block, not GameState
    auto* shared_block = static_cast<SharedMemoryBlock*>(arg);

    while(true){
        // Lock the global mutex from the block
        pthread_mutex_lock(&shared_block->global_mutex);

        for (int i = 0; i < shared_block->state.num_active_players; ++i) {
            auto& player = shared_block->state.players[i];
            if (player.isAlive()) {
                player.setStamina(player.getStamina() + player.getStaminaRecoveryRate());
            }
        }
        for (int i = 0; i < shared_block->state.num_active_enemies; ++i) {
            auto& enemy = shared_block->state.enemies[i];
            if (enemy.isAlive()) {
                enemy.setStamina(enemy.getStamina() + enemy.getStaminaRecoveryRate());
            }
        }

        pthread_mutex_unlock(&shared_block->global_mutex);
        usleep(100000); // 100ms
    }
    return nullptr;
}
void* deadlock_detection(void* arg)
{
    while(true){
        // Check for deadlocks and resolve them
        sleep(5); // Sleep for 5 seconds before checking again
    }
    return nullptr;
}


// pick the first player/enemy that has full stamina, if no one has full stamina, return nullptr
// randomly chooses what to find in first, player or enemy
void find_turn(std::array<Player, 4>* players_arr, int num_active_players,
               std::array<Enemy, 9>* enemies_arr, int num_active_enemies,
               int* out_turn_index, bool* out_turn){

    *out_turn_index = -1;
    bool check_players_first = rand() % 2 == 0;
    if(check_players_first){
        for(int i = 0; i < num_active_players; ++i){
            if((*players_arr)[i].getStamina() >= (*players_arr)[i].getMaxStamina()){
                *out_turn_index = i;
                *out_turn = true;
                return;
            }
        }
        for(int i = 0; i < num_active_enemies; ++i){
            if((*enemies_arr)[i].getStamina() >= (*enemies_arr)[i].getMaxStamina()){
                *out_turn_index = num_active_players + i;
                *out_turn = false;
                return;
            }
        }
    }

    else {
        for(int i = 0; i < num_active_enemies; ++i){
            if((*enemies_arr)[i].getStamina() >= (*enemies_arr)[i].getMaxStamina()){
                *out_turn_index = num_active_players + i;
                *out_turn = false;
                return;
            }
        }
         for(int i = 0; i < num_active_players; ++i){
            if((*players_arr)[i].getStamina() >= (*players_arr)[i].getMaxStamina()){
                *out_turn_index = i;
                *out_turn = true;
                return;
            }
        }
    }

}


int main(int argc, char* argv[]) {
    // 1. Seed the Random Number Generator
    unsigned int seed = std::hash<std::string>{}("24I0607");
    srand(seed);

    pthread_t stamina_accumalator, deadlock_detector;

    // 2. --- Create Shared Memory for the Master Block ---
    const char* shm_name = "/game_state_shm";
    shm_unlink(shm_name); // Clean up any zombie memory from previous crashes

    // Instantiate your SharedMem wrapper class to handle creation and mapping securely
    SharedMem master_shm(shm_name, sizeof(SharedMemoryBlock), true, true);

    // Cast the raw pointer directly to our SharedMemoryBlock
    SharedMemoryBlock* shared_block = static_cast<SharedMemoryBlock*>(master_shm.getPtr());

    // 3. --- Initialize the Process-Shared Mutexes and CVs ---
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&shared_block->global_mutex, &mutex_attr);
    pthread_mutex_init(&shared_block->resource_table_mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_block->turn_condition, &cond_attr);
    pthread_condattr_destroy(&cond_attr);

    // 4. --- Setup Initial Game State & Arbiter ---
    shared_block->state.game_running = true;
    shared_block->state.level = 1;
    shared_block->state.sublevel = 1;
    shared_block->state.enemies_defeated = 0;
    shared_block->state.current_turn_owner_id = -1;
    shared_block->hip_mailbox.is_ready = false;
    shared_block->asp_mailbox.is_ready = false;

    // Instantiate Arbiter and initialize the first level's entities
    Arbiter arbiter(shared_block);
    arbiter.initialize_entities(240607, 7, 7);

    // 5. --- Fork Child Processes ---
    pid_t hip_pid = fork();
    if (hip_pid == 0) 
    {
        execl("./hip", "./hip", shm_name, nullptr);
        std::cerr << "Failed to exec HIP process" << std::endl;
        return 1;
    }

    pid_t asp_pid = fork();
    if (asp_pid == 0) {
        execl("./asp", "./asp", shm_name, nullptr);
        std::cerr << "Failed to exec ASP process" << std::endl;
        return 1;
    }

    // 6. --- Ignite Background Threads ---
    pthread_create(&stamina_accumalator, NULL, stamina_recovery, shared_block);
    pthread_create(&deadlock_detector, NULL, deadlock_detection, NULL);

    std::cout << "[ARBITER] System Kernel Online. Commencing Simulation." << std::endl;

    // 7. --- The Main Arbiter Event Loop ---
    while (shared_block->state.game_running) {
        pthread_mutex_lock(&shared_block->global_mutex);

        int turn_index = -1;
        bool is_player = false;

        // Step A: Find who goes next
        arbiter.find_turn(&turn_index, &is_player);

        if (turn_index != -1) {
            // Step B: Assign the turn
            shared_block->state.current_turn_owner_id = turn_index;
            shared_block->state.is_player_turn = is_player;

            // Step C: Flush mailboxes and wake up children
            shared_block->hip_mailbox.is_ready = false;
            shared_block->asp_mailbox.is_ready = false;
            pthread_cond_broadcast(&shared_block->turn_condition);

            // Step D: Wait for response and execute
            if (is_player) {
                while (!shared_block->hip_mailbox.is_ready) {
                    pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
                }

                std::cout << "[ARBITER] Player " << turn_index << " executed action!" << std::endl;
                // TODO: HIP Action Math
                shared_block->state.players[turn_index].setStamina(0);

            } else {
                while (!shared_block->asp_mailbox.is_ready) {
                    pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
                }

                int enemy_actual_index = turn_index - shared_block->state.num_active_players;
                std::cout << "[ARBITER] Enemy " << enemy_actual_index << " executed action!" << std::endl;
                // TODO: ASP Action Math
                shared_block->state.enemies[enemy_actual_index].setStamina(0);
            }

            // Step E: Check win/loss/level-up conditions here!
        }

        pthread_mutex_unlock(&shared_block->global_mutex);
        usleep(10000);
    }

    // 8. --- Graceful Teardown ---
    std::cout << "[ARBITER] Simulation Terminated. Cleaning up resources..." << std::endl;

    kill(hip_pid, SIGTERM);
    kill(asp_pid, SIGTERM);
    waitpid(hip_pid, NULL, 0);
    waitpid(asp_pid, NULL, 0);

    pthread_cancel(stamina_accumalator);
    pthread_cancel(deadlock_detector);

    // Unmap and unlink memory
    pthread_mutex_destroy(&shared_block->global_mutex);
    pthread_mutex_destroy(&shared_block->resource_table_mutex);
    pthread_cond_destroy(&shared_block->turn_condition);

    // Note: No need for munmap or shm_unlink here because the master_shm destructor handles it automatically.

    return 0;
}