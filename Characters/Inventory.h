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

    // Artifact weapon IDs are 0, 1, 2 — never evict these
    static bool isArtifactWeapon(const Weapon& w)
    {
        return w.getType() == WeaponType::ARTIFACT;
    }


// ── Add this private method ───────────────────────────────────────────
private:
    // Defragments the slots array after any removal
    // Weapons are repacked left-to-right in weaponStore order
        void repackSlots()
        {
            slots.fill(-1);
            int pos = 0;
            for (int i = 0; i < weaponCount; i++)
            {
                if (!weaponStore[i].occupied) continue;
                int sz = weaponStore[i].weapon.getSlotSize();
                int id = weaponStore[i].weapon.getWeaponId();
                for (int j = pos; j < pos + sz; j++)
                    slots[j] = id;
                pos += sz;
            }
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
        int count = 0;
        int start = -1;
        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            if (slots[i] == -1)
            {
                if (count == 0) start = i;   // mark start of free run
                count++;
                if (count >= neededSize) return start;
            }
            else
            {
                count = 0;
                start = -1;   // ← was missing this reset
            }
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
        // Returns total slots currently occupied across all weapons
    int getTotalUsedSlots() const
    {
        int total = 0;
        for (int i = 0; i < weaponCount; i++)
            if (weaponStore[i].occupied)
                total += weaponStore[i].weapon.getSlotSize();
        return total;
    }

    int getFreeSlots() const
    {
        return INVENTORY_SIZE - getTotalUsedSlots();
    }



    bool insertWeapon(const Weapon& weapon)
    {
        if (weaponCount >= MAX_WEAPONS) return false;

        // ── STRICT: reject if total slots would exceed 20 ─────────────────
        if (getTotalUsedSlots() + weapon.getSlotSize() > INVENTORY_SIZE)
        {
            std::cout << "[Inventory] REJECTED: inserting '"
                    << weapon.getName()
                    << "' (size=" << weapon.getSlotSize()
                    << ") would exceed 20 slots. Used="
                    << getTotalUsedSlots() << "\n";
            return false;
        }

        int start = findContiguousFreeBlock(weapon.getSlotSize());
        if (start == -1)
        {
            std::cout << "[Inventory] REJECTED: no contiguous block of size "
                    << weapon.getSlotSize() << " available\n";
            return false;
        }

        // Store in flat array
        weaponStore[weaponCount].occupied = true;
        weaponStore[weaponCount].weapon   = weapon;
        weaponCount++;

        // Mark slots
        for (int i = start; i < start + weapon.getSlotSize(); i++)
            slots[i] = weapon.getWeaponId();

        std::cout << "[Inventory] Inserted '" << weapon.getName()
                << "' (size=" << weapon.getSlotSize()
                << "). Total used=" << getTotalUsedSlots() << "/20\n";
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

            for (int s = 0; s < INVENTORY_SIZE; s++)
                if (slots[s] == weaponId) slots[s] = -1;

            weaponStore[i] = weaponStore[weaponCount - 1];
            weaponStore[weaponCount - 1].occupied = false;
            weaponCount--;

            repackSlots();   // ← ADD: always defragment after removal
            return true;
        }
    }
    return false;
}


    std::vector<int> getUniqueWeaponIds() const
    {
        std::vector<int> ids;
        for (int i = 0; i < weaponCount; i++)
            if (weaponStore[i].occupied
                && !isArtifactWeapon(weaponStore[i].weapon))  // guard
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

public:
    // Greedy eviction: biggest non-artifact first, stops as soon as enough space freed
    std::vector<int> findBestWeaponsToRemove(int neededSize) const
    {
        struct Candidate { int id; int size; };
        std::vector<Candidate> candidates;

        for (int i = 0; i < weaponCount; i++)
        {
            if (!weaponStore[i].occupied) continue;
            int id = weaponStore[i].weapon.getWeaponId();
            if (isArtifactWeapon(weaponStore[i].weapon)) continue;   // NEVER evict artifacts
            candidates.push_back({ id, weaponStore[i].weapon.getSlotSize() });
        }

        // Biggest slot size first  fewest evictions needed
        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b){ return a.size > b.size; });

        int freeNow = getFreeSlots();
        std::vector<int> toRemove;

        for (const auto& c : candidates)
        {
            if (freeNow >= neededSize) break;
            toRemove.push_back(c.id);
            freeNow += c.size;
        }

        if (freeNow < neededSize)
        {
            std::cout << "[Inventory] Cannot free " << neededSize
                      << " slots — only " << freeNow
                      << " achievable (artifacts blocking)\n";
            return {};
        }

        return toRemove;
    }



};
