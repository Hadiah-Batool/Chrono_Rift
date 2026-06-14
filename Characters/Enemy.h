#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include <iostream>
#include "Characters_header.h"
// Enemy.h — top includes
#include "../DisplayRendering/Animator.h"

enum class EnemyType
{
    ALIEN_ENEMY, // 0
    BEAST_ENEMY, //1
    CYBOT_ENEMY, // 
    DRAGONTANK_ENEMY,
    GIGAGAIA_ENEMY,
    GOBLIN_OGAN_ENEMY,
    IMPS_ENEMY,
    LAVOSCORE_ENEMY,
    MUTANT_ENEMY,
    MOTHERnBRAIN_ENEMY,
    NIZBELN_ENEMY,
    BLOB_ENEMY
};



struct EnemySheetInfo
{
    const char* path;
    int         frameCount;
    FrameData   frames[6];
};

static const EnemySheetInfo sheetData[] =
{
    // 0. ALIEN_ENEMY
    {
        "../Enemies/Alien_enemy.png", 6,
        {
            {   0,  0, 46, 48, 0.28f, 23.f,   48.f },
            {  47,  0, 52, 49, 0.28f, 26.f,   49.f },
            {  97,  0, 55, 49, 0.28f, 27.5f,  49.f },
            {   0, 62, 47, 49, 0.28f, 23.5f,  49.f },
            {  59, 62, 37, 49, 0.28f, 18.5f,  49.f },
            { 114, 62, 40, 49, 0.28f, 20.f,   49.f },
        }
    },
    // 1. BEAST_ENEMY
    {
        "../Enemies/Beasts_Colorful_enemy_movementFrame.png", 3,
        {
            {  0, 0, 27, 37, 0.35f, 28.f, 37.f },
            { 38, 0, 25, 37, 0.35f, 28.f, 37.f },
            { 73, 0, 25, 37, 0.35f, 28.f, 37.f }
        }
    },
    // 2. CYBOT_ENEMY
    {
        "../Enemies/Cybot_enemy_movementFrame.png", 4,
        {
            {   0, 0, 52, 68, 0.35f, 26.f, 68.f },
            {  56, 0, 52, 68, 0.35f, 20.f, 68.f },
            { 114, 0, 52, 68, 0.35f, 20.f, 68.f },
            { 172, 0, 52, 68, 0.35f, 20.f, 68.f }
        }
    },
    // 3. DRAGONTANK_ENEMY
    {
        "../Enemies/DragonTank_enemy_movementFrame.png", 4,
        {
            {   0, 0, 112, 82, 0.35f, 32.f, 64.f },
            { 114, 0, 110, 82, 0.35f, 32.f, 64.f },
            { 227, 0, 110, 82, 0.35f, 32.f, 64.f },
            { 339, 0, 110, 82, 0.35f, 32.f, 64.f }
        }
    },
    // 4. GIGAGAIA_ENEMY
    {
        "../Enemies/GigaGaia_enemy.png", 3,
        {
            {   0, 0, 175, 124, 0.38f, 87.5f, 124.f },
            { 180, 0, 174, 124, 0.38f, 87.f,  124.f },
            { 358, 0, 174, 124, 0.38f, 87.f,  124.f }
        }
    },
    // 5. GOBLIN_OGAN_ENEMY
    {
        "../Enemies/Goblin_Ogan_Green_Movement.png", 6,
        {
            {   0, 0, 40, 34, 0.35f, 20.f,   34.f },
            {  52, 0, 51, 34, 0.35f, 25.5f,  34.f },
            { 106, 0, 46, 34, 0.35f, 23.f,   34.f },
            { 157, 0, 40, 34, 0.35f, 20.f,   34.f },
            { 199, 0, 51, 34, 0.35f, 25.5f,  34.f },
            { 250, 0, 46, 34, 0.35f, 23.f,   34.f }
        }
    },
    // 6. IMPS_ENEMY
    {
        "../Enemies/ImpsColorful_enemy_idle.png", 4,
        {
            {  0, 0, 21, 26, 0.35f, 11.5f, 26.f },
            { 25, 0, 19, 26, 0.35f,  9.5f, 26.f },
            { 47, 0, 21, 26, 0.35f, 11.5f, 26.f },
            { 71, 0, 22, 26, 0.35f, 11.f,  26.f }
        }
    },
    // 7. LAVOSCORE_ENEMY
    {
        "../Enemies/LavosCore_enemy_movementFrame.png", 4,
        {
            {   0, 0, 43, 65, 0.35f, 21.5f, 126.f },
            {  48, 0, 43, 65, 0.35f, 21.5f, 126.f },
            {  94, 0, 63, 65, 0.35f, 31.5f, 126.f },
            { 160, 0, 55, 65, 0.35f, 27.5f, 126.f }
        }
    },
    // 8. MUTANT_ENEMY
    {
        "../Enemies/MutantNMetalMute_enemy_movement_frame.png", 6,
        {
            {   0, 0, 35, 64, 0.35f, 17.5f, 64.f },
            {  42, 0, 35, 64, 0.35f, 17.5f, 64.f },
            {  90, 0, 35, 64, 0.35f, 17.5f, 64.f },
            { 135, 0, 35, 64, 0.35f, 17.5f, 64.f },
            { 182, 0, 35, 64, 0.35f, 17.5f, 64.f },
            { 228, 0, 35, 64, 0.35f, 17.5f, 64.f }
        }
    },
    // 9. MOTHERnBRAIN_ENEMY
    {
        "../Enemies/MotherBrain_enemy_movementFrame.png", 4,
        {
            {   0, 0, 40, 36, 0.35f, 20.f, 36.f },
            {  45, 0, 40, 36, 0.35f, 20.f, 36.f },
            {  90, 0, 40, 36, 0.35f, 20.f, 36.f },
            { 135, 0, 40, 36, 0.35f, 20.f, 36.f }
        }
    },
    // 10. NIZBELN_ENEMY
    {
        "../Enemies/NizbelnNizble2_movementframe.png", 6,
        {
            {   0, 0, 46, 54, 0.35f, 23.f,  54.f },
            {  45, 0, 45, 54, 0.35f, 22.5f, 54.f },
            { 101, 0, 45, 54, 0.35f, 22.5f, 54.f },
            { 149, 0, 55, 54, 0.35f, 27.5f, 54.f },
            { 209, 0, 61, 54, 0.35f, 35.5f, 54.f },
            { 272, 0, 45, 54, 0.35f, 22.5f, 54.f }
        }
    },
    // 11. BLOB_ENEMY
    {
        "../Enemies/Blob.png", 5,
        {
            {   0, 0, 38, 34, 0.35f, 19.f, 34.f },
            {  38, 0, 38, 34, 0.35f, 15.f, 15.f },
            {  76, 0, 38, 34, 0.35f, 15.f, 15.f },
            { 114, 0, 38, 34, 0.35f, 15.f, 15.f },
            { 152, 0, 38, 34, 0.35f, 15.f, 15.f }
        }
    }
};


class Enemy : public Character
{
private:
    EnemyType enemyType;
    int       enemyId;
    char      name[64];

public:
    Enemy(int id, EnemyType type)
        : Character(CharacterType::ENEMY),
          enemyType(type),
          enemyId(id)
    {
        name[0] = '\0';
    }

    const char* getName() const { return name; }

    // ── Roll stats ────────────────────────────────────────────
    virtual void initRollStats(float speedOverride = -1.f) override
    {
        srand(rollFull + enemyId * 7);

        maxHp      = rollLastTwo + 50 + (rand() % 151); // Enemy HP: Last 2 digits of Roll No + random(50-200)
        Hp         = maxHp;
        demage     = rollLastDig + 10; // Enemy Damage: Second last digit of Roll No + 10
        speed      = static_cast<float>(10 + (rand() % 21)); // Enemy Speed: Random(10-30)
        MaxStamina = 150; // Enemy Max Stamina: 150
        stamina    = 0;
        alive      = true;
        myTurn     = false;
        stunned    = false;
        stunEndTem = 0;
    }

    // ── Position + scale init (called by arbiter after initRollStats) ─────
    bool InitAllProperties(float spawnX, float spawnY)
    {
        std::cout << "[ENEMY] Spawning id=" << enemyId
                  << " type=" << static_cast<int>(enemyType)
                  << " at (" << spawnX << ", " << spawnY << ")\n";

        switch (enemyType)
        {
            //1. 
            case EnemyType::ALIEN_ENEMY:
                strncpy(name, "Alien",      sizeof(name) - 1);
                setScale(3.0f, 3.0f);
                break;
            //2. 
            case EnemyType::BEAST_ENEMY:
                strncpy(name, "Beast",      sizeof(name) - 1);
                setScale(3.0f, 3.0f);
                break;
               //3.  
            case EnemyType::CYBOT_ENEMY:
                strncpy(name, "Cybot",      sizeof(name) - 1);
                setScale(3.0f, 3.0f);
                break;
                //4.
            case EnemyType::DRAGONTANK_ENEMY:
                strncpy(name, "Dragontank", sizeof(name) - 1);
                setScale(0.5f, 0.5f);
                break;
                //5. 
            case EnemyType::GIGAGAIA_ENEMY:
                strncpy(name, "Gigagaia",   sizeof(name) - 1);
                setScale(4.0f, 4.0f);
                break;
                //6. 
            case EnemyType::GOBLIN_OGAN_ENEMY:
                strncpy(name, "GoblinOgan", sizeof(name) - 1);
                setScale(3.0f, 3.0f);
                break;
                //7. 
            case EnemyType::IMPS_ENEMY:
                strncpy(name, "Imps",       sizeof(name) - 1);
                setScale(3.5f, 3.5f);
                break;
                //8. 
            case EnemyType::LAVOSCORE_ENEMY:
                strncpy(name, "LavosCore",  sizeof(name) - 1);
                setScale(2.5f, 2.5f);
                break;
                //9.
            case EnemyType::MOTHERnBRAIN_ENEMY:
                strncpy(name, "MotherBrain",sizeof(name) - 1);
                setScale(5.5f, 5.5f);
                break;
                //10. 
            case EnemyType::MUTANT_ENEMY:
                strncpy(name, "Mutant",     sizeof(name) - 1);
                setScale(2.5f, 2.5f);
                break;
                //10. 
            case EnemyType::NIZBELN_ENEMY:
                strncpy(name, "Nizbeln",    sizeof(name) - 1);
                setScale(4.0f, 4.0f);
                break;
                //11. 

            case EnemyType::BLOB_ENEMY:
                strncpy(name, "Blob",       sizeof(name) - 1);
                setScale(4.0f, 4.0f);
                break;
            default:
                strncpy(name, "Enemy",      sizeof(name) - 1);
                setScale(1.0f, 1.0f);
                break;
        }

        name[sizeof(name) - 1] = '\0';
        setXPos(spawnX);
        setYPos(spawnY);
        return true;
    }

    // ── AI decision ──────────────────────────────────────────
    enum class EnemyActionType { STRIKE, SKIP };

    struct EnemyAction
    {
        EnemyActionType type;
        int             targetPlayerId;
    };

    EnemyAction decideAction(const int* alivePlayers, int count)
    {
        if (count == 0 || (rand() % 10 == 0))
        {
            depleteStamina(false);
            return { EnemyActionType::SKIP, -1 };
        }
        depleteStamina(true);
        return { EnemyActionType::STRIKE, alivePlayers[rand() % count] };
    }

    virtual void DoAction() override {}

    // ── Getters ──────────────────────────────────────────────
    int       getEnemyId()   const { return enemyId; }
    EnemyType getEnemyType() const { return enemyType; }
};
