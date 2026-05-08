#pragma once
#include <cstring>
#include <string>
#include <SFML/Graphics.hpp>

enum class WeaponType
{
    IRON_HALBERD,
    VENOM_DAGGER,
    THUNDERSTAFF,
    OBSIDIAN_AXE,
    FROSTBOW,
    SPLINTER_STICK,
    ARTIFACT
};

class Weapon
{
private:
    static const int NAME_LEN = 32;

    int        weaponId;
    WeaponType type;
    char       name[NAME_LEN];   // fixed — no heap
    int        slotSize;
    int        damage;
    bool       canBeUsed;
    // imagePath, weaponTexture, weaponSprite removed —
    // renderer loads textures itself by weapon name, never needs these in shm

public:
    Weapon()
        : weaponId(-1), type(WeaponType::SPLINTER_STICK),
          slotSize(0), damage(0), canBeUsed(true)
    {
        name[0] = '\0';
    }

    Weapon(int id, WeaponType t, const std::string& n,
           int slotSize, int damage, bool canBeUsed = true)
        : weaponId(id), type(t),
          slotSize(slotSize), damage(damage), canBeUsed(canBeUsed)
    {
        strncpy(name, n.c_str(), NAME_LEN - 1);
        name[NAME_LEN - 1] = '\0';
    }

    int         getWeaponId()  const { return weaponId; }
    WeaponType  getType()      const { return type; }
    std::string getName()      const { return std::string(name); }
    int         getSlotSize()  const { return slotSize; }
    int         getDamage()    const { return damage; }
    bool        isUsable()     const { return canBeUsed; }
    void        setUsable(bool v)    { canBeUsed = v; }
    
};


enum class ArtifactType  { SOLAR_CORE, LUNAR_BLADE, ECLIPSE_RELIC };
enum class PassiveEffect { DAMAGE_BOOST, STUN_ON_HIT, ULTIMATE_ENABLE };

class Artifact : public Weapon
{
private:
    ArtifactType  artifactType;
    PassiveEffect passiveEffect;
    int           holder_id;
    bool          exists;

public:
    Artifact() : Weapon(), artifactType(ArtifactType::SOLAR_CORE),
                 passiveEffect(PassiveEffect::DAMAGE_BOOST),
                 holder_id(-1), exists(false) {}

    Artifact(int weaponID, ArtifactType type, const std::string& name,
             int damage, int slotSize)
        : Weapon(weaponID, WeaponType::ARTIFACT, name, slotSize, damage),
          artifactType(type), holder_id(-1), exists(true)
    {
        switch (type)
        {
            case ArtifactType::SOLAR_CORE:
                passiveEffect = PassiveEffect::DAMAGE_BOOST;    break;
            case ArtifactType::LUNAR_BLADE:
                passiveEffect = PassiveEffect::STUN_ON_HIT;     break;
            case ArtifactType::ECLIPSE_RELIC:
                passiveEffect = PassiveEffect::ULTIMATE_ENABLE;
                exists = false;
                break;
        }
    }

    int applyPassive(int baseDamage, Character* target = nullptr)
    {
        switch (passiveEffect)
        {
            case PassiveEffect::DAMAGE_BOOST:
                return (int)(baseDamage * 1.25f);
            case PassiveEffect::STUN_ON_HIT:
                if (target && (rand() % 100) < 30)
                    target->setStunned(true, 2);
                return baseDamage;
            case PassiveEffect::ULTIMATE_ENABLE:
                return baseDamage;
        }
        return baseDamage;
    }

    bool isHeld()          const { return holder_id != -1; }
    int  getHolder()       const { return holder_id; }
    void setHolder(int id)       { holder_id = id; }
    void release()               { holder_id = -1; }

    bool         isAvailable()     const { return exists && !isHeld(); }
    void         introduce()             { exists = true; }
    ArtifactType getArtifactType() const { return artifactType; }
    PassiveEffect getPassiveEffect() const { return passiveEffect; }
};
