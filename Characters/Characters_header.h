#pragma once
#include <cstdlib>
#include <string>

enum class CharacterType
{
    ENEMY,
    PLAYER
};

enum class ActiveStatus
{
    ACTIVE,
    STUNNED
};

class Character
{
protected:
    CharacterType characterType;

    // ── Core stats ───────────────────────────────────────────
    int   Hp;
    int   maxHp;
    int   demage;
    int   stamina;
    int   MaxStamina;

    // ── Status ───────────────────────────────────────────────
    bool   alive;
    bool   myTurn;
    bool   stunned;
    time_t stunEndTem;

    // ── Movement / position ──────────────────────────────────
    float speed;
    float posX;
    float posY;

    // ── Scale (read by EnemyRenderer each frame) ─────────────
    float scaleX;
    float scaleY;

    // ── Roll number fields ───────────────────────────────────
    int rollFull;
    int rollLastDig;
    int rollLastTwo;

public:
    Character(CharacterType type)
        : characterType(type),
          Hp(0), maxHp(0), demage(0),
          stamina(0), MaxStamina(0),
          alive(false), myTurn(false),
          stunned(false), stunEndTem(0),
          speed(0.f),
          posX(0.f), posY(0.f),
          scaleX(1.f), scaleY(1.f),
          rollFull(0), rollLastDig(0), rollLastTwo(0)
    {}

    virtual ~Character() {}

    // ── Roll number setup ─────────────────────────────────────
    void setRollNumber(int full, int lastDig, int lastTwo)
    {
        rollFull    = full;
        rollLastDig = lastDig;
        rollLastTwo = lastTwo;
    }

    virtual void initRollStats(float speedOverride = -1.f) = 0;

    // ── Damage / health ───────────────────────────────────────
    void TakeDamage(int amount)
    {
        Hp -= amount;
        if (Hp <= 0) { Hp = 0; alive = false; }
    }

    void RegainHealth(int amount)
    {
        Hp += amount;
        if (Hp > maxHp) Hp = maxHp;
    }

    // ── Stamina ───────────────────────────────────────────────
    void tickStamina()
    {
        if (!alive || stunned) return;
        stamina += static_cast<int>(speed);
        if (stamina > MaxStamina) stamina = MaxStamina;
    }

    bool isReadyToAct() const
    {
        return alive && !stunned && stamina >= MaxStamina;
    }

    void depleteStamina(bool fullDeplete)
    {
        stamina = fullDeplete ? 0 : MaxStamina / 2;
    }

    void ResetStamina() { stamina = 0; }
    bool CanAct()       const { return isReadyToAct(); }

    // ── Stun ──────────────────────────────────────────────────
    void applyStun(time_t stun)
    {
        stunned    = true;
        stunEndTem = 3;
        if (stamina >= MaxStamina) stamina = 0;
    }

    void clearStun()
    {
        stunned    = false;
        stunEndTem = 0;
    }

    bool amStunned() const { return stunned; }
    bool amAlive()   const { return alive; }
    float getStaminaRecoveryRate() const { return speed; }

    // ── Pure virtual ──────────────────────────────────────────
    virtual void DoAction() = 0;

    // ── Getters / Setters ─────────────────────────────────────
    int   getHp()          const { return Hp; }
    void  setHp(int hp)          { Hp = hp; }
    int   getMaxHp()       const { return maxHp; }
    void  setMaxHp(int v)        { maxHp = v; }
    int   getDemage()      const { return demage; }
    void  setDemage(int v)       { demage = v; }
    int   getStamina()     const { return stamina; }
    void  setStamina(int v)      { stamina = v; }
    int   getMaxStamina()  const { return MaxStamina; }
    void  setMaxStamina(int v)   { MaxStamina = v; }
    bool  isAlive()        const { return alive; }
    void  setAlive(bool v)       { alive = v; }
    bool  isMyTurn()       const { return myTurn; }
    void  setMyTurn(bool v)      { myTurn = v; }
    bool  isStunned()      const { return stunned; }
    void  setStunned(bool v, int turn_num)
    {
        stunned    = v;
        stunEndTem = turn_num + 3;
    }

    int   getStunEndTem()  const { return stunEndTem; }
    void  setStunEndTem(int v)   { stunEndTem = v; }
    float getSpeed()       const { return speed; }
    void  setSpeed(float v)      { speed = v; }

    // ── Position ──────────────────────────────────────────────
    float getXPos()        const { return posX; }
    void  setXPos(float v)       { posX = v; }
    float getYPos()        const { return posY; }
    void  setYPos(float v)       { posY = v; }

    // ── Scale (set by InitAllProperties, read by EnemyRenderer) ──
    float getScaleX()      const { return scaleX; }
    float getScaleY()      const { return scaleY; }
    void  setScale(float x, float y) { scaleX = x; scaleY = y; }

    // ── Roll ──────────────────────────────────────────────────
    int getRollFull()      const { return rollFull; }
    int getRollLastDig()   const { return rollLastDig; }
    int getRollLastTwo()   const { return rollLastTwo; }
};
