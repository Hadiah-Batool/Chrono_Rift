#pragma once
#include <array>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include "../Weapons/Weapons.h"

class Inventory
{
private:
    static const int INVENTORY_SIZE = 20;

    std::array<int, INVENTORY_SIZE> slots; // -1 means empty, otherwise weaponId
    std::unordered_map<int, Weapon> equippedWeapons;

public:
    Inventory()
    {
        slots.fill(-1);
    }

    const std::array<int, INVENTORY_SIZE>& getSlots() const
    {
        return slots;
    }

    const std::unordered_map<int, Weapon>& getEquippedWeapons() const
    {
        return equippedWeapons;
    }

    bool hasWeapon(int weaponId) const
    {
        return equippedWeapons.find(weaponId) != equippedWeapons.end();
    }

    bool ownsWeaponType(WeaponType type) const
    {
        for (const auto& pair : equippedWeapons)
        {
            if (pair.second.getType() == type)
                return true;
        }
        return false;
    }

    int findContiguousFreeBlock(int neededSize) const
    {
        int count = 0;
        int start = -1;

        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            if (slots[i] == -1)
            {
                if (count == 0)
                    start = i;

                count++;

                if (count >= neededSize)
                    return start;
            }
            else
            {
                count = 0;
                start = -1;
            }
        }

        return -1;
    }

    bool insertWeapon(const Weapon& weapon)
    {
        int start = findContiguousFreeBlock(weapon.getSlotSize());
        if (start == -1)
            return false;

        equippedWeapons[weapon.getWeaponId()] = weapon;
        for (int i = start; i < start + weapon.getSlotSize(); i++)
            slots[i] = weapon.getWeaponId();

        return true;
    }

    bool removeWeapon(int weaponId, Weapon& removedWeapon)
    {
        auto it = equippedWeapons.find(weaponId);
        if (it == equippedWeapons.end())
            return false;

        removedWeapon = it->second;

        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            if (slots[i] == weaponId)
                slots[i] = -1;
        }

        equippedWeapons.erase(it);
        return true;
    }

    std::vector<int> getUniqueWeaponIds() const
    {
        std::vector<int> ids;
        for (const auto& pair : equippedWeapons)
            ids.push_back(pair.first);
        return ids;
    }

    void printInventory() const
    {
        std::cout << "Slots: ";
        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            std::cout << slots[i] << " ";
        }
        std::cout << "\n";

        std::cout << "Weapons:\n";
        for (const auto& pair : equippedWeapons)
        {
            std::cout << "ID: " << pair.first
                      << " Name: " << pair.second.getName()
                      << " Size: " << pair.second.getSlotSize()
                      << " Damage: " << pair.second.getDamage() << "\n";
        }
    }
    private:
    bool canCreateSpaceByRemoving(const std::vector<int>& removeIds, int neededSize) const
    {
        std::array<int, INVENTORY_SIZE> tempSlots = slots;

        for (int removeId : removeIds)
        {
            for (int i = 0; i < INVENTORY_SIZE; i++)
            {
                if (tempSlots[i] == removeId)
                    tempSlots[i] = -1;
            }
        }

        int count = 0;
        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            if (tempSlots[i] == -1)
            {
                count++;
                if (count >= neededSize)
                    return true;
            }
            else
            {
                count = 0;
            }
        }
        return false;
    }

    int getTotalRemovedSize(const std::vector<int>& removeIds) const
    {
        int total = 0;
        for (int id : removeIds)
        {
            auto it = equippedWeapons.find(id);
            if (it != equippedWeapons.end())
                total += it->second.getSlotSize();
        }
        return total;
    }

public:
    std::vector<int> findBestWeaponsToRemove(int neededSize) const
    {
        std::vector<int> ids = getUniqueWeaponIds();
        int n = static_cast<int>(ids.size());

        std::vector<int> bestChoice;
        bool found = false;

        for (int mask = 1; mask < (1 << n); mask++)
        {
            std::vector<int> chosen;

            for (int i = 0; i < n; i++)
            {
                if (mask & (1 << i))
                    chosen.push_back(ids[i]);
            }

            if (canCreateSpaceByRemoving(chosen, neededSize))
            {
                if (!found)
                {
                    bestChoice = chosen;
                    found = true;
                }
                else
                {
                    if (chosen.size() < bestChoice.size())
                    {
                        bestChoice = chosen;
                    }
                    else if (chosen.size() == bestChoice.size() &&
                             getTotalRemovedSize(chosen) < getTotalRemovedSize(bestChoice))
                    {
                        bestChoice = chosen;
                    }
                }
            }
        }

        return bestChoice;
    }

};
