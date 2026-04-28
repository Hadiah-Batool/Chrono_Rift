#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <deque>
#include <utility>
#include "Characters.h"
#include "Inventory.h"
#include "Backpack.h"
#include <cmath>
#include "../resources/shared_mem_abs.h"

using std::vector;
using std::pair;

enum class PlayerType
{
    CHRONO,
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
    std::deque<std::pair<int, int>> path_to_follow;
    sf::Vector2f nextPos;

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

    bool movement(bool& completed_section) {
        // Fetch next checkpoint if the current one is reached
        if (completed_section) {
            if (!path_to_follow.empty()) {
                std::pair<int, int> next_coord = path_to_follow.front();

                nextPos = sf::Vector2f(static_cast<float>(next_coord.first), static_cast<float>(next_coord.second));

                path_to_follow.pop_front();
                completed_section = false;
            } else {
                return true; // Path fully finished
            }
        }

        sf::Vector2f direction = nextPos - pos;
        float dist = std::sqrt(direction.x * direction.x + direction.y * direction.y);

        if (dist > 0) {
            if (speed >= dist) {
                // Snap to target to prevent overshooting
                pos = nextPos;
                completed_section = true;
            } else {
                pos += (direction / dist) * speed;
            }
        } else {
            completed_section = true;
        }

        return false;
    }

    std::deque<std::pair<int, int>> getPath(int level, int round) {
        std::string filename = "../movements/movement_" + std::to_string(level) + "_" + std::to_string(round) + ".txt";
        std::ifstream file(filename);

        if (!file.is_open()) {
            throw std::runtime_error("Movement file not found: " + filename);
        }

        // Clear existing path
        path_to_follow.clear();

        int x, y;
        while (file >> x >> y) {
            path_to_follow.push_back({x, y});
        }
        file.close();

        return path_to_follow;
    }
};
