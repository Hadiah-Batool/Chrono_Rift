#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "shared_types.h"
#include "../Characters/Player.h"
#include "../Characters/Enemy.h"
#include "../Weapons/Weapons.h"

// 2. THE PURE GAME STATE
struct GameState {
    bool game_running;
    bool game_result;
    int turn_count;

    int num_active_players;
    std::array<Player, 4> players;

    int total_enemies_spawned;
    int total_players_spawned;

    int num_active_enemies;
    std::array<Enemy, 9> enemies;

    int num_artifacts;
    std::array<Artifact, 5> artifacts;

    int current_turn_owner_id;
    bool is_player_turn;

    int level;
    int sublevel;
    int enemies_defeated;

    bool haslevelended;
    bool hassublevelended;

    struct special_weapon {
        int solar_core_holder;
        int lunar_blade_holder;
        int eclipse_relic_holder;
        bool eclipse_relic_exists;
    } special_weapon_status;

    ActionLog action_log;
};

// 3. THE MASTER SHARED MEMORY BLOCK
struct SharedMemoryBlock {
    pthread_mutex_t global_mutex;
    pthread_mutex_t resource_table_mutex;
    pthread_cond_t turn_condition;

    GameState state;

    ActionRequest hip_mailbox;
    ActionRequest asp_mailbox;
};

#endif // GAME_STATE_H
