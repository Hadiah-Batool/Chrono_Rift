#include "../Characters/Player.h"
#include "../Characters/Enemy.h"
#include "../Weapons/Weapons.h"
#include <array>
#include <pthread.h>

#define ACTION_LOG_SIZE 10      // stores last 10 actions
#define ACTION_MSG_LEN  128     // max chars per message

struct ActionLog {
    char    messages[ACTION_LOG_SIZE][ACTION_MSG_LEN];
    int     head;               // index of oldest message
    int     count;              // how many valid messages (max ACTION_LOG_SIZE)
};

struct Stamina {
    float current_stamina;
    float max_stamina;
    float recovery_rate;
};

// ---------------------------------------------------------
// 1. THE MAILBOX (Written by HIP/ASP, Read by Arbiter)
// ---------------------------------------------------------
struct ActionRequest {
    int requesting_entity_id; // The index of the player/enemy taking the action
    int action_type;          // e.g., 0: Skip, 1: Strike, 2: Exhaust, 3: Use Weapon, etc.
    int target_id;            // Which enemy/player is being attacked
    int weapon_id;            // Which weapon to use (if applicable)

    bool is_ready;            // FLAG: HIP/ASP sets to TRUE when finished writing
};

// ---------------------------------------------------------
// 2. THE PURE GAME STATE (Written by Arbiter, Read by HIP/ASP)
// ---------------------------------------------------------
struct GameState {
    bool game_running;
    bool game_result; // true if player wins, false if enemy wins
    int turn_count;

    // --- Entity Arrays ---
    int num_active_players;
    std::array<Player, 4> players;

    int num_active_enemies;
    std::array<Enemy, 9> enemies;

    int num_artifacts;
    std::array<Artifact, 5> artifacts;

    // --- Arbiter's Turn Assignment ---
    int current_turn_owner_id; // Arbiter sets this so HIP/ASP know who acts
    bool is_player_turn;       // TRUE = HIP wakes up, FALSE = ASP wakes up

    // --- Game Progress ---
    int level;
    int sublevel;
    int enemies_defeated;

    struct special_weapon {
        int solar_core_holder;   // -1 if free, otherwise entity ID
        int lunar_blade_holder;  // -1 if free
        int eclipse_relic_holder; // -1 if not introduced or free
        bool eclipse_relic_exists;
    } special_weapon_status;

    ActionLog action_log;
};

// ---------------------------------------------------------
// 3. THE MASTER SHARED MEMORY BLOCK (What you actually mmap)
// ---------------------------------------------------------
struct SharedMemoryBlock {
    // 1. Synchronization Primitives (Must be here to sync access to the block)
    pthread_mutex_t global_mutex;
    pthread_mutex_t resource_table_mutex;
    pthread_cond_t turn_condition;

    // 2. The Read-Only Data (for the children)
    GameState state;

    // 3. The Writeable Mailboxes (for the children)
    ActionRequest hip_mailbox;
    ActionRequest asp_mailbox;
};
