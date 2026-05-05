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
#include <time.h>

using std::vector;
using std::cout;
using std::endl;
using std::mutex;
using std::condition_variable;
using std::array;


#define time_of_response 3 // 3 seconds to make move




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

            // ==========================================
            // YOUR CUSTOM ENEMY TYPE LOGIC GOES HERE
            // e.g., int random_type = rand() % 3;
            // shared_block->state.enemies[i].setType(random_type);
            // ==========================================
        }
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




void handle_player_action(const ActionRequest& request, SharedMemoryBlock* shared_block) {
    int attacker_id = shared_block->hip_mailbox.requesting_entity_id;
    int target_id   = shared_block->hip_mailbox.target_id;

    switch (request.action_type)
    {
    case Action::STRIKE:
        int damage = shared_block->state.players[attacker_id].getDemage();

        shared_block->state.enemies[target_id].TakeDamage(damage);

        // check if dead
        if (!shared_block->state.enemies[target_id].isAlive()) {
            shared_block->state.enemies_defeated++;
        }
        shared_block->state.players[attacker_id].ResetStamina();
        break;

    case Action::EXHAUST:
        int damage = shared_block->state.players[attacker_id].getDemage();
        int current_stamina = shared_block->state.enemies[target_id].getStamina();
        shared_block->state.enemies[target_id].setStamina(damage > current_stamina ? 0 : current_stamina - damage);
        shared_block->state.players[attacker_id].ResetStamina();
        break;

    case Action::USE_WEAPON:
        int weapon_id = shared_block->hip_mailbox.weapon_id;
        int weapon_damage = shared_block->state.players[attacker_id].getInventory().getEquippedWeapons().at(weapon_id).getDamage();// assuming this correctly retrieves the weapon damage
        shared_block->state.enemies[target_id].TakeDamage(weapon_damage);

        // check if dead
        if(shared_block->state.enemies[target_id].isAlive()) {
            shared_block->state.enemies_defeated++;
        }

        shared_block->state.players[attacker_id].ResetStamina();
        break;

    case Action::SWAP_IN:
        int weapon_id = shared_block->hip_mailbox.weapon_id;
        shared_block->state.players[attacker_id].swapInFromBackpack(weapon_id); // assuming this correctly swaps the weapon
        shared_block->state.players[attacker_id].ResetStamina();
        break;

    case Action::HEAL:
        int current_hp = shared_block->state.players[attacker_id].getHp();
        // Heal 10% of max HP, but not above max HP
        int heal_amount = shared_block->state.players[attacker_id].getMaxHp() / 10;
        shared_block->state.players[attacker_id].RegainHealth(heal_amount);
        shared_block->state.players[attacker_id].ResetStamina();
        break;

    case Action::SKIP:
        shared_block->state.players[attacker_id].setStamina(shared_block->state.players[attacker_id].getMaxStamina() / 2);
        break;

    case Action::SETUP_GAME:
        // create number of players
        shared_block->state.num_active_players = shared_block->hip_mailbox.target_id;
        for(int i = 0; i < shared_block->state.num_active_players; i++){
            shared_block->state.players[i] = Player(shared_block->hip_mailbox.types[i]);
        }

    default:
        break;
    }
}

void handle_enemy_action(const ActionRequest& request, SharedMemoryBlock* shared_block) {
    int attacker_id = shared_block->asp_mailbox.requesting_entity_id;
    int target_id   = shared_block->asp_mailbox.target_id;

    switch (request.action_type)
    {
    case Action::STRIKE:
        int damage = shared_block->state.enemies[attacker_id].getDemage();

        shared_block->state.players[target_id].TakeDamage(damage);
        shared_block->state.enemies[attacker_id].ResetStamina();
        break;

    case Action::SKIP:
        shared_block->state.enemies[attacker_id].setStamina(shared_block->state.enemies[attacker_id].getMaxStamina() / 2);
        break;

    default:
        break;
    }
}


bool need_more_enemies(const SharedMemoryBlock* shared_block) {

    for (int i = 0; i < shared_block->state.num_active_enemies; ++i) {
        if (shared_block->state.enemies[i].isAlive()) {
            return false; // Found an alive enemy, no need for more
        }
    }
    return true; // All enemies are dead, we need more
}


int main(int argc, char* argv[]) 
{
    // 1. Setup
    unsigned int seed = std::hash<std::string>{}("24I0607");
    srand(seed);
    pthread_t stamina_accumalator, deadlock_detector;

    // 2. Shared Memory
    const char* shm_name = "/game_state_shm";
    shm_unlink(shm_name);
    SharedMem master_shm(shm_name, sizeof(SharedMemoryBlock), true, true);
    SharedMemoryBlock* shared_block = static_cast<SharedMemoryBlock*>(master_shm.getPtr());

    // 3. Sync Primitives
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_block->global_mutex, &mutex_attr);
    pthread_mutex_init(&shared_block->resource_table_mutex, &mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_block->turn_condition, &cond_attr);

    // 4. Initial State
    shared_block->state.game_running = true;
    Arbiter arbiter(shared_block);

    // 5. Processes
    pid_t hip_pid = fork();
    if (hip_pid == 0) { execl("./hip", "./hip", shm_name, nullptr); return 1; }
    pid_t asp_pid = fork();
    if (asp_pid == 0) { execl("./asp", "./asp", shm_name, nullptr); return 1; }

    // --- PHASE 6: BOOTSTRAP (Setup Players & Enemies) ---
    // A. Wait for HIP to define players
    pthread_mutex_lock(&shared_block->global_mutex);
    shared_block->state.current_turn_owner_id = -2;
    pthread_cond_broadcast(&shared_block->turn_condition);
    while (!shared_block->hip_mailbox.is_ready) {
        pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
    }
    handle_player_action(shared_block->hip_mailbox, shared_block);

    // B. Arbiter creates the random enemies
    arbiter.initialize_entities(240607, 7, 7);

    shared_block->hip_mailbox.is_ready = false;
    pthread_mutex_unlock(&shared_block->global_mutex);

    // --- PHASE 7: IGNITE THREADS ---
    // Threads only start after all memory is fully populated
    pthread_create(&stamina_accumalator, NULL, stamina_recovery, shared_block);
    pthread_create(&deadlock_detector, NULL, deadlock_detection, NULL);

    // --- PHASE 8: MAIN EVENT LOOP ---
    while (shared_block->state.game_running) {
        pthread_mutex_lock(&shared_block->global_mutex);

        // --- NORMAL SCHEDULING ---
        int turn_index = -1;
        bool is_player = false;
        arbiter.find_turn(&turn_index, &is_player);

        if (turn_index != -1) {
            shared_block->state.current_turn_owner_id = turn_index;
            shared_block->state.is_player_turn = is_player;
            pthread_cond_broadcast(&shared_block->turn_condition);

            if (is_player) {
                while (!shared_block->hip_mailbox.is_ready) {
                    pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
                }
                handle_player_action(shared_block->hip_mailbox, shared_block);
            } else {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 3;
                int res = 0;
                while (!shared_block->asp_mailbox.is_ready && res != ETIMEDOUT) {
                    res = pthread_cond_timedwait(&shared_block->turn_condition, &shared_block->global_mutex, &ts);
                }

                if (res == ETIMEDOUT && !shared_block->asp_mailbox.is_ready) {
                    shared_block->asp_mailbox.action_type = Action::SKIP;
                    shared_block->asp_mailbox.requesting_entity_id = turn_index;
                }
                handle_enemy_action(shared_block->asp_mailbox, shared_block);
            }

            // Flush mailboxes for the next turn
            shared_block->hip_mailbox.is_ready = false;
            shared_block->asp_mailbox.is_ready = false;

            // Check for endgame conditions
            if (shared_block->state.enemies_defeated >= 10) {
                shared_block->state.game_running = false;
                shared_block->state.game_result = true; // Player wins
                break;
            } else if (shared_block->state.num_active_players == 0) {
                shared_block->state.game_running = false;
                shared_block->state.game_result = false; // Enemy wins
                break;
            }


            if(need_more_enemies(shared_block)) {
                arbiter.initialize_entities(240607, 7, 7);
            }
            shared_block->state.turn_count++;

        }

        pthread_mutex_unlock(&shared_block->global_mutex);
        usleep(10000);
    }

    // --- PHASE 9: CLEANUP ---
    kill(hip_pid, SIGTERM);
    kill(asp_pid, SIGTERM);
    waitpid(hip_pid, NULL, 0);
    waitpid(asp_pid, NULL, 0);
    pthread_cancel(stamina_accumalator);
    pthread_cancel(deadlock_detector);
    pthread_mutex_destroy(&shared_block->global_mutex);
    pthread_mutex_destroy(&shared_block->resource_table_mutex);
    pthread_cond_destroy(&shared_block->turn_condition);

    return 0;
}
