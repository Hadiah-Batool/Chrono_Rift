#pragma once
#include <string>
#include "Characters.h"
#include "Inventory.h"
#include "Backpack.h"

enum class PlayerType
{
    CRONO,
    FROG,
    MARLE,
    MAGUS
};


class Player : public Character
{
private:
    PlayerType playerType;
    std::string name;

    Inventory inventory;
    Backpack backpack;

    int nextWeaponId;

public:
    Player()
    {
        nextWeaponId = 1;
    }

    virtual void DoAction() override
    {
        // Hook this to turn logic later
    }

    int generateWeaponId()
    {
        return nextWeaponId++;
    }

    Inventory& getInventory()
    {
        return inventory;
    }

    Backpack& getBackpack()
    {
        return backpack;
    }

    bool pickupWeapon(const Weapon& weapon)
    {
        if (inventory.insertWeapon(weapon))
            return true;

        std::vector<int> toRemove = inventory.findBestWeaponsToRemove(weapon.getSlotSize());

        if (toRemove.empty())
            return false;

        for (int id : toRemove)
        {
            Weapon removed;
            if (inventory.removeWeapon(id, removed))
            {
                backpack.addWeapon(removed);
            }
        }

        return inventory.insertWeapon(weapon);
    }

    bool swapInFromBackpack(int backpackIndex)
    {
        if (backpackIndex < 0 || backpackIndex >= backpack.getCount())
            return false;

        Weapon weapon = backpack.getWeaponAt(backpackIndex);

        if (inventory.insertWeapon(weapon))
        {
            backpack.removeWeaponAt(backpackIndex);
            return true;
        }

        std::vector<int> toRemove = inventory.findBestWeaponsToRemove(weapon.getSlotSize());

        if (toRemove.empty())
            return false;

        for (int id : toRemove)
        {
            Weapon removed;
            if (inventory.removeWeapon(id, removed))
            {
                backpack.addWeapon(removed);
            }
        }

        if (inventory.insertWeapon(weapon))
        {
            backpack.removeWeaponAt(backpackIndex);
            return true;
        }

        return false;
    }

    bool canUseUltimate() const
    {
        return inventory.ownsWeaponType(WeaponType::SOLAR_CORE) &&
               inventory.ownsWeaponType(WeaponType::LUNAR_BLADE);
    }
};
