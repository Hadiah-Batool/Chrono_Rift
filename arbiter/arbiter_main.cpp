#include "arbiter_utils.h"

// ─────────────────────────────────────────────────────────────────────────────
//  GLOBALS DEFINITION
// ─────────────────────────────────────────────────────────────────────────────
pid_t g_asp_pid = -1;
SharedMemoryBlock* g_shm_ptr = nullptr;
volatile sig_atomic_t g_sigalrm_received = 0;
volatile sig_atomic_t g_sigterm_received = 0;
bool g_ultimate_active = false; // <--- Instantiating the Ultimate Flag

static void handle_sigalrm(int sig) { g_sigalrm_received = 1; }
static void handle_sigterm(int sig) {
    g_sigterm_received = 1;
    // Naked broadcast to wake the Arbiter if it is stuck waiting for a handshake
    if (g_shm_ptr) {
        pthread_cond_broadcast(&g_shm_ptr->turn_condition);
    }
}

inline bool need_more_enemies(const SharedMemoryBlock* shared_block) {
    for (int i = 0; i < shared_block->state.num_active_enemies; ++i) {
        if (shared_block->state.enemies[i].isAlive()) return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    unsigned int seed = std::hash<std::string>{}("24I0805");
    srand(seed);
    pthread_t stamina_accumalator, deadlock_detector;
    bool threads_started = false; // Safety flag for graceful shutdown

    struct sigaction sa_alrm{};
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_handler = handle_sigalrm;
    sigaction(SIGALRM, &sa_alrm, nullptr);

    struct sigaction sa_term{};
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa_term, nullptr);

    const char* shm_name = "/game_state_shm";
    shm_unlink(shm_name);
    SharedMem master_shm(shm_name, sizeof(SharedMemoryBlock), true, true);
    SharedMemoryBlock* shared_block = static_cast<SharedMemoryBlock*>(master_shm.getPtr());

    g_shm_ptr = shared_block;

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_block->global_mutex, &mutex_attr);
    pthread_mutex_init(&shared_block->resource_table_mutex, &mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_block->turn_condition, &cond_attr);

    for (int i = 0; i < 4; i++) {
        shared_block->state.players_artifact_state[i].waiting_for_artifact_idx = -1;
        shared_block->state.players_artifact_state[i].holding_artifact_idx = -1;
    }
    for (int i = 0; i < 9; i++) {
        shared_block->state.enemies_artifact_state[i].waiting_for_artifact_idx = -1;
        shared_block->state.enemies_artifact_state[i].holding_artifact_idx = -1;
    }

    shared_block->state.game_running = true;
    shared_block->state.level = 1;
    shared_block->state.sublevel = 1;
    shared_block->state.enemies_defeated = 0;
    Arbiter arbiter(shared_block);

    pid_t hip_pid = fork();
    if (hip_pid == 0) { execl("./hip.out", "./hip.out", shm_name, nullptr);
        std::cout<<"Could not launch HIP process. Make sure hip.out is compiled and in the same directory.\n";
        return 1;
     }
    pid_t asp_pid = fork();
    if (asp_pid == 0) { execl("./asp.out", "./asp.out", shm_name, nullptr);
        std::cout<<"Could not launch ASP process. Make sure asp.out is compiled and in the same directory.\n";
        return 1;
    }

    g_asp_pid = asp_pid;

    pthread_mutex_lock(&shared_block->global_mutex);
    shared_block->state.current_turn_owner_id = -2;
    pthread_cond_broadcast(&shared_block->turn_condition);

    // Wait for the HIP menu to complete
    while (!shared_block->hip_mailbox.is_ready && !g_sigterm_received) {
        pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
    }

    // --- CLEAN IF/ELSE BLOCK FOR STARTUP ---
    if (g_sigterm_received) {
        std::cout << "\n[ARBITER] Setup aborted. Menu closed before game start.\n";
        shared_block->state.game_running = false;
        pthread_mutex_unlock(&shared_block->global_mutex);
    } else {
        // Proceed with game initialization
        handle_player_action(shared_block->hip_mailbox, shared_block);

        arbiter.initialize_entities(0607, 7, 7, shared_block->state.level, shared_block->state.sublevel);
        arbiter.initialize_players_positions(shared_block->state.level, shared_block->state.sublevel);

        shared_block->hip_mailbox.is_ready = false;
        pthread_mutex_unlock(&shared_block->global_mutex);

        // ── ARTIFACT DROP SCHEDULE ────────────────────────────────────────────────
        shared_block->state.num_artifacts = 3;

        new (&shared_block->state.artifacts[0]) Artifact(0, ArtifactType::ECLIPSE_RELIC, "Solar Core",    95, 10);
        new (&shared_block->state.artifacts[1]) Artifact(1, ArtifactType::ECLIPSE_RELIC, "Lunar Blade",   90, 10);
        new (&shared_block->state.artifacts[2]) Artifact(2, ArtifactType::ECLIPSE_RELIC, "Eclipse Relic",  0,  5);

        int artifact_drop_turn[3];
        artifact_drop_turn[0] = 10 + (rand() % 2);
        artifact_drop_turn[1] = artifact_drop_turn[0] + 1 + (rand() % 2);
        artifact_drop_turn[2] = artifact_drop_turn[1] + 1 + (rand() % 2);
        bool artifact_dropped[3] = { false, false, false };

        std::cout << "[ARBITER] Artifact drop schedule — "
                  << "Solar Core: turn "    << artifact_drop_turn[0]
                  << ", Lunar Blade: turn " << artifact_drop_turn[1]
                  << ", Eclipse Relic: turn " << artifact_drop_turn[2] << "\n";

        pthread_create(&stamina_accumalator, NULL, stamina_recovery, shared_block);
        pthread_create(&deadlock_detector, NULL, deadlock_detection, shared_block);
        threads_started = true; // Mark threads as active so we know to cancel them later

        // ── MAIN GAME LOOP ────────────────────────────────────────────────────────
        while (shared_block->state.game_running) {
            pthread_mutex_lock(&shared_block->global_mutex);
            int turn_index = -1;
            bool is_player = false;
            arbiter.find_turn(&turn_index, &is_player);

            if (turn_index != -1) {
                shared_block->state.current_turn_owner_id = turn_index;
                shared_block->state.is_player_turn = is_player;
                pthread_cond_broadcast(&shared_block->turn_condition);

                if (is_player) {
                    while (!shared_block->hip_mailbox.is_ready && shared_block->state.game_running && !g_sigterm_received) {
                        pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
                    }

                    if (g_sigterm_received) {
                        pthread_mutex_unlock(&shared_block->global_mutex);
                        break;
                    }

                    if (shared_block->state.game_running) {
                        handle_player_action(shared_block->hip_mailbox, shared_block);
                    }
                } else {
                    struct timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_sec += 3;
                    int res = 0;

                    while (!shared_block->asp_mailbox.is_ready && res != ETIMEDOUT && shared_block->state.game_running && !g_sigterm_received) {
                        res = pthread_cond_timedwait(&shared_block->turn_condition, &shared_block->global_mutex, &ts);
                    }

                    if (g_sigterm_received) {
                        pthread_mutex_unlock(&shared_block->global_mutex);
                        break;
                    }

                    if (shared_block->state.game_running) {
                        if (res == ETIMEDOUT && !shared_block->asp_mailbox.is_ready) {
                            shared_block->asp_mailbox.action_type = Action::SKIP;
                            shared_block->asp_mailbox.requesting_entity_id = turn_index;
                        }
                        handle_enemy_action(shared_block->asp_mailbox, shared_block);
                    }
                }

                shared_block->hip_mailbox.is_ready = false;
                shared_block->asp_mailbox.is_ready = false;

                if (shared_block->state.enemies_defeated >= 10) {
                    std::cout << "[ARBITER] 10 Enemies Slain. Objective Complete. YOU WIN!\n";
                    shared_block->state.game_running = false;
                    shared_block->state.game_result = true;
                    pthread_mutex_unlock(&shared_block->global_mutex);
                    break;
                } else if (shared_block->state.num_active_players == 0) {
                    std::cout << "[ARBITER] All players have fallen. YOU LOSE!\n";
                    shared_block->state.game_running = false;
                    shared_block->state.game_result = false;
                    pthread_mutex_unlock(&shared_block->global_mutex);
                    break;
                }
                if(shared_block->state.game_running && need_more_enemies(shared_block)) {

                    // --- NEW LOGIC: Check if Sublevel 2 just ended ---
                    if (shared_block->state.sublevel == 2) {
                        std::cout << "\n[ARBITER] Sublevel 2 cleared! Demo Complete. YOU WIN!\n";
                        shared_block->state.game_running = false;
                        shared_block->state.game_result = true;
                        pthread_mutex_unlock(&shared_block->global_mutex);
                        break;
                    }
                    // -------------------------------------------------

                    shared_block->state.hassublevelended = true;
                    shared_block->state.sublevel++;
                    std::cout << "[ARBITER] Wave cleared! Loading Sublevel " << shared_block->state.sublevel << "...\n";

                    std::cout << "[ARBITER] Player 0 is advancing to the next zone...\n";
                    try {
                        // Load the coordinates from the text file
                        shared_block->state.players[0].getPath(shared_block->state.level, shared_block->state.sublevel);

                        bool completed_section = true;
                        bool path_finished = false;

                        // Loop until the movement function returns true (path complete)
                        while (!path_finished && shared_block->state.game_running && !g_sigterm_received) {
                            // Only lock around the actual state modification
                            pthread_mutex_lock(&shared_block->global_mutex);
                            path_finished = shared_block->state.players[0].movement(completed_section);
                            pthread_mutex_unlock(&shared_block->global_mutex);

                            // Sleep outside the lock so renderer can read positions
                            usleep(16000);
                        }
                        std::cout << "[ARBITER] Player 0 reached the new battle position!\n";
                    } catch (const std::exception& e) {
                        std::cerr << "[ARBITER] Movement file warning: " << e.what() << " (Skipping walk)\n";
                    }

                    arbiter.initialize_entities(0607, 7, 7, shared_block->state.level, shared_block->state.sublevel);

                    for(int i = 0; i < shared_block->state.num_active_players; ++i) shared_block->state.players[i].setStamina(0);
                    kill(asp_pid, SIGUSR1);
                    shared_block->state.current_turn_owner_id = -3;
                    pthread_cond_broadcast(&shared_block->turn_condition);
                    shared_block->state.hassublevelended = false;
                    std::cout << "[ARBITER] HIP rendering complete. Resuming combat!\n";
                    usleep(50000);
                }
                shared_block->state.turn_count++;

                // ── ARTIFACT DROP CHECK ───────────────────────────────────────────────────
                {
                    struct { ArtifactType type; const char* name; int dmg; int slots; } schedule[3] = {
                        { ArtifactType::SOLAR_CORE,    "Solar Core",    95, 10 },
                        { ArtifactType::LUNAR_BLADE,   "Lunar Blade",   90, 10 },
                        { ArtifactType::ECLIPSE_RELIC, "Eclipse Relic",  0,  5 },
                    };
                    for (int a = 0; a < 3; ++a) {
                        if (!artifact_dropped[a] &&
                            shared_block->state.turn_count >= artifact_drop_turn[a])
                        {
                            artifact_dropped[a] = true;
                            pthread_mutex_lock(&shared_block->resource_table_mutex);
                            // Re-construct in-place as the real type
                            new (&shared_block->state.artifacts[a]) Artifact(
                                a, schedule[a].type, schedule[a].name,
                                schedule[a].dmg, schedule[a].slots);
                            // Reveal Eclipse Relic
                            if (schedule[a].type == ArtifactType::ECLIPSE_RELIC)
                                shared_block->state.artifacts[a].introduce();
                            pthread_mutex_unlock(&shared_block->resource_table_mutex);

                            std::cout << "\n[ARBITER] *** " << schedule[a].name
                                      << " has appeared on turn " << shared_block->state.turn_count
                                      << "! Use GET_ARTIFACT to claim it. ***\n\n";
                        }
                    }
                }

                shared_block->state.current_turn_owner_id = -1;
            }
            pthread_mutex_unlock(&shared_block->global_mutex);

            if (g_sigalrm_received) {
                g_sigalrm_received = 0;
                g_ultimate_active = false; // <--- Restore Enemy Time!

                if (g_asp_pid > 0) {
                    std::cout << "\n[ARBITER] *** 10 SECONDS PASSED! ULTIMATE ABILITY ENDED! ***\n";
                    std::cout << "[ARBITER] *** Sending SIGCONT to ASP. Time resumes for enemies! ***\n> ";
                    std::cout.flush();
                    kill(g_asp_pid, SIGCONT);
                }
            }
            if (g_sigterm_received) {
                g_sigterm_received = 0;
                pthread_mutex_lock(&shared_block->global_mutex);
                std::cout << "\n[ARBITER] SIGTERM received! Commencing graceful shutdown...\n";
                shared_block->state.game_running = false;
                pthread_cond_broadcast(&shared_block->turn_condition);
                pthread_mutex_unlock(&shared_block->global_mutex);
            }

            usleep(10000);
        }
    }

    // ── CLEAN EXIT PROTOCOL ───────────────────────────────────────────────────
    std::cout << "\n[ARBITER] Sending SIGTERM to child processes...\n";
    kill(hip_pid, SIGTERM);
    kill(asp_pid, SIGTERM);

    std::cout << "[ARBITER] Waiting for HIP to close...\n";
    waitpid(hip_pid, NULL, 0);

    std::cout << "[ARBITER] HIP closed. Waiting for ASP to close...\n";
    waitpid(asp_pid, NULL, 0);

    // Only attempt to cancel background threads if they were actually created
    if (threads_started) {
        std::cout << "[ARBITER] ASP closed. Canceling background threads...\n";
        pthread_cancel(stamina_accumalator);
        pthread_cancel(deadlock_detector);
        pthread_join(stamina_accumalator, NULL);
        pthread_join(deadlock_detector, NULL);
    }

    std::cout << "[ARBITER] Destroying Mutexes. Game successfully terminated.\n";
    pthread_mutex_destroy(&shared_block->global_mutex);
    pthread_mutex_destroy(&shared_block->resource_table_mutex);
    pthread_cond_destroy(&shared_block->turn_condition);

    return 0;
}
