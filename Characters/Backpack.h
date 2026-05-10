#pragma once
#include <vector>
#include <iostream>
#include "../Weapons/Weapons.h"

class Backpack
{
public:
    static const int MAX_BACKPACK = 20;

private:
    Weapon  storedWeapons[MAX_BACKPACK];
    int     count = 0;

public:
    Backpack() : count(0) {}

    void addWeapon(const Weapon& weapon)
    {
        if (count >= MAX_BACKPACK) return;
        storedWeapons[count++] = weapon;
    }

    bool isEmpty() const { return count == 0; }
    int  getCount() const { return count; }
// Add this to Backpack public section
bool removeWeaponById(int weaponId)
{
    for (int i = 0; i < count; i++)
    {
        if (storedWeapons[i].getWeaponId() == weaponId)
        {
            removeWeaponAt(i);   // reuses existing shift logic
            return true;
        }
    }
    std::cout << "[Backpack] removeWeaponById: id="
              << weaponId << " not found\n";
    return false;
}

    // Returns a freshly built vector — only called by renderer, not hot path
    std::vector<Weapon> getWeapons() const
    {
        std::vector<Weapon> out;
        for (int i = 0; i < count; i++)
            out.push_back(storedWeapons[i]);
        return out;
    }

    Weapon getWeaponAt(int index) const
    {
        if (index < 0 || index >= count)
            return Weapon{};   // safe empty default
        return storedWeapons[index];
    }

    void removeWeaponAt(int index)
    {
        if (index < 0 || index >= count) return;

        // Shift left — preserves order
        for (int i = index; i < count - 1; i++)
            storedWeapons[i] = storedWeapons[i + 1];
        count--;
    }
};
