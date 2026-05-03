#pragma once
#include <SFML/Graphics.hpp>
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
    bool  alive;
    bool  myTurn;
    bool  stunned;
    time_t   stunEndTem;

    // ── Movement ─────────────────────────────────────────────
    float        speed;
    sf::Vector2f pos;

    // ── Sprite ───────────────────────────────────────────────
    sf::Sprite  sprite;
    sf::Texture texture;

    // ── Roll number fields (shared by Player and Enemy) ──────
    int rollFull;       // full roll number  → seed + HP base for player
    int rollLastDig;    // last digit        → damage base  (both)
    int rollLastTwo;    // last two digits   → HP base for enemy

public:
    Character(CharacterType type)
        : characterType(type),
          Hp(0), maxHp(0), demage(0),
          stamina(0), MaxStamina(0),
          alive(false), myTurn(false),
          stunned(false), stunEndTem(0),
          speed(0.f),
          rollFull(0), rollLastDig(0), rollLastTwo(0)
    {}

    virtual ~Character() {}

    // ── Roll number setup ─────────────────────────────────────
    // Call this once before InitAllProperties.
    // Player:  pass full roll, last digit, last two digits
    // Enemy:   same — it only uses lastTwo + lastDig
    void setRollNumber(int full, int lastDig, int lastTwo)
    {
        rollFull    = full;
        rollLastDig = lastDig;
        rollLastTwo = lastTwo;
    }

    // Override in Player and Enemy to set HP/damage/speed
    // using the stored roll fields
    virtual void initRollStats(float speedOverride = -1.f) = 0;

    // ── Damage / health ───────────────────────────────────────
    void TakeDamage(int amount)
    {
        Hp -= amount;
        if (Hp <= 0)
        {
            Hp    = 0;
            alive = false;
        }
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
        // true  → action/strike → 0
        // false → skip          → 50%
        stamina = fullDeplete ? 0 : MaxStamina / 2;
    }

    void ResetStamina() { stamina = 0; }

    bool CanAct() const { return isReadyToAct(); }

    // ── Stun ─────────────────────────────────────────────────
    void applyStun(time_t stun)
    {
        stunned    = true;
        stunEndTem = 3;             // 3 seconds per spec
        if (stamina >= MaxStamina)
            stamina = 0;            // lose turn if stamina was full
    }

    void clearStun()
    {
        stunned    = false;
        stunEndTem = 0;
        // stamina preserved — spec says resume from exact point
    }

    bool amStunned() const { return stunned; }
    bool amAlive()   const { return alive; }
    float getStaminaRecoveryRate() const { return speed; }

    // ── Sprite helpers ────────────────────────────────────────
    bool loadTexture(const std::string& path)
    {
        if (texture.loadFromFile(path))
        {
            sprite.setTexture(texture);
            return true;
        }
        return false;
    }

    void SetScaleSprite(float scaleX, float scaleY)
    {
        sprite.setScale(scaleX, scaleY);
    }

    void SetOriginSprite(float originX, float originY)
    {
        sprite.setOrigin(originX, originY);
    }

    void draw(sf::RenderWindow& window)
    {
        if (alive)
        {
            sprite.setPosition(pos);
            window.draw(sprite);
        }
    }

    // ── Pure virtual action hook ──────────────────────────────
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
            stunned = v;
              stunEndTem = turn_num + 3; /* 3 turns per spec */
        }

    int   getStunEndTem()  const { return stunEndTem; }
    void  setStunEndTem(int v)   { stunEndTem = v; }
    float getSpeed()       const { return speed; }
    void  setSpeed(float v)      { speed = v; }
    float getXPos()        const { return pos.x; }
    void  setXPos(float v)       { pos.x = v; }
    float getYPos()        const { return pos.y; }
    void  setYPos(float v)       { pos.y = v; }
    int   getRollFull()    const { return rollFull; }
    int   getRollLastDig() const { return rollLastDig; }
    int   getRollLastTwo() const { return rollLastTwo; }
<<<<<<< Updated upstream
};
=======
    float getScaleX()       const { return sprite.getScale().x; }
    float getScaleY()       const { return sprite.getScale().y; }
};
>>>>>>> Stashed changes
