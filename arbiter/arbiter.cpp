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
//  OS SIGNAL HANDLERS (Sections 8 & 10)
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
//  OS SIGNAL HANDLERS (Sections 8 & 10)
// ─────────────────────────────────────────────────────────────────────────────
static pid_t g_asp_pid = -1;
static SharedMemoryBlock* g_shm_ptr = nullptr;

// POSIX safe signal flags
static volatile sig_atomic_t g_sigalrm_received = 0;
static volatile sig_atomic_t g_sigterm_received = 0;

static void handle_sigalrm(int sig) { g_sigalrm_received = 1; }
static void handle_sigterm(int sig) { g_sigterm_received = 1; }

// ─────────────────────────────────────────────────────────────────────────────
//  ARTIFACT HELPER SECTION (Requires resource_table_mutex lock)
// ─────────────────────────────────────────────────────────────────────────────

static int find_artifact_idx(SharedMemoryBlock* sb, int weapon_id) {
    for (int i = 0; i < sb->state.num_artifacts; ++i)
        if (sb->state.artifacts[i].getWeaponId() == weapon_id)
            return i;
    return -1;
}

static bool try_grant_artifact_to_player(SharedMemoryBlock* sb, int player_idx, int art_idx) {
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

static bool try_grant_artifact_to_enemy(SharedMemoryBlock* sb, int enemy_idx, int art_idx) {
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

static void release_artifact_from_player(SharedMemoryBlock* sb, int player_idx) {
    int art_idx = sb->state.players_artifact_state[player_idx].holding_artifact_idx;
    if (art_idx < 0) return;
    sb->state.artifacts[art_idx].release();
    sb->state.players_artifact_state[player_idx].holding_artifact_idx   = -1;
    sb->state.players_artifact_state[player_idx].waiting_for_artifact_idx = -1;
    std::cout << "[ARBITER][ARTIFACT] Player " << player_idx << " released artifact " << art_idx << "\n";
    pthread_cond_broadcast(&sb->turn_condition);
}

static void release_artifact_from_enemy(SharedMemoryBlock* sb, int enemy_idx) {
    int art_idx = sb->state.enemies_artifact_state[enemy_idx].holding_artifact_idx;
    if (art_idx < 0) return;
    sb->state.artifacts[art_idx].release();
    sb->state.enemies_artifact_state[enemy_idx].holding_artifact_idx   = -1;
    sb->state.enemies_artifact_state[enemy_idx].waiting_for_artifact_idx = -1;
    std::cout << "[ARBITER][ARTIFACT] Enemy " << enemy_idx << " released artifact " << art_idx << "\n";
    pthread_cond_broadcast(&sb->turn_condition);
}

// ─────────────────────────────────────────────────────────────────────────────
//  WEAPON DROP & DEATH HANDLER (Sections 6 & 7)
// ─────────────────────────────────────────────────────────────────────────────

static Weapon generateRandomWeapon(int& out_id) {
    static int weapon_counter = 100; // High ID to avoid clashing with standard inventory
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

static void handle_enemy_death(SharedMemoryBlock* shared_block, int target_id) {
    release_artifact_from_enemy(shared_block, target_id);
    shared_block->state.enemies_defeated++;
    std::cout << "[ARBITER] Enemy " << target_id << " defeated! Total: " << shared_block->state.enemies_defeated << "/10\n";

    int drop_roll = rand() % 100;

    // 15% Chance to introduce the Eclipse Relic
    if (drop_roll < 15 && !shared_block->state.artifacts[2].isAvailable() && !shared_block->state.artifacts[2].isHeld()) {
        std::cout << "\n[ARBITER] *** A blinding light bursts from the fallen enemy! ***\n";
        std::cout << "[ARBITER] *** The ECLIPSE RELIC has been introduced! (Use 'g 2' to lock it) ***\n\n";

        pthread_mutex_lock(&shared_block->resource_table_mutex); // <-- ADDED LOCK
        shared_block->state.artifacts[2].introduce();
        pthread_mutex_unlock(&shared_block->resource_table_mutex); // <-- ADDED UNLOCK
    }

    // 35% Chance to drop a standard weapon (only if the ground is clear)
    else if (drop_roll >= 15 && drop_roll < 50 && !shared_block->state.is_weapon_dropped) {
        int w_id;
        shared_block->state.dropped_weapon = generateRandomWeapon(w_id);
        shared_block->state.is_weapon_dropped = true;
        std::cout << "\n[ARBITER] Enemy dropped: " << shared_block->state.dropped_weapon.getName() << "!\n";
        std::cout << "[ARBITER] (Use PICKUP to claim it, or an enemy will steal it!)\n\n";
    }
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
                // Only position the number of players actually in the game
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

void* stamina_recovery(void* arg){
    auto* shared_block = static_cast<SharedMemoryBlock*>(arg);
    while(true){
        pthread_mutex_lock(&shared_block->global_mutex);
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
        usleep(100000);
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ACTION HANDLERS
// ─────────────────────────────────────────────────────────────────────────────

void handle_player_action(const ActionRequest& request, SharedMemoryBlock* shared_block) {
    int attacker_id = shared_block->hip_mailbox.requesting_entity_id;
    int target_id   = shared_block->hip_mailbox.target_id;

    // --- RULE: If a player takes an action that is NOT Pickup, the enemy steals it! ---
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
            } else {
                std::cout << "[ARBITER] Player " << attacker_id << " tried to pick it up, but inventory swapping failed!\n";
            }
        } else {
            std::cout << "[ARBITER] There is no weapon on the ground to pick up.\n";
        }
        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }

    case Action::STRIKE: {
        // Prevent hitting dead enemies
        if (!shared_block->state.enemies[target_id].isAlive()) {
            std::cout << "[ARBITER] Target already dead. Strike wasted!\n";
            shared_block->state.players[attacker_id].ResetStamina();
            break;
        }

        int damage = shared_block->state.players[attacker_id].getDemage();
        shared_block->state.enemies[target_id].TakeDamage(damage);

        std::cout << "[ARBITER] Player " << attacker_id << " struck Enemy " << target_id
          << " for " << damage << " DMG! (Enemy HP: " << shared_block->state.enemies[target_id].getHp() << ")" << endl;

        // CORRECT PLACEMENT: After damage is taken, replace all old death logic.
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
        // Prevent hitting dead enemies
        if (!shared_block->state.enemies[target_id].isAlive()) {
            std::cout << "[ARBITER] Target already dead. Weapon strike wasted!\n";
            shared_block->state.players[attacker_id].ResetStamina();
            break;
        }

        // CORRECT LOGIC: Actually fetch the damage from the inventory!
        int weapon_id = shared_block->hip_mailbox.weapon_id;
        int weapon_damage = 0;

        if (shared_block->state.players[attacker_id].getInventory().hasWeapon(weapon_id)) {
            // equipped weapons are stored as std::pair<int, Weapon>
            // access the Weapon via .second
            weapon_damage = shared_block->state.players[attacker_id].getInventory().getEquippedWeapons().at(weapon_id).second.getDamage();
        }

        shared_block->state.enemies[target_id].TakeDamage(weapon_damage);
        std::cout << "[ARBITER] Player " << attacker_id << " used weapon " << weapon_id
                  << " on Enemy " << target_id << " for " << weapon_damage << " DMG!\n";

        // CORRECT PLACEMENT: After damage is taken, replacing the duplicate block entirely.
        if (!shared_block->state.enemies[target_id].isAlive()) {
            handle_enemy_death(shared_block, target_id);
        }

        shared_block->state.players[attacker_id].ResetStamina();
        break;
    }

    case Action::SWAP_IN: {
        int backpack_idx = shared_block->hip_mailbox.weapon_id;  // renderer sends index
        int bp_count = shared_block->state.players[attacker_id].getBackpack().getCount();

        if (backpack_idx >= 0 && backpack_idx < bp_count)
        {
            shared_block->state.players[attacker_id].swapInFromBackpack(backpack_idx);
            std::cout << "[ARBITER] Player " << attacker_id
                    << " swapped in backpack slot " << backpack_idx << "\n";
        }
        else
        {
            std::cout << "[ARBITER] SWAP_IN ignored: invalid backpack index "
                    << backpack_idx << " (count=" << bp_count << ")\n";
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
            shared_block->state.players[i].setRollNumber(240607, 7, 7);
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
        // Artifact IDs: Solar Core = 0, Lunar Blade = 1
        bool has_solar = shared_block->state.players[attacker_id].getInventory().hasWeapon(0);
        bool has_lunar = shared_block->state.players[attacker_id].getInventory().hasWeapon(1);

        if (has_solar && has_lunar) {
            std::cout << "\n[ARBITER] *** Player " << attacker_id << " triggered the ULTIMATE ABILITY! ***\n";
            std::cout << "[ARBITER] *** Sending SIGSTOP to ASP. Enemies frozen for 10 seconds! ***\n\n";

            // 1. Freeze the ASP immediately at the OS level
            kill(g_asp_pid, SIGSTOP);

            // 2. Set an OS alarm to fire exactly 10 seconds from now
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

void handle_enemy_action(const ActionRequest& request, SharedMemoryBlock* shared_block) {
    int attacker_id = shared_block->asp_mailbox.requesting_entity_id;
    int target_id   = shared_block->asp_mailbox.target_id;

    if (shared_block->state.is_weapon_dropped) {
        if (shared_block->state.enemies[attacker_id].isAlive()) { // <-- ADDED GUARD
            shared_block->state.enemies[attacker_id].setDemage(shared_block->state.enemies[attacker_id].getDemage() + shared_block->state.dropped_weapon.getDamage());
            std::cout << "[ARBITER] Enemy " << attacker_id << " snatched the dropped weapon on its turn!\n";
        }
        shared_block->state.is_weapon_dropped = false;
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

static int waiting_for(SharedMemoryBlock* sb, int entity_id) {
    if (entity_id >= 0 && entity_id < 4) return sb->state.players_artifact_state[entity_id].waiting_for_artifact_idx;
    if (entity_id >= 10 && entity_id < 19) return sb->state.enemies_artifact_state[entity_id - 10].waiting_for_artifact_idx;
    return -1;
}

static void force_abandon_wait(SharedMemoryBlock* sb, int entity_id) {
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

void* deadlock_detection(void* arg) {
    auto* sb = static_cast<SharedMemoryBlock*>(arg);
    while (true) {
        sleep(10);
        if (!sb->state.game_running) break;

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
            if (entity_C != entity_A) continue;

            std::cout << "[ARBITER][DEADLOCK] *** CIRCULAR WAIT DETECTED ***\n"
                      << "  Entity " << entity_A << " waiting for artifact " << art_X << " (held by " << entity_B << ")\n"
                      << "  Entity " << entity_B << " waiting for artifact " << art_Y << " (held by " << entity_A << ")\n"
                      << "[ARBITER][DEADLOCK] Resolution: forcing entity " << entity_A << " to abandon its wait.\n";
            force_abandon_wait(sb, entity_A);
            deadlock_found = true;
        }
        if (!deadlock_found) std::cout << "[ARBITER][DEADLOCK] Watchdog tick: no deadlock detected.\n";
        pthread_mutex_unlock(&sb->resource_table_mutex);

        pthread_mutex_lock(&sb->global_mutex);
        pthread_cond_broadcast(&sb->turn_condition);
        pthread_mutex_unlock(&sb->global_mutex);
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN KERNEL LOOP
// ─────────────────────────────────────────────────────────────────────────────

bool need_more_enemies(const SharedMemoryBlock* shared_block) {
    for (int i = 0; i < shared_block->state.num_active_enemies; ++i) {
        if (shared_block->state.enemies[i].isAlive()) return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    unsigned int seed = std::hash<std::string>{}("24I0607");
    srand(seed);
    pthread_t stamina_accumalator, deadlock_detector;

    // --- REGISTER THE SIGALRM HANDLER (Section 8) ---
    struct sigaction sa_alrm{};
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_handler = handle_sigalrm;
    sigaction(SIGALRM, &sa_alrm, nullptr);

    // --- REGISTER THE SIGTERM HANDLER (Section 10) ---
    struct sigaction sa_term{};
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa_term, nullptr);

    const char* shm_name = "/game_state_shm";
    shm_unlink(shm_name);
    SharedMem master_shm(shm_name, sizeof(SharedMemoryBlock), true, true);
    SharedMemoryBlock* shared_block = static_cast<SharedMemoryBlock*>(master_shm.getPtr());

    // Pass the pointer to the global variable so the SIGTERM handler can use it
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
        cout<<"Could not launch HIP process. Make sure hip.out is compiled and in the same directory."<<endl;
        return 1;
     }
    pid_t asp_pid = fork();
    if (asp_pid == 0) { execl("./asp.out", "./asp.out", shm_name, nullptr);
        cout<<"Could not launch ASP process. Make sure asp.out is compiled and in the same directory."<<endl;
        return 1;
    }

    // CAPTURE THE ASP PID FOR THE ULTIMATE ABILITY HANDLER
    g_asp_pid = asp_pid;

    pthread_mutex_lock(&shared_block->global_mutex);
    shared_block->state.current_turn_owner_id = -2;
    pthread_cond_broadcast(&shared_block->turn_condition);
    while (!shared_block->hip_mailbox.is_ready) {
        pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
    }
    handle_player_action(shared_block->hip_mailbox, shared_block);

    arbiter.initialize_entities(240607, 7, 7, shared_block->state.level, shared_block->state.sublevel);
    arbiter.initialize_players_positions(shared_block->state.level, shared_block->state.sublevel);

    shared_block->hip_mailbox.is_ready = false;
    pthread_mutex_unlock(&shared_block->global_mutex);

    // --- INJECT ARTIFACTS ---
    shared_block->state.num_artifacts = 3;
    new (&shared_block->state.artifacts[0]) Artifact(0, ArtifactType::SOLAR_CORE, "Solar Core", 95, 10);
    new (&shared_block->state.artifacts[1]) Artifact(1, ArtifactType::LUNAR_BLADE, "Lunar Blade", 90, 10);
    new (&shared_block->state.artifacts[2]) Artifact(2, ArtifactType::ECLIPSE_RELIC, "Eclipse Relic", 0, 5); // Exists = False by default

    pthread_create(&stamina_accumalator, NULL, stamina_recovery, shared_block);
    pthread_create(&deadlock_detector, NULL, deadlock_detection, shared_block);

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
                while (!shared_block->hip_mailbox.is_ready && shared_block->state.game_running) {
                    pthread_cond_wait(&shared_block->turn_condition, &shared_block->global_mutex);
                }
                if (shared_block->state.game_running) {
                    handle_player_action(shared_block->hip_mailbox, shared_block);
                }
            } else {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 3;
                int res = 0;
                while (!shared_block->asp_mailbox.is_ready && res != ETIMEDOUT && shared_block->state.game_running) {
                    res = pthread_cond_timedwait(&shared_block->turn_condition, &shared_block->global_mutex, &ts);
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
                break;
            } else if (shared_block->state.num_active_players == 0) {
                std::cout << "[ARBITER] All players have fallen. YOU LOSE!\n";
                shared_block->state.game_running = false;
                shared_block->state.game_result = false;
                break;
            }

            if(shared_block->state.game_running && need_more_enemies(shared_block)) {
                shared_block->state.hassublevelended = true;
                shared_block->state.sublevel++;
                std::cout << "[ARBITER] Wave cleared! Loading Sublevel " << shared_block->state.sublevel << "...\n";
                arbiter.initialize_entities(240607, 7, 7, shared_block->state.level, shared_block->state.sublevel);
                arbiter.initialize_players_positions(shared_block->state.level, shared_block->state.sublevel);

                for(int i = 0; i < shared_block->state.num_active_players; ++i) shared_block->state.players[i].setStamina(0);
                kill(asp_pid, SIGUSR1);
                shared_block->state.current_turn_owner_id = -3;
                pthread_cond_broadcast(&shared_block->turn_condition);
                shared_block->state.hassublevelended = false;
                std::cout << "[ARBITER] HIP rendering complete. Resuming combat!\n";
            }
            shared_block->state.turn_count++;
        }
        pthread_mutex_unlock(&shared_block->global_mutex);

        // --- SIGNAL DISPATCHER (Processed safely outside the interrupt) ---
        if (g_sigalrm_received) {
            g_sigalrm_received = 0;
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
