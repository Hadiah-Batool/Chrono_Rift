#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include "Characters.h"
#include"../DisplayRendering/Animator.h"
#include <SFML/Graphics.hpp>


enum class EnemyType
{
    ALIEN_ENEMY,
    BEAST_ENEMY,
    CYBOT_ENEMY,
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
    FrameData   frames[6];   //  per-frame data, no vectors
};

static const EnemySheetInfo sheetData[] =
{
    //ALIEN_ENEMY — 6 frames, uniform
    {   
        "../Enemies/Alien_enemy.png", 6,
        {
            {   0, 0, 46, 48, 0.28f, 23.f, 48.f },
            {  47, 0, 52, 49, 0.28f, 26.f, 49.f },
            {  97, 0, 55, 49, 0.28f, 27.5f, 49.f },
            { 0, 62, 47, 49, 0.28f, 23.5f, 49.f },
            { 59, 62, 37, 49, 0.28f, 18.5f, 49.f },
            { 114, 62, 40, 49, 0.28f, 20.f, 49.f },
        }
    },
    {   //1.  BEAST_ENEMY — 6 frames, uniform
        "../Enemies/Beasts_Colorful_enemy_movementFrame.png", 3,
        {
            {   0, 0, 27, 37, 0.35f, 28.f, 37.f },
            {  38, 0, 25, 37, 0.35f, 28.f, 37.f },
            { 73, 0, 25, 37, 0.35f, 28.f, 37.f }
        }
    },
    {
        // 2. CYBOT_ENEMY — 4 frames, uniform
        "../Enemies/Cybot_enemy_movementFrame.png", 4,
        {
            {   0, 0, 52, 68, 0.35f, 26.f, 68.f },
            {  56, 0, 52, 68, 0.35f, 20.f, 68.f },
            {  114, 0, 52, 68, 0.35f, 20.f, 68.f },
            { 172, 0, 52, 68, 0.35f, 20.f, 68.f }
        }

    }, 
    //3. DRAGON_TANK

    {
        "../Enemies/DragonTank_enemy_movementFrame.png", 4, 
        {
            {   0, 0, 112, 82, 0.35f, 32.f, 64.f },
            {  114, 0, 110, 82, 0.35f, 32.f, 64.f },
            { 227, 0, 110, 82, 0.35f, 32.f, 64.f },
            { 339, 0, 110, 82, 0.35f, 32.f, 64.f }
        }

    },

    //4. GIGA GIA
    {    "../Enemies/GigaGaia_enemy.png", 3, 
    
        {
            {   0, 0, 175, 124, 0.38f, 87.5f, 124.f },
            {  180, 0, 174, 124, 0.38f, 87.f, 124.f },
            {  358, 0, 174, 124, 0.38f, 87.f, 124.f }

        }
    }, 

    //5. GOBLIN_OGAN_ENEMY
    {
        "../Enemies/Goblin_Ogan_Green_Movement.png", 6, 
        {
            {0, 0, 40, 34, 0.35f, 20.f, 34.f },
            { 52, 0, 51, 34, 0.35f, 25.5f, 34.f },
            { 106, 0, 46, 34, 0.35f, 23.f, 34.f },
            {157, 0, 40, 34, 0.35f, 20.f, 34.f },
            {199, 0, 51, 34, 0.35f, 25.5f, 34.f },
            {250, 0, 46, 34, 0.35f, 23.f, 34.f }

        }
    },
    //6. IMPS ENEMY
    {
        "../Enemies/ImpsColorful_enemy_idle.png", 4, 
        {
            {0, 0, 21, 26, 0.35f, 11.5f, 26.f },
            { 25, 0, 19, 26, 0.35f, 9.5f, 26.f },
            { 47, 0, 21, 26, 0.35f, 11.5f, 26.f },
            {71, 0, 22, 26, 0.35f, 11.f, 26.f }
        }
    }, 
    //7. LAVOS CORE
    {
          "../Enemies/LavosCore_enemy_movementFrame.png", 4, 
        {
                {0, 0, 43, 65, 0.35f, 21.5f, 126.f },
                { 48, 0,43, 65, 0.35f, 21.5f, 126.f },
                {94, 0, 63, 65, 0.35f, 31.5f, 126.f },
                {160, 0, 55, 65, 0.35f, 27.5f, 126.f }
            
        }
    },

    //8. MUTANT_ENEMY
    {
        "../Enemies/MutantNMetalMute_enemy_movement_frame.png", 6, 
        {
                {0, 0, 35, 64, 0.35f, 17.5f, 64.f }, 
                {42, 0,  35, 64,0.35f, 17.5f, 64.f }, 
                {90, 0,  35, 64, 0.35f, 17.5f, 64.f }, 
                {135, 0,  35, 64, 0.35f, 17.5f, 64.f }, 
                {182, 0,  35, 64, 0.35f, 17.5f, 64.f }, 
                {228, 0,  35, 64, 0.35f, 17.5f, 64.f }


        }
    }, 
    //9. MOTHER BRAIN
    {
        "../Enemies/MotherBrain_enemy_movementFrame.png", 4, 
        {
                {0, 0, 40, 36, 0.35f, 20.f, 36.f }, 
                {45, 0, 40, 36, 0.35f, 20.f, 36.f }, 
                {90, 0, 40, 36, 0.35f, 20.f, 36.f }, 
                {135, 0, 40, 36, 0.35f, 20.f, 36.f }
        }
    }, 
    //10. NIZBELN
    {
        "../Enemies/NizbelnNizble2_movementframe.png", 6, 
        {
                {0, 0, 46, 54, 0.35f, 23.f, 54.f }, 
                {45, 0, 45, 54, 0.35f, 22.5f, 54.f }, 
                {101, 0, 45, 54, 0.35f, 22.5f, 54.f }, 
                {149, 0, 55, 54, 0.35f, 27.5f, 54.f }, 
                {209, 0, 61, 54, 0.35f, 35.5f, 54.f }, 
                {272, 0, 45, 54, 0.35f, 22.5f, 54.f }
        }
    },
    //11. BLOB ENEMY
    {
        "../Enemies/Blob.png", 5, 
        {
            {0, 0, 38, 34, 0.35f, 19.f, 34.f }, 
            {38, 0, 38, 34, 0.35f, 15.f, 15.f }, 
            {76, 0, 38, 34, 0.35f, 15.f, 15.f }, 
            {114, 0, 38, 34, 0.35f, 15.f, 15.f }, 
            {152, 0, 38, 34, 0.35f, 15.f, 15.f }, 
  
        }


        }
    



};


class Enemy : public Character
{
private:
    EnemyType   enemyType;
    int         enemyId;
    char name[64];

    sf::Sprite EnemySprite;
    Animation spawnAnimation;



public:
    Enemy(int id, EnemyType type)
        : Character(CharacterType::ENEMY),
          enemyId(id),
          enemyType(type)
    {}
    const char* getName()      const { return name; }


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
        cout<<"Setting position: ("<<spawnX<<", "<<spawnY<<") for enemy "<<enemyId<<" of type "<<static_cast<int>(enemyType)<<endl;
        switch (enemyType)
        {
            case EnemyType::ALIEN_ENEMY:
                strncpy(name, "Alien", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null
                SetOriginSprite(16.0f, 32.0f);
                SetScaleSprite(4.0f, 4.0f);
                

                break;
            case EnemyType::BEAST_ENEMY  :
                strncpy(name, "Beast", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null
                SetOriginSprite(28.0f, 34.0f);
                SetScaleSprite(4.0f, 4.0f);
                break;
            case EnemyType::CYBOT_ENEMY:
                strncpy(name, "Cybot", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null
                SetOriginSprite(28.f, 34.0f);
                SetScaleSprite(3.0f, 3.0f);
                break;
            case EnemyType::DRAGONTANK_ENEMY:
                strncpy(name, "Dragontank", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                SetScaleSprite( 2.5f,    2.5f);
                break;
            case EnemyType::GIGAGAIA_ENEMY:
                strncpy(name, "Gigagaia", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                 SetOriginSprite(16.0f, 32.0f);
                 SetScaleSprite(4.0f, 4.0f);
                break;
            case EnemyType::GOBLIN_OGAN_ENEMY:
                strncpy(name, "GoblinOgan", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null    
                // SetOriginSprite(18.0f, 34.0f);
                SetScaleSprite(4.0f, 4.0f);
                break;  
            case EnemyType::IMPS_ENEMY:
                strncpy(name, "Imps", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null
                    SetOriginSprite(22.0f, 23.0f);
                SetScaleSprite(5.5f, 5.5f);
                break;
            case EnemyType::LAVOSCORE_ENEMY:    
                strncpy(name, "LavosCore", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null
                SetOriginSprite(43.0f, 63.0f);
                SetScaleSprite(3.5f, 3.5f);
                break;
            case EnemyType::MOTHERnBRAIN_ENEMY:
                strncpy(name, "MotherBrain", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null
                SetOriginSprite(20.0f, 36.0f);
                SetScaleSprite(5.5f, 5.5f);
                break;

            case EnemyType::MUTANT_ENEMY:
                strncpy(name, "Mutant", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination    
                SetOriginSprite(16.0f, 30.0f);
                SetScaleSprite(3.5f, 3.5f); 

                break;
            case EnemyType::NIZBELN_ENEMY:
                strncpy(name, "Nizbeln", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                SetOriginSprite(44.0f, 55.0f);
                SetScaleSprite(4.f,4.f);
                break;

            case EnemyType::BLOB_ENEMY:
                strncpy(name, "Blob", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                SetOriginSprite(19.0f, 34.0f);
                SetScaleSprite(4.f, 4.f);
                break;

            default:
                strncpy(name, "Enemy", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
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
    void initAnimation(const EnemySheetInfo& info)
{
    spawnAnimation.loadTexture(info.path);
    spawnAnimation.setLooping(true);

    for (int i = 0; i < info.frameCount; i++)
    {
        const FrameData& f = info.frames[i];
        spawnAnimation.addFrame(f.x, f.y, f.w, f.h, f.duration, f.centerX, f.centerY);
    }
}
void updateAnimation(float dt)
{
    if (!alive) return;

    spawnAnimation.update(dt);
    spawnAnimation.applyToSprite(EnemySprite);

    // apply scale from InitAllProperties
    EnemySprite.setScale(getScaleX(), getScaleY());
    EnemySprite.setPosition(getXPos(), getYPos());
}

void draw(sf::RenderWindow& window)
{
    if (!alive) return;
    window.draw(EnemySprite);
}



    
};