#include <pthread.h>
#include <semaphore>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <signal.h>
#include <iostream>
#include "../resources/shared_mem_abs.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include "../Characters/Player.h"
#include <fcntl.h>      // for shm_open
#include <sys/mman.h>   // for mmap
#include <cstring>      // for strerror

using std::vector;
using std::cout;
using std::endl;
using std::mutex;
using std::condition_variable;


struct Stamina {
    float current_stamina;
    float max_stamina;
    float recovery_rate; // stamina points recovered per second
};

struct Thread_Player{
    int thread_id;
    bool is_player; // checks if player or enemy
    bool turn; // checks if it's the player's turn
    SharedMemPipe* shared_mem; // pointer to shared memory that acts as a pipe
    Stamina stamina; // stamina struct for the player or enemy
};

struct GameState{
    pthread_mutex_t global_mutex; // mutex for synchronizing access to the game state
    pthread_mutex_t resource_table_mutex; // for artifacts
    pthread_cond_t turn_condition; // for syncing turns


    bool game_running;
    bool game_result; // true if player wins, false if enemy wins

    int turn_count; // counts the number of turns taken

    vector<Player> players;
    vector<Artifact> artifacts;
    vector<Enemy> enemies;

    bool turn; // true if it's player's turn, false if it's enemy's turn
    int current_turn_index; // index of the player or enemy whose turn it is

    int choice; // attack, skip etc
    int attack_choice; // which person to attack if attack chosen

    int level;
    int sublevel;

    int enemies_defeated;
    int attack_weapon_id; // which weapon to attack with

    struct special_weapon {
        int solar_core_holder;   // -1 if free, otherwise entity ID
        int lunar_blade_holder;  // -1 if free
        int eclipse_relic_holder; // -1 if not introduced or free
        bool eclipse_relic_exists; // 0/1
    };

    // stunned entities, idk how to make em
    // bool paused;

};

struct HIP_Message {
    int player_id;
    int action; // 0 for skip, 1 for attack, 2 for use ultimate etc etc
    int target_id; // which enemy to attack if action is attack
    int weapon_id; // which weapon to use if action is attack or use ultimate
};




class Arbiter{
    vector<Thread_Player> players;
    vector<Thread_Player> suspened; // use signals only
    GameState game_state;
    int strategic_time = 3; // 3 seconds for making a move

public:
    Arbiter(){
        players = vector<Thread_Player>();
        suspened = vector<Thread_Player>();
        game_state = GameState();
    }

    // it will create shared memory itself and pass it to the players
    SharedMemPipe* add_player(int thread_id, bool is_player){
        Thread_Player new_player;
        new_player.thread_id = thread_id;
        new_player.is_player = is_player;
        new_player.turn = false;
        new_player.shared_mem = new SharedMemPipe(("player_pipe_" + std::to_string(thread_id)).c_str(), 1024, true, true);
        players.push_back(new_player);
        return new_player.shared_mem;
    }
};


// for stamina thread, doesn't sleep, constantly adds stamina, should modify the original vector
void* stamina_recovery(void* arg){
    auto* character_staminas = static_cast<vector<Stamina>*>(arg);

    while(true){
        for (auto& stamina : *character_staminas) {
            stamina.current_stamina = std::min(stamina.current_stamina + stamina.recovery_rate, stamina.max_stamina);
        }
        usleep(100000); // Sleep for 100ms to avoid 100% CPU usage
    }

    return nullptr;
}

void* deadlock_detection(void* arg){
    while(true){
        // Check for deadlocks and resolve them
        sleep(5); // Sleep for 5 seconds before checking again
    }
    return nullptr;
}


// pick the first player that has full stamina, if no one has full stamina, return nullptr
Thread_Player* find_turn(vector<Thread_Player>& players){
    for(auto& player : players){
        if(player.stamina.current_stamina >= player.stamina.max_stamina){
            return &player;
        }
    }
    return nullptr;
}


int main(int argc, char* argv[]) {
    Arbiter arbiter;
    SharedMemPipe* hip_pipe = nullptr;
    SharedMemPipe* asp_pipe = nullptr;

    pthread_t stamina_accumalator, turn_decider, deadlock_detector; // stamina will run 4ever, turn will be blocked, deadlock will run every 5 seconds
    vector<Stamina> entities_stamina; // init after both hip and asp are made

    // --- Create shared memory for GameState ---
    const char* shm_name = "/game_state_shm";
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "shm_open failed: " << strerror(errno) << std::endl;
        return 1;
    }
    // Set size
    if (ftruncate(shm_fd, sizeof(GameState)) == -1) {
        std::cerr << "ftruncate failed: " << strerror(errno) << std::endl;
        return 1;
    }
    // Map into Arbiter's address space
    GameState* shared_game_state = (GameState*)mmap(NULL, sizeof(GameState),
                                                    PROT_READ | PROT_WRITE,
                                                    MAP_SHARED, shm_fd, 0);
    if (shared_game_state == MAP_FAILED) {
        std::cerr << "mmap failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // --- Initialize the mutex and condition variable (process-shared) ---
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_game_state->global_mutex, &mutex_attr);
    pthread_mutex_init(&shared_game_state->resource_table_mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_game_state->turn_condition, &cond_attr);
    pthread_condattr_destroy(&cond_attr);

    // Initialize other GameState fields as needed
    shared_game_state->game_running = true;
    shared_game_state->turn = true;   // example: player starts
    shared_game_state->turn_count = 0;
    shared_game_state->game_result = true;
    shared_game_state->choice = -1;
    shared_game_state->attack_choice = -1;
    shared_game_state->level = 1;
    shared_game_state->sublevel = 1;
    shared_game_state->enemies_defeated = 0;
    shared_game_state->attack_weapon_id = -1;
    shared_game_state->special_weapon.solar_core_holder = -1;
    shared_game_state->special_weapon.lunar_blade_holder = -1;
    shared_game_state->special_weapon.eclipse_relic_holder = -1;
    shared_game_state->special_weapon.eclipse_relic_exists = false;
    shared_game_state->current_turn_index = 0;
    // players and enemies vectors will be populated later

    try {
        hip_pipe = new SharedMemPipe("HIP_pipe", 1024, true, true);
    } catch (const std::exception& e) {
        std::cerr << "Error creating shared memory: " << e.what() << std::endl;
        return 1;
    }

    try {
        asp_pipe = new SharedMemPipe("ASP_pipe", 1024, true, true);
    } catch (const std::exception& e) {
        std::cerr << "Error creating shared memory: " << e.what() << std::endl;
        return 1;
    }

    pid_t hip_pid = fork();
    if (hip_pid == 0) {
        // Child process for HIP
        execl("./hip", "./hip", "HIP_pipe", shm_name, nullptr);
        std::cerr << "Failed to exec HIP process" << std::endl;
        return 1;
    } else if (hip_pid < 0) {
        std::cerr << "Failed to fork for HIP process" << std::endl;
        return 1;
    }

    pid_t asp_pid = fork();
    if (asp_pid == 0) {
        // Child process for ASP
        execl("./asp", "./asp", "ASP_pipe", shm_name, nullptr);
        std::cerr << "Failed to exec ASP process" << std::endl;
        return 1;
    } else if (asp_pid < 0) {
        std::cerr << "Failed to fork for ASP process" << std::endl;
        return 1;
    }


    // handling signals type shi


    // Cleanup shared memory
    munmap(shared_game_state, sizeof(GameState));
    shm_unlink(shm_name);

    return 0;
}
