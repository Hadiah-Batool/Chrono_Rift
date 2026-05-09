#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "shared_types.h"
#include "../Characters/Player.h"
#include "../Characters/Enemy.h"
#include "../Weapons/Weapons.h"

// ---------------------------------------------------------------------------
// ACTION ENUM  — add GET_ARTIFACT and RELEASE_ARTIFACT alongside your
// existing actions.  Keep all previous values so nothing breaks.
// ---------------------------------------------------------------------------
// NOTE: If your Action enum lives in shared_types.h, move ONLY the two new
//       values there.  They are shown here for clarity.
//
//  enum class Action {
//      STRIKE, EXHAUST, USE_WEAPON, SWAP_IN, HEAL, SKIP, SETUP_GAME,
//      GET_ARTIFACT,      // ← NEW: entity requests to lock an artifact
//      RELEASE_ARTIFACT,  // ← NEW: entity releases a previously held artifact
//  };

// ---------------------------------------------------------------------------
// ARTIFACT WAIT FIELDS
// Each entity that is blocked waiting for an artifact stores the artifact
// index it needs here (-1 = not waiting).
// These two parallel arrays live inside GameState so the deadlock watchdog
// (running inside the Arbiter) can inspect them without extra IPC.
// ---------------------------------------------------------------------------
struct ArtifactWaitState {
    // Index into GameState::artifacts[] that this entity is waiting for.
    // -1  → not waiting for anything right now.
    int waiting_for_artifact_idx;

    // Index into GameState::artifacts[] that this entity CURRENTLY HOLDS.
    // -1  → holds nothing.
    int holding_artifact_idx;

    ArtifactWaitState() : waiting_for_artifact_idx(-1), holding_artifact_idx(-1) {}
};


// ---------------------------------------------------------------------------
// GAME STATE
// ---------------------------------------------------------------------------
struct GameState {
    bool game_running;
    bool game_result;
    int  turn_count;

    int num_active_players;
    std::array<Player, 4> players;

    int total_enemies_spawned;
    int total_players_spawned;

    int num_active_enemies;
    std::array<Enemy, 9> enemies;

    int num_artifacts;
    std::array<Artifact, 5> artifacts;

    // ── NEW: per-entity artifact wait / hold tracking ──────────────────────
    // players_artifact_state[i]  corresponds to players[i]
    // enemies_artifact_state[i]  corresponds to enemies[i]
    std::array<ArtifactWaitState, 4> players_artifact_state;
    std::array<ArtifactWaitState, 9> enemies_artifact_state;

    int  current_turn_owner_id;
    bool is_player_turn;

    int level;
    int sublevel;
    int enemies_defeated;

    bool haslevelended;
    bool hassublevelended;

    struct special_weapon {
        int  solar_core_holder;
        int  lunar_blade_holder;
        int  eclipse_relic_holder;
        bool eclipse_relic_exists;
    } special_weapon_status;

    ActionLog action_log;

    bool is_weapon_dropped = false;
    Weapon dropped_weapon;
};

// ---------------------------------------------------------------------------
// SHARED MEMORY BLOCK  (unchanged layout — just shown for completeness)
// ---------------------------------------------------------------------------
struct SharedMemoryBlock {
    pthread_mutex_t global_mutex;
    pthread_mutex_t resource_table_mutex;   // guards artifact table access
    pthread_cond_t  turn_condition;

    GameState state;

    ActionRequest hip_mailbox;
    ActionRequest asp_mailbox;
};

#endif // GAME_STATE_H
