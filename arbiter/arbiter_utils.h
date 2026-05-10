#ifndef ARBITER_UTILS_H
#define ARBITER_UTILS_H

#include <fstream>
#include <pthread.h>
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
#include <fcntl.h>
#include <sys/mman.h>
#include <cstring>
#include "../shared/game_state.h"
#include <time.h>

using std::vector;
using std::cout;
using std::endl;
using std::array;

#define time_of_response 3

// ─────────────────────────────────────────────────────────────────────────────
//  GLOBALS (Defined in arbiter.cpp)
// ─────────────────────────────────────────────────────────────────────────────
extern pid_t g_asp_pid;
extern SharedMemoryBlock* g_shm_ptr;
extern volatile sig_atomic_t g_sigalrm_received;
extern volatile sig_atomic_t g_sigterm_received;

// ─────────────────────────────────────────────────────────────────────────────
//  ARTIFACT HELPER SECTION (Requires resource_table_mutex lock)
// ─────────────────────────────────────────────────────────────────────────────

static inline int find_artifact_idx(SharedMemoryBlock* sb, int weapon_id) {
    for (int i = 0; i < sb->state.num_artifacts; ++i)
        // GUARD ADDED: Must be available!
        if (sb->state.artifacts[i].getWeaponId() == weapon_id && sb->state.artifacts[i].isAvailable())
            return i;
    return -1;
}

static inline bool try_grant_artifact_to_player(SharedMemoryBlock* sb, int player_idx, int art_idx) {
    Artifact& art = sb->state.artifacts[art_idx];
    if (art.isHeld()) {
        sb->state.players_artifact_state[player_idx].waiting_for_artifact_idx = art_idx;
        std::cout << "[ARBITER][ARTIFACT] Player " << player_idx
                  << " is WAITING for artifact " << art_idx
                  << " (held by entity " << art.getHolder() << ")\n";
        return false;
    }
    art.setHolder(player_idx);
    sb->state.players_artifact_state[player_idx].holding_artifact_idx   = art_idx;
    sb->state.players_artifact_state[player_idx].waiting_for_artifact_idx = -1;
    std::cout << "[ARBITER][ARTIFACT] Player " << player_idx
              << " acquired artifact " << art_idx << " ("
              << art.getName() << ")\n";
    sb->state.players[player_idx].pickupWeapon(static_cast<Weapon&>(art));
    return true;
}

static inline bool try_grant_artifact_to_enemy(SharedMemoryBlock* sb, int enemy_idx, int art_idx) {
    Artifact& art = sb->state.artifacts[art_idx];
    if (art.isHeld()) {
        sb->state.enemies_artifact_state[enemy_idx].waiting_for_artifact_idx = art_idx;
        std::cout << "[ARBITER][ARTIFACT] Enemy " << enemy_idx
                  << " is WAITING for artifact " << art_idx << "\n";
        return false;
    }
    art.setHolder(10 + enemy_idx);
    sb->state.enemies_artifact_state[enemy_idx].holding_artifact_idx   = art_idx;
    sb->state.enemies_artifact_state[enemy_idx].waiting_for_artifact_idx = -1;
    std::cout << "[ARBITER][ARTIFACT] Enemy " << enemy_idx
              << " acquired artifact " << art_idx << "\n";
    return true;
}

// --- ADD THIS HELPER TO UNFREEZE WAITING ENTITIES ---
static inline void wake_waiters_for_artifact(SharedMemoryBlock* sb, int art_idx) {
    for(int p = 0; p < sb->state.num_active_players; ++p) {
        if (sb->state.players_artifact_state[p].waiting_for_artifact_idx == art_idx)
            sb->state.players_artifact_state[p].waiting_for_artifact_idx = -1;
    }
    for(int e = 0; e < sb->state.num_active_enemies; ++e) {
        if (sb->state.enemies_artifact_state[e].waiting_for_artifact_idx == art_idx)
            sb->state.enemies_artifact_state[e].waiting_for_artifact_idx = -1;
    }
}

static inline void release_artifact_from_player(SharedMemoryBlock* sb, int player_idx) {
    int art_idx = sb->state.players_artifact_state[player_idx].holding_artifact_idx;
    if (art_idx < 0) return;
    sb->state.artifacts[art_idx].release();
    sb->state.players_artifact_state[player_idx].holding_artifact_idx   = -1;
    sb->state.players_artifact_state[player_idx].waiting_for_artifact_idx = -1;
    std::cout << "[ARBITER][ARTIFACT] Player " << player_idx << " released artifact " << art_idx << "\n";

    wake_waiters_for_artifact(sb, art_idx); // <--- AWAKENS EVERYONE WAITING FOR IT
    pthread_cond_broadcast(&sb->turn_condition);
}

static inline void release_artifact_from_enemy(SharedMemoryBlock* sb, int enemy_idx) {
    int art_idx = sb->state.enemies_artifact_state[enemy_idx].holding_artifact_idx;
    if (art_idx < 0) return;
    sb->state.artifacts[art_idx].release();
    sb->state.enemies_artifact_state[enemy_idx].holding_artifact_idx   = -1;
    sb->state.enemies_artifact_state[enemy_idx].waiting_for_artifact_idx = -1;
    std::cout << "[ARBITER][ARTIFACT] Enemy " << enemy_idx << " released artifact " << art_idx << "\n";

    wake_waiters_for_artifact(sb, art_idx); // <--- AWAKENS EVERYONE WAITING FOR IT
    pthread_cond_broadcast(&sb->turn_condition);
}

// ─────────────────────────────────────────────────────────────────────────────
//  WEAPON DROP & DEATH HANDLER
// ─────────────────────────────────────────────────────────────────────────────

static inline Weapon generateRandomWeapon(int& out_id) {
    static int weapon_counter = 100;
    int type_rand = rand() % 6;
    out_id = weapon_counter++;

    switch(type_rand) {
        case 0: return Weapon(out_id, WeaponType::IRON_HALBERD, "Iron Halberd", 7, 55);
        case 1: return Weapon(out_id, WeaponType::VENOM_DAGGER, "Venom Dagger", 4, 30);
        case 2: return Weapon(out_id, WeaponType::THUNDERSTAFF, "Thunderstaff", 6, 50);
        case 3: return Weapon(out_id, WeaponType::OBSIDIAN_AXE, "Obsidian Axe", 5, 45);
        case 4: return Weapon(out_id, WeaponType::FROSTBOW, "Frostbow", 6, 48);
        default: return Weapon(out_id, WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12);
    }
}

static inline void handle_enemy_death(SharedMemoryBlock* shared_block, int target_id) {
    release_artifact_from_enemy(shared_block, target_id);
    shared_block->state.enemies_defeated++;
    std::cout << "[ARBITER] Enemy " << target_id
              << " defeated! Total: "
              << shared_block->state.enemies_defeated << "/10\n";

    int drop_roll = rand() % 100;

    if (drop_roll < 10
        && !shared_block->state.artifacts[2].isAvailable()
        && !shared_block->state.artifacts[2].isHeld())
    {
        std::cout << "\n[ARBITER] *** A blinding light bursts from the fallen enemy! ***\n";
        std::cout << "[ARBITER] *** The ECLIPSE RELIC has been introduced! ***\n\n";
        pthread_mutex_lock(&shared_block->resource_table_mutex);
        shared_block->state.artifacts[2].introduce();
        pthread_mutex_unlock(&shared_block->resource_table_mutex);
        return;
    }

    if (drop_roll >= 10 && drop_roll < 70)
    {
        if (shared_block->state.is_weapon_dropped)
        {
            std::cout << "[ARBITER] Enemy " << target_id
                      << " would have dropped a weapon, but the ground"
                      << " is already occupied!\n";
        }
        else
        {
            int w_id;
            shared_block->state.dropped_weapon      = generateRandomWeapon(w_id);
            shared_block->state.is_weapon_dropped   = true;
            shared_block->state.dropped_by_enemy_id = target_id;

            std::cout << "\n[ARBITER] Enemy " << target_id
                      << " dropped: "
                      << shared_block->state.dropped_weapon.getName() << "!\n";
            std::cout << "[ARBITER] (Press P to PICKUP, or an enemy will steal it!)\n\n";
        }
        return;
    }

    std::cout << "[ARBITER] Enemy " << target_id << " dropped nothing.\n";
}


// ─────────────────────────────────────────────────────────────────────────────
//  ARBITER KERNEL CLASS
// ─────────────────────────────────────────────────────────────────────────────

class Arbiter {
private:
    SharedMemoryBlock* shared_block;
public:
    Arbiter(SharedMemoryBlock* block) { this->shared_block = block; }

    void initialize_entities(int seed_roll_full, int seed_last_dig, int seed_last_two, int level, int sublevel) {
        std::string filename = "enemies_description/level_" + std::to_string(level) + "_sublevel_" + std::to_string(sublevel) + ".txt";
        std::ifstream infile(filename);
        if (infile.is_open()) {
            int num_enemies;
            if (infile >> num_enemies) {
                shared_block->state.num_active_enemies = num_enemies;
                std::cout << "[ARBITER] Parsing " << filename << ". Spawning " << num_enemies << " enemies.\n";
                for (int i = 0; i < num_enemies; ++i) {
                    int x, y, type;
                    if (infile >> x >> y >> type)
                    {
                        new (&shared_block->state.enemies[i]) Enemy(i, static_cast<EnemyType>(type));
                        shared_block->state.enemies[i].setRollNumber(seed_roll_full, seed_last_dig, seed_last_two);
                        shared_block->state.enemies[i].initRollStats();
                        shared_block->state.enemies[i].setAlive(true);
                        shared_block->state.enemies[i].clearStun();
                        shared_block->state.enemies[i].ResetStamina();

                        shared_block->state.enemies[i].InitAllProperties(static_cast<float>(x), static_cast<float>(y));
                        std::cout << "  -> Spawned Enemy " << i << " (Type: " << type << ")\n";
                    }
                }
            }
            infile.close();
        } else {
            std::cerr << "[ARBITER] CRITICAL: Could not open " << filename << "\n";
        }
    }


    void initialize_players_positions(int level, int sublevel) {
        std::string filename = "player_description/level_" + std::to_string(level) + "_sublevel_" + std::to_string(sublevel) + ".txt";
        std::ifstream infile(filename);
        if (infile.is_open()) {
            int num_players_in_file;
            if (infile >> num_players_in_file) {
                std::cout << "[ARBITER] Parsing " << filename << " for player positions.\n";
                int limit = std::min(num_players_in_file, shared_block->state.num_active_players);
                for (int i = 0; i < limit; ++i) {
                    int x, y, type;
                    if (infile >> x >> y >> type) {
                        shared_block->state.players[i].InitAllProperties(static_cast<float>(x), static_cast<float>(y));
                        std::cout << "  -> Positioned Player " << i << " at (" << x << ", " << y << ")\n";
                    }
                }
            }
            infile.close();
        } else {
            std::cerr << "[ARBITER] WARNING: Could not open " << filename << "\n";
        }
    }

    void find_turn(int* out_turn_index, bool* out_turn) {
        *out_turn_index = -1;
        bool check_players_first = rand() % 2 == 0;
        int num_players = shared_block->state.num_active_players;
        int num_enemies = shared_block->state.num_active_enemies;

        if(check_players_first) {
            for(int i = 0; i < num_players; ++i) {
                if (!shared_block->state.players[i].isAlive() || shared_block->state.players[i].isStunned() ||
                    shared_block->state.players_artifact_state[i].waiting_for_artifact_idx != -1) continue;
                if(shared_block->state.players[i].getStamina() >= shared_block->state.players[i].getMaxStamina()) {
                    *out_turn_index = i; *out_turn = true; return;
                }
            }
            for(int i = 0; i < num_enemies; ++i) {
                if (!shared_block->state.enemies[i].isAlive() || shared_block->state.enemies[i].isStunned() ||
                    shared_block->state.enemies_artifact_state[i].waiting_for_artifact_idx != -1) continue;
                if(shared_block->state.enemies[i].getStamina() >= shared_block->state.enemies[i].getMaxStamina()) {
                    *out_turn_index = num_players + i; *out_turn = false; return;
                }
            }
        } else {
            for(int i = 0; i < num_enemies; ++i) {
                if (!shared_block->state.enemies[i].isAlive() || shared_block->state.enemies[i].isStunned() ||
                    shared_block->state.enemies_artifact_state[i].waiting_for_artifact_idx != -1) continue;
                if(shared_block->state.enemies[i].getStamina() >= shared_block->state.enemies[i].getMaxStamina()) {
                    *out_turn_index = num_players + i; *out_turn = false; return;
                }
            }
            for(int i = 0; i < num_players; ++i) {
                if (!shared_block->state.players[i].isAlive() || shared_block->state.players[i].isStunned() ||
                    shared_block->state.players_artifact_state[i].waiting_for_artifact_idx != -1) continue;
                if(shared_block->state.players[i].getStamina() >= shared_block->state.players[i].getMaxStamina()) {
                    *out_turn_index = i; *out_turn = true; return;
                }
            }
        }
    }

    void apply_stun_to_enemy(int enemy_index, int duration_seconds, pid_t asp_pid) {
        std::cout << "[ARBITER] Applying stun to Enemy " << enemy_index << " for " << duration_seconds << " seconds.\n";
        shared_block->state.enemies[enemy_index].applyStun(duration_seconds);
        kill(asp_pid, SIGUSR1);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  STAMINA THREAD
// ─────────────────────────────────────────────────────────────────────────────

inline void* stamina_recovery(void* arg){
    auto* shared_block = static_cast<SharedMemoryBlock*>(arg);
    while(true){
        pthread_mutex_lock(&shared_block->global_mutex);
        if (!shared_block->state.game_running || g_sigterm_received) {
            pthread_mutex_unlock(&shared_block->global_mutex);
            break;
        }

        if (shared_block->state.game_running && !shared_block->state.hassublevelended) {
            int current_turn = shared_block->state.turn_count;

            for (int i = 0; i < shared_block->state.num_active_players; ++i) {
                auto& player = shared_block->state.players[i];
                if (player.isAlive()) {
                    if (player.isStunned() && current_turn >= player.getStunEndTem()) {
                        player.clearStun();
                    }
                    int new_stamina = player.getStamina() + player.getStaminaRecoveryRate();
                    player.setStamina(std::min(new_stamina, player.getMaxStamina()));
                }
            }
            for (int i = 0; i < shared_block->state.num_active_enemies; ++i) {
                auto& enemy = shared_block->state.enemies[i];
                if (enemy.isAlive()) {
                    if (enemy.isStunned() && current_turn >= enemy.getStunEndTem()) {
                        enemy.clearStun();
                    }
                    int new_stamina = enemy.getStamina() + enemy.getStaminaRecoveryRate();
                    enemy.setStamina(std::min(new_stamina, enemy.getMaxStamina()));
                }
            }
        }
        pthread_mutex_unlock(&shared_block->global_mutex);
        usleep(1000000);
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ACTION HANDLERS
// ─────────────────────────────────────────────────────────────────────────────

inline void handle_player_action(const ActionRequest& request, SharedMemoryBlock* shared_block) {
    int attacker_id = shared_block->hip_mailbox.requesting_entity_id;
    int target_id   = shared_block->hip_mailbox.target_id;

    if (shared_block->state.is_weapon_dropped && request.action_type != Action::PICKUP) {
        std::cout << "[ARBITER] Player " << attacker_id << " ignored the dropped weapon!\n";
        for(int e = 0; e < shared_block->state.num_active_enemies; e++) {
            if (shared_block->state.enemies[e].isAlive()) {
                shared_block->state.enemies[e].setDemage(shared_block->state.enemies[e].getDemage() + shared_block->state.dropped_weapon.getDamage());
                std::cout << "[ARBITER] Enemy " << e << " snatched the " << shared_block->state.dropped_weapon.getName() << " and gained its damage!\n";
                break;
            }
        }
        shared_block->state.is_weapon_dropped = false;
    }

    switch (request.action_type) {

    case Action::PICKUP: {
        if (shared_block->state.is_weapon_dropped) {
            bool success = shared_block->state.players[attacker_id].pickupWeapon(shared_block->state.dropped_weapon);

            if (success) {
                std::cout << "[ARBITER] Player " << attacker_id << " looted the " << shared_block->state.dropped_weapon.getName() << "!\n";
                shared_block->state.is_weapon_dropped = false;
                shared_block->state.dropped_by_enemy_id = -1;
            } else {
                std::cout << "[ARBITER] Player " << attacker_id << " tried to pick it up, but inventory swapping failed!\n";
            }
        } else {
            std::cout << "[ARBITER] There is no weapon on the ground to pick up.\n";
            shared_block->state.dropped_by_enemy_id = -1;
        }
        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }

    case Action::STRIKE: {
        if (!shared_block->state.enemies[target_id].isAlive()) {
            std::cout << "[ARBITER] Target already dead. Strike wasted!\n";
            shared_block->state.players[attacker_id].ResetStamina();
            break;
        }

        int damage = shared_block->state.players[attacker_id].getDemage();
        shared_block->state.enemies[target_id].TakeDamage(damage);

        std::cout << "[ARBITER] Player " << attacker_id << " struck Enemy " << target_id
          << " for " << damage << " DMG! (Enemy HP: " << shared_block->state.enemies[target_id].getHp() << ")" << endl;

        if (!shared_block->state.enemies[target_id].isAlive()) {
            handle_enemy_death(shared_block, target_id);
        }

        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }

    case Action::EXHAUST: {
        int damage        = shared_block->state.players[attacker_id].getDemage();
        int cur_stamina   = shared_block->state.enemies[target_id].getStamina();
        shared_block->state.enemies[target_id].setStamina(damage > cur_stamina ? 0 : cur_stamina - damage);
        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }

    case Action::USE_WEAPON: {
        if (!shared_block->state.enemies[target_id].isAlive()) {
            std::cout << "[ARBITER] Target already dead. Weapon strike wasted!\n";
            shared_block->state.players[attacker_id].ResetStamina();
            break;
        }

        int weapon_id = shared_block->hip_mailbox.weapon_id;
        int weapon_damage = 0;

        if (shared_block->state.players[attacker_id].getInventory().hasWeapon(weapon_id)) {
            weapon_damage = shared_block->state.players[attacker_id].getInventory().getEquippedWeapons().at(weapon_id).second.getDamage();
        }

        shared_block->state.enemies[target_id].TakeDamage(weapon_damage);
        std::cout << "[ARBITER] Player " << attacker_id << " used weapon " << weapon_id
                  << " on Enemy " << target_id << " for " << weapon_damage << " DMG!\n";

        if (!shared_block->state.enemies[target_id].isAlive()) {
            handle_enemy_death(shared_block, target_id);
        }

        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }

    case Action::SWAP_IN: {
        int backpack_idx = shared_block->hip_mailbox.weapon_id;
        int bp_count = shared_block->state.players[attacker_id].getBackpack().getCount();

        if (backpack_idx >= 0 && backpack_idx < bp_count) {
            shared_block->state.players[attacker_id].swapInFromBackpack(backpack_idx);
            std::cout << "[ARBITER] Player " << attacker_id << " swapped in backpack slot " << backpack_idx << "\n";
        } else {
            std::cout << "[ARBITER] SWAP_IN ignored: invalid backpack index " << backpack_idx << " (count=" << bp_count << ")\n";
        }
        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }

    case Action::HEAL: {
        int heal_amount = shared_block->state.players[attacker_id].getMaxHp() / 10;
        shared_block->state.players[attacker_id].RegainHealth(heal_amount);
        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }
    case Action::SKIP: {
        int half = shared_block->state.players[attacker_id].getMaxStamina() / 2;
        shared_block->state.players[attacker_id].setStamina(half);
        break;
    }
    case Action::SETUP_GAME: {
        shared_block->state.num_active_players = shared_block->hip_mailbox.target_id;
        for (int i = 0; i < shared_block->state.num_active_players; i++) {
            new (&shared_block->state.players[i]) Player(shared_block->hip_mailbox.types[i]);
            shared_block->state.players[i].setRollNumber(0607, 7, 7);
            shared_block->state.players[i].initRollStats(100.0f / shared_block->state.num_active_players);
        }
        break;
    }
    case Action::GET_ARTIFACT: {
        int desired_weapon_id = shared_block->hip_mailbox.weapon_id;
        pthread_mutex_lock(&shared_block->resource_table_mutex);
        int art_idx = find_artifact_idx(shared_block, desired_weapon_id);
        if (art_idx == -1) {
            std::cout << "[ARBITER][ARTIFACT] Player requested non-existent artifact. Ignored.\n";
            pthread_mutex_unlock(&shared_block->resource_table_mutex);
            break;
        }
        bool granted = try_grant_artifact_to_player(shared_block, attacker_id, art_idx);
        pthread_mutex_unlock(&shared_block->resource_table_mutex);
        if (granted) shared_block->state.players[attacker_id].ResetStamina();
        break;
    }
    case Action::RELEASE_ARTIFACT: {
        pthread_mutex_lock(&shared_block->resource_table_mutex);
        release_artifact_from_player(shared_block, attacker_id);
        pthread_mutex_unlock(&shared_block->resource_table_mutex);
        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }
    case Action::ULTIMATE: {
        bool has_solar = shared_block->state.players[attacker_id].getInventory().hasWeapon(0);
        bool has_lunar = shared_block->state.players[attacker_id].getInventory().hasWeapon(1);

        if (has_solar && has_lunar) {
            std::cout << "\n[ARBITER] *** Player " << attacker_id << " triggered the ULTIMATE ABILITY! ***\n";
            std::cout << "[ARBITER] *** Sending SIGSTOP to ASP. Enemies frozen for 10 seconds! ***\n\n";
            kill(g_asp_pid, SIGSTOP);
            alarm(10);
        } else {
            std::cout << "[ARBITER] Player " << attacker_id << " attempted Ultimate but lacks the required artifacts (Needs both Solar Core and Lunar Blade)!\n";
        }
        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }
    default:
        break;
    }
}

inline void handle_enemy_action(const ActionRequest& request, SharedMemoryBlock* shared_block) {
    int attacker_id = shared_block->asp_mailbox.requesting_entity_id;
    int target_id   = shared_block->asp_mailbox.target_id;

    if (shared_block->state.is_weapon_dropped) {
        if (shared_block->state.enemies[attacker_id].isAlive()) {
            shared_block->state.enemies[attacker_id].setDemage(shared_block->state.enemies[attacker_id].getDemage() + shared_block->state.dropped_weapon.getDamage());
            std::cout << "[ARBITER] Enemy " << attacker_id << " snatched the dropped weapon on its turn!\n";
        }
        shared_block->state.is_weapon_dropped = false;
        shared_block->state.dropped_by_enemy_id = -1;
    }

    switch (request.action_type) {
    case Action::STRIKE: {
        if (!shared_block->state.players[target_id].isAlive()) {
            std::cout << "[ARBITER] Player " << target_id << " is already dead! Ignoring action.\n";
            break;
        }
        int damage = shared_block->state.enemies[attacker_id].getDemage();
        shared_block->state.players[target_id].TakeDamage(damage);

        std::cout << "[ARBITER] Enemy " << attacker_id << " struck Player " << target_id
          << " for " << damage << " DMG! (Player HP: " << shared_block->state.players[target_id].getHp() << ")" << endl;

        if (!shared_block->state.players[target_id].isAlive()) {
            shared_block->state.num_active_players--;
            std::cout << "[ARBITER] Player " << target_id << " has fallen! Active: " << shared_block->state.num_active_players << "\n";
        }
        shared_block->state.enemies[attacker_id].ResetStamina();
        break;
    }
    case Action::SKIP: {
        int half = shared_block->state.enemies[attacker_id].getMaxStamina() / 2;
        shared_block->state.enemies[attacker_id].setStamina(half);
        break;
    }
    case Action::GET_ARTIFACT: {
        int desired_weapon_id = shared_block->asp_mailbox.weapon_id;
        pthread_mutex_lock(&shared_block->resource_table_mutex);
        int art_idx = find_artifact_idx(shared_block, desired_weapon_id);
        if (art_idx == -1) {
            std::cout << "[ARBITER][ARTIFACT] Enemy requested non-existent artifact. Ignored.\n";
            pthread_mutex_unlock(&shared_block->resource_table_mutex);
            break;
        }
        bool granted = try_grant_artifact_to_enemy(shared_block, attacker_id, art_idx);
        pthread_mutex_unlock(&shared_block->resource_table_mutex);
        if (granted) shared_block->state.enemies[attacker_id].ResetStamina();
        break;
    }
    case Action::RELEASE_ARTIFACT: {
        pthread_mutex_lock(&shared_block->resource_table_mutex);
        release_artifact_from_enemy(shared_block, attacker_id);
        pthread_mutex_unlock(&shared_block->resource_table_mutex);
        shared_block->state.enemies[attacker_id].ResetStamina();
        break;
    }
    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  DEADLOCK WATCHDOG THREAD
// ─────────────────────────────────────────────────────────────────────────────

static inline int holder_of(SharedMemoryBlock* sb, int art_idx) {
    if (art_idx < 0 || art_idx >= sb->state.num_artifacts) return -1;
    return sb->state.artifacts[art_idx].isHeld() ? sb->state.artifacts[art_idx].getHolder() : -1;
}

static inline int waiting_for(SharedMemoryBlock* sb, int entity_id) {
    if (entity_id >= 0 && entity_id < 4) return sb->state.players_artifact_state[entity_id].waiting_for_artifact_idx;
    if (entity_id >= 10 && entity_id < 19) return sb->state.enemies_artifact_state[entity_id - 10].waiting_for_artifact_idx;
    return -1;
}

static inline void force_abandon_wait(SharedMemoryBlock* sb, int entity_id) {
    if (entity_id >= 0 && entity_id < 4) {
        int art_idx = sb->state.players_artifact_state[entity_id].waiting_for_artifact_idx;
        sb->state.players_artifact_state[entity_id].waiting_for_artifact_idx = -1;
        int half = sb->state.players[entity_id].getMaxStamina() / 2;
        sb->state.players[entity_id].setStamina(half);
        std::cout << "[ARBITER][DEADLOCK] Forced Player " << entity_id << " to abandon wait for artifact " << art_idx << ".\n";
    }
    else if (entity_id >= 10 && entity_id < 19) {
        int ei = entity_id - 10;
        int art_idx = sb->state.enemies_artifact_state[ei].waiting_for_artifact_idx;
        sb->state.enemies_artifact_state[ei].waiting_for_artifact_idx = -1;
        int half = sb->state.enemies[ei].getMaxStamina() / 2;
        sb->state.enemies[ei].setStamina(half);
        std::cout << "[ARBITER][DEADLOCK] Forced Enemy " << ei << " to abandon wait for artifact " << art_idx << ".\n";
    }
}

inline void* deadlock_detection(void* arg) {
    auto* sb = static_cast<SharedMemoryBlock*>(arg);
    while (true) {
        sleep(10);
        if (!sb->state.game_running || g_sigterm_received) break;

        pthread_mutex_lock(&sb->resource_table_mutex);
        bool deadlock_found = false;
        int waiters[13];
        int num_waiters = 0;

        for (int p = 0; p < sb->state.num_active_players; ++p) {
            if (sb->state.players_artifact_state[p].waiting_for_artifact_idx != -1)
                waiters[num_waiters++] = p;
        }
        for (int e = 0; e < sb->state.num_active_enemies; ++e) {
            if (sb->state.enemies_artifact_state[e].waiting_for_artifact_idx != -1)
                waiters[num_waiters++] = 10 + e;
        }

        for (int i = 0; i < num_waiters && !deadlock_found; ++i) {
            int entity_A = waiters[i];
            int art_X    = waiting_for(sb, entity_A);
            int entity_B = holder_of(sb, art_X);
            if (entity_B == -1) continue;

            int art_Y    = waiting_for(sb, entity_B);
            if (art_Y == -1) continue;
            int entity_C = holder_of(sb, art_Y);

            // CHECK FOR 2-WAY AND 3-WAY DEADLOCKS
            if (entity_C == entity_A) {
                std::cout << "[ARBITER][DEADLOCK] *** 2-WAY CIRCULAR WAIT DETECTED ***\n"
                          << "  Entity " << entity_A << " waits for " << entity_B << " who waits for " << entity_A << "\n";
                force_abandon_wait(sb, entity_A);
                deadlock_found = true;
            } else if (entity_C != -1) {
                int art_Z = waiting_for(sb, entity_C);
                if (art_Z != -1) {
                    int entity_D = holder_of(sb, art_Z);
                    if (entity_D == entity_A) {
                        std::cout << "[ARBITER][DEADLOCK] *** 3-WAY CIRCULAR WAIT DETECTED ***\n"
                                  << "  Entity " << entity_A << " -> " << entity_B << " -> " << entity_C << " -> " << entity_A << "\n";
                        force_abandon_wait(sb, entity_A);
                        deadlock_found = true;
                    }
                }
            }
        }
        if (!deadlock_found) std::cout << "[ARBITER][DEADLOCK] Watchdog tick: no deadlock detected.\n";
        pthread_mutex_unlock(&sb->resource_table_mutex);

        pthread_mutex_lock(&sb->global_mutex);
        pthread_cond_broadcast(&sb->turn_condition);
        pthread_mutex_unlock(&sb->global_mutex);
    }
    return nullptr;
}

#endif // ARBITER_UTILS_H
