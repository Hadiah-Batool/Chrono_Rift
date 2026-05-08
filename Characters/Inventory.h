#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <iostream>
#include "../Weapons/Weapons.h"

class Inventory
{
public:
    static const int INVENTORY_SIZE  = 20;
    static const int MAX_WEAPONS     = 10;  // max unique weapons that fit in 20 slots

private:
    // ── Flat, shm-safe storage ────────────────────────────────────────────────
    // No heap pointers — everything lives inside the struct itself

    struct WeaponSlot
    {
        bool   occupied = false;
        Weapon weapon;
    };

    std::array<int,        INVENTORY_SIZE> slots;        // -1 = empty, else weaponId
    std::array<WeaponSlot, MAX_WEAPONS>    weaponStore;  // flat weapon storage
    int                                    weaponCount = 0;

    // ── Internal helpers ──────────────────────────────────────────────────────

    WeaponSlot* findSlotById(int weaponId)
    {
        for (int i = 0; i < weaponCount; i++)
            if (weaponStore[i].occupied && weaponStore[i].weapon.getWeaponId() == weaponId)
                return &weaponStore[i];
        return nullptr;
    }

    const WeaponSlot* findSlotById(int weaponId) const
    {
        for (int i = 0; i < weaponCount; i++)
            if (weaponStore[i].occupied && weaponStore[i].weapon.getWeaponId() == weaponId)
                return &weaponStore[i];
        return nullptr;
    }

public:
    Inventory()
    {
        slots.fill(-1);
        for (auto& s : weaponStore) s.occupied = false;
        weaponCount = 0;
    }

    // ── Drop-in replacement for getEquippedWeapons() ──────────────────────────
    // Returns a vector built on the fly — only called by renderer, not hot path
    std::vector<std::pair<int, Weapon>> getEquippedWeaponsVec() const
    {
        std::vector<std::pair<int, Weapon>> out;
        for (int i = 0; i < weaponCount; i++)
            if (weaponStore[i].occupied)
                out.push_back({ weaponStore[i].weapon.getWeaponId(),
                                weaponStore[i].weapon });
        return out;
    }

    // ── Keep old name so render.h doesn't need changes ────────────────────────
    std::vector<std::pair<int, Weapon>> getEquippedWeapons() const
    {
        return getEquippedWeaponsVec();
    }

    const std::array<int, INVENTORY_SIZE>& getSlots() const { return slots; }

    bool hasWeapon(int weaponId) const
    {
        return findSlotById(weaponId) != nullptr;
    }

    bool ownsWeaponType(WeaponType type) const
    {
        for (int i = 0; i < weaponCount; i++)
            if (weaponStore[i].occupied && weaponStore[i].weapon.getType() == type)
                return true;
        return false;
    }

    int findContiguousFreeBlock(int neededSize) const
    {
        int count = 0, start = -1;
        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            if (slots[i] == -1)
            {
                if (count == 0) start = i;
                if (++count >= neededSize) return start;
            }
            else { count = 0; start = -1; }
        }
        return -1;
    }
    // In Inventory.h — public section
bool getWeaponById(int weaponId, Weapon& out) const
{
    for (int i = 0; i < weaponCount; i++)
    {
        if (weaponStore[i].occupied &&
            weaponStore[i].weapon.getWeaponId() == weaponId)
        {
            out = weaponStore[i].weapon;
            return true;
        }
    }
    return false;
}


    bool insertWeapon(const Weapon& weapon)
    {
        if (weaponCount >= MAX_WEAPONS) return false;

        int start = findContiguousFreeBlock(weapon.getSlotSize());
        if (start == -1) return false;

        // Store in flat array
        weaponStore[weaponCount].occupied = true;
        weaponStore[weaponCount].weapon   = weapon;
        weaponCount++;

        // Mark slots
        for (int i = start; i < start + weapon.getSlotSize(); i++)
            slots[i] = weapon.getWeaponId();

        return true;
    }

    bool removeWeapon(int weaponId, Weapon& removedWeapon)
    {
        for (int i = 0; i < weaponCount; i++)
        {
            if (weaponStore[i].occupied &&
                weaponStore[i].weapon.getWeaponId() == weaponId)
            {
                removedWeapon = weaponStore[i].weapon;

                // Clear slots
                for (int s = 0; s < INVENTORY_SIZE; s++)
                    if (slots[s] == weaponId) slots[s] = -1;

                // Compact the store (swap with last)
                weaponStore[i] = weaponStore[weaponCount - 1];
                weaponStore[weaponCount - 1].occupied = false;
                weaponCount--;
                return true;
            }
        }
        return false;
    }

    std::vector<int> getUniqueWeaponIds() const
    {
        std::vector<int> ids;
        for (int i = 0; i < weaponCount; i++)
            if (weaponStore[i].occupied)
                ids.push_back(weaponStore[i].weapon.getWeaponId());
        return ids;
    }

    void printInventory() const
    {
        std::cout << "Slots: ";
        for (int i = 0; i < INVENTORY_SIZE; i++)
            std::cout << slots[i] << " ";
        std::cout << "\n";

        std::cout << "Weapons:\n";
        for (int i = 0; i < weaponCount; i++)
        {
            if (!weaponStore[i].occupied) continue;
            const Weapon& w = weaponStore[i].weapon;
            std::cout << "ID: "     << w.getWeaponId()
                      << " Name: "  << w.getName()
                      << " Size: "  << w.getSlotSize()
                      << " Damage: "<< w.getDamage() << "\n";
        }
    }

    // ── findBestWeaponsToRemove — unchanged logic, same interface ─────────────
    std::vector<int> findBestWeaponsToRemove(int neededSize) const
    {
        std::vector<int> ids = getUniqueWeaponIds();
        int n = (int)ids.size();

        std::vector<int> bestChoice;
        bool found = false;

        for (int mask = 1; mask < (1 << n); mask++)
        {
            std::vector<int> chosen;
            for (int i = 0; i < n; i++)
                if (mask & (1 << i)) chosen.push_back(ids[i]);

            if (canCreateSpaceByRemoving(chosen, neededSize))
            {
                if (!found)
                {
                    bestChoice = chosen;
                    found = true;
                }
                else if (chosen.size() < bestChoice.size())
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
        return bestChoice;
    }

private:
    bool canCreateSpaceByRemoving(const std::vector<int>& removeIds, int neededSize) const
    {
        std::array<int, INVENTORY_SIZE> tempSlots = slots;
        for (int removeId : removeIds)
            for (int i = 0; i < INVENTORY_SIZE; i++)
                if (tempSlots[i] == removeId) tempSlots[i] = -1;

        int count = 0;
        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            if (tempSlots[i] == -1) { if (++count >= neededSize) return true; }
            else count = 0;
        }
        return false;
    }

    int getTotalRemovedSize(const std::vector<int>& removeIds) const
    {
        int total = 0;
        for (int id : removeIds)
        {
            const WeaponSlot* s = findSlotById(id);
            if (s) total += s->weapon.getSlotSize();
        }
        return total;
    }
};
