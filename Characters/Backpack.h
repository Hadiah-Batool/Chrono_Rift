#pragma once
#include <vector>
#include "../Weapons/Weapons.h"

class Backpack
{
private:
    std::vector<Weapon> storedWeapons;

public:
    void addWeapon(const Weapon& weapon)
    {
        storedWeapons.push_back(weapon);
    }

    bool isEmpty() const
    {
        return storedWeapons.empty();
    }

    int getCount() const
    {
        return static_cast<int>(storedWeapons.size());
    }

    const std::vector<Weapon>& getWeapons() const
    {
        return storedWeapons;
    }

    Weapon getWeaponAt(int index) const
    {
        return storedWeapons.at(index);
    }

    void removeWeaponAt(int index)
    {
        storedWeapons.erase(storedWeapons.begin() + index);
    }
};
