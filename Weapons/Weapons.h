#pragma once
#include <string>
#include <SFML/Graphics.hpp>

using namespace std;
using namespace sf;
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
//as enum gives shi to stuff it'll be foin hopefully
class Weapon
{
private:
    int weaponId;          // unique per instance
    WeaponType type;
    std::string name;
    int slotSize;
    int damage;
    bool canBeUsed;
    std ::string imagePath;
    sf::Texture weaponTexture;
    sf::Sprite weaponSprite;
      


public:
    Weapon()
        : weaponId(-1), type(WeaponType::SPLINTER_STICK), name(""), slotSize(0), damage(0), canBeUsed(true) {}

    Weapon(int id, WeaponType type, const std::string& name, int slotSize, int damage, bool canBeUsed = true)
        : weaponId(id), type(type), name(name), slotSize(slotSize), damage(damage), canBeUsed(canBeUsed) {}

    int getWeaponId() const { return weaponId; }
    WeaponType getType() const { return type; }
    std::string getName() const { return name; }
    int getSlotSize() const { return slotSize; }
    int getDamage() const { return damage; }
    bool isUsable() const { return canBeUsed; }

    void setUsable(bool value) { canBeUsed = value; }
};






enum class ArtifactType {
    SOLAR_CORE,
    LUNAR_BLADE,
    ECLIPSE_RELIC
};

enum class PassiveEffect {
    DAMAGE_BOOST,       // Solar Core  — boosts attack damage
    STUN_ON_HIT,        // Lunar Blade — chance to stun target
    ULTIMATE_ENABLE     // Eclipse Relic — unlocks ultimate when paired
};

class Artifact : public Weapon
{
private:
    ArtifactType  artifactType;
    PassiveEffect passiveEffect;
    int           holder_id;      // -1 if no one holds it
    bool          exists;         // Eclipse Relic starts as false

public:
    Artifact(int weaponID, ArtifactType type, const std::string& name,
             int damage, int slotSize)
        : Weapon(weaponID  , WeaponType::ARTIFACT, name, slotSize, damage),
          artifactType(type),
          holder_id(-1),
          exists(true)
    {
        // Assign passive based on type
        switch (type)
        {
            case ArtifactType::SOLAR_CORE:
                passiveEffect = PassiveEffect::DAMAGE_BOOST;
                break;
            case ArtifactType::LUNAR_BLADE:
                passiveEffect = PassiveEffect::STUN_ON_HIT;
                break;
            case ArtifactType::ECLIPSE_RELIC:
                passiveEffect = PassiveEffect::ULTIMATE_ENABLE;
                exists = false;   // not in game until introduced
                break;
        }
    }

    // ── Passive application ───────────────────────────────────
    // Call this in Arbiter after every attack that uses this artifact
    int applyPassive(int baseDamage, Character* target = nullptr)
    {
        switch (passiveEffect)
        {
            case PassiveEffect::DAMAGE_BOOST:
                return (int)(baseDamage * 1.25f);   // +25% damage

            case PassiveEffect::STUN_ON_HIT:
                if (target && (rand() % 100) < 30)  // 30% stun chance
                    target->setStunned(true, 2);     // stun for 2 turns
                return baseDamage;

            case PassiveEffect::ULTIMATE_ENABLE:
                return baseDamage;   // passive only, no damage change
        }
        return baseDamage;
    }

    // ── Ownership ─────────────────────────────────────────────
    bool isHeld()           const { return holder_id != -1; }
    int  getHolder()        const { return holder_id; }
    void setHolder(int id)        { holder_id = id; }
    void release()                { holder_id = -1; }

    bool isAvailable()      const { return exists && !isHeld(); }
    void introduce()              { exists = true; }   // for Eclipse Relic

    ArtifactType  getArtifactType()  const { return artifactType; }
    PassiveEffect getPassiveEffect() const { return passiveEffect; }
};

