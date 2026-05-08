#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <array>
#include <pthread.h>

#define ACTION_LOG_SIZE 10
#define ACTION_MSG_LEN  128

enum class PlayerType {
    CHRONO = 0, FROG = 1, MARLE = 2, MAGUS = 3, NONE = 4
};

enum class Action {
    STRIKE = 0, EXHAUST = 1, USE_WEAPON = 2, SWAP_IN = 3, HEAL = 4, SKIP = 5, SETUP_GAME = 6
};

struct ActionLog {
    char messages[ACTION_LOG_SIZE][ACTION_MSG_LEN];
    int head;
    int count;
};

struct Stamina {
    float current_stamina;
    float max_stamina;
    float recovery_rate;
};

// 1. THE MAILBOX
struct ActionRequest {
    int requesting_entity_id;
    Action action_type;
    int target_id;
    int weapon_id;
    bool is_ready;
    PlayerType types[4];
};

#endif // SHARED_TYPES_H
