#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include "Characters.h"

enum class EnemyType
{
    ALIEN_ENEMY,
    BEAST_ENEMY,
    CYBOT_ENEMY,
    DRAGONTANK_ENEMY,
    GIGAGAIA_ENEMY,
    IMPS_ENEMY,
    LAVOSCORE_ENEMY,
    MOTHERnBRAIN_ENEMY,
    MUTANT_ENEMY,
    NIZBELN_ENEMY,
    RATnGERMLIN_ENEMY,

};

class Enemy : public Character
{
private:
    EnemyType   enemyType;
    int         enemyId;
    std::string name;

public:
    Enemy(int id, EnemyType type)
        : Character(CharacterType::ENEMY),
          enemyId(id),
          enemyType(type)
    {}

    // ── Roll stats ────────────────────────────────────────────
    // Call setRollNumber() before this
    // speedOverride unused for enemy (speed is fully random) — kept to
    // satisfy the pure virtual signature from Character
    virtual void initRollStats(float speedOverride = -1.f) override
    {
        srand(rollFull + enemyId * 7); // unique seed per enemy

        // HP:     last two digits + rand(50-200)  per spec
        maxHp  = rollLastTwo + 50 + (rand() % 151);
        Hp     = maxHp;

        // Damage: second last digit + 10  per spec
        demage = rollLastDig + 10;

        // Speed:  rand(10-30)  per spec
        speed  = static_cast<float>(10 + (rand() % 21));

        MaxStamina = 150; // fixed per spec
        stamina    = 0;
        alive      = true;
        myTurn     = false;
        stunned    = false;
        stunEndTem = 0;
    }

    // ── Sprite + position init ────────────────────────────────
    bool InitAllProperties(float spawnX, float spawnY)
    {
        switch (enemyType)
        {
            case EnemyType::ALIEN_ENEMY:
                name = "Alien-" + std::to_string(enemyId);
                SetOriginSprite(16.0f, 32.0f);
                SetScaleSprite(4.0f, 4.0f);
                break;
            case EnemyType::BEAST_ENEMY  :
                name = "Brute-" + std::to_string(enemyId);
                SetOriginSprite(18.0f, 36.0f);
                SetScaleSprite(5.0f, 5.0f);
                break;
            case EnemyType::CYBOT_ENEMY:
                name = "Cybot-" + std::to_string(enemyId);
                SetOriginSprite(16.0f, 30.0f);
                SetScaleSprite(4.5f, 4.5f);
                break;
            case EnemyType::DRAGONTANK_ENEMY:
                name = "Dragontank-" + std::to_string(enemyId);
                SetOriginSprite(20.0f, 38.0f);

                SetScaleSprite(5.5f, 5.5f);
                break;
            case EnemyType::GIGAGAIA_ENEMY:
                name = "Gigagaia-" + std::to_string(enemyId);
                SetOriginSprite(18.0f, 34.0f);
                SetScaleSprite(5.0f, 5.0f);
                break;  
            case EnemyType::IMPS_ENEMY:
                name = "Imp-" + std::to_string(enemyId);
                SetOriginSprite(17.0f, 32.0f);
                SetScaleSprite(3.8f, 4.2f);
                break;
            case EnemyType::LAVOSCORE_ENEMY:    
                name = "Lavoscore-" + std::to_string(enemyId);
                SetOriginSprite(22.0f, 40.0f);
                SetScaleSprite(6.0f, 6.0f);
                break;
            case EnemyType::MOTHERnBRAIN_ENEMY:
                name = "MothernBrain-" + std::to_string(enemyId);
                SetOriginSprite(20.0f, 36.0f);
                SetScaleSprite(5.5f, 5.5f);
                break;
            case EnemyType::MUTANT_ENEMY:
                name = "Mutant-" + std::to_string(enemyId);
                SetOriginSprite(16.0f, 30.0f);
                SetScaleSprite(4.5f, 4.5f); 
                break;
            case EnemyType::NIZBELN_ENEMY:
                name = "Nizbeln-" + std::to_string(enemyId);
                SetOriginSprite(18.0f, 32.0f);
                SetScaleSprite(5.0f, 5.0f);
                break;
            case EnemyType::RATnGERMLIN_ENEMY:
                name = "RatnGremlin-" + std::to_string(enemyId);
                SetOriginSprite(14.0f, 28.0f);
                SetScaleSprite(3.5f, 3.5f);
                break;
            default:
                name = "Enemy-" + std::to_string(enemyId);  
        }
        setXPos(spawnX);
        setYPos(spawnY);
        return true;
    }

    // ── AI decision ──────────────────────────────────────────
    enum class EnemyActionType { STRIKE, SKIP };

    struct EnemyAction
    {
        EnemyActionType type;
        int             targetPlayerId; // -1 on SKIP
    };

    // alivePlayers: array of alive player ids, count: how many
    EnemyAction decideAction(const int* alivePlayers, int count)
    {
        // 10% chance to skip, or no targets
        if (count == 0 || (rand() % 10 == 0))
        {
            depleteStamina(false); // skip → 50%
            return {EnemyActionType::SKIP, -1};
        }

        depleteStamina(true); // strike → 0
        return {EnemyActionType::STRIKE, alivePlayers[rand() % count]};
    }

    // ── DoAction satisfies pure virtual; thread calls decideAction ──
    virtual void DoAction() override {}

    // ── Getters ──────────────────────────────────────────────
    int         getEnemyId()   const { return enemyId; }
    EnemyType   getEnemyType() const { return enemyType; }
    std::string getName()      const { return name; }
};