#pragma once
#include <string>
#include <SFML/Graphics.hpp>

using namespace std;
using namespace sf;
enum class WeaponType
{
    SOLAR_CORE,
    LUNAR_BLADE,
    IRON_HALBERD,
    VENOM_DAGGER,
    THUNDERSTAFF,
    OBSIDIAN_AXE,
    FROSTBOW,
    SPLINTER_STICK,
    ECLIPSE_RELIC
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
