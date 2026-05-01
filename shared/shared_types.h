#include "../resources/shared_mem_abs.h"
#include "../Characters/Player.h"
#include "../Characters/Enemy.h"
#include "../Weapons/Weapons.h"


#define ACTION_LOG_SIZE 10      // stores last 10 actions
#define ACTION_MSG_LEN  128     // max chars per message

struct ActionLog {
    char    messages[ACTION_LOG_SIZE][ACTION_MSG_LEN];
    int     head;               // index of oldest message
    int     count;              // how many valid messages (max ACTION_LOG_SIZE)
};
struct Stamina 
{
    float current_stamina;
    float max_stamina;
    float recovery_rate; // stamina points recovered per second
};

struct Thread_Player
{
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


    struct special_weapon 
    {
        int solar_core_holder;   // -1 if free, otherwise entity ID
        int lunar_blade_holder;  // -1 if free
        int eclipse_relic_holder; // -1 if not introduced or free
        bool eclipse_relic_exists; // 0/1
    };
    special_weapon special_weapon_status;
    // stunned entities, idk how to make em
    // bool paused;

    //Action Log type shi->Naam se zahir ho rha
    ActionLog action_log;

};

struct HIP_Message {
    int player_id;
    int action; // 0 for skip, 1 for attack, 2 for use ultimate etc etc
    int target_id; // which enemy to attack if action is attack
    int weapon_id; // which weapon to use if action is attack or use ultimate
};