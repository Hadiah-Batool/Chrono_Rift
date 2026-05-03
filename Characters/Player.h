#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <deque>
#include <utility>
#include <cstdlib>
#include <cmath>
#include "Characters.h"
#include "Inventory.h"
#include "Backpack.h"
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
    PlayerType   playerType;
    char name[64];

    Inventory inventory;
    Backpack  backpack;

    int nextWeaponId;
    std::deque<std::pair<float, float>> path_to_follow;
    sf::Vector2f nextPos;

public:
    Player(PlayerType type)
        : Character(CharacterType::PLAYER), playerType(type)
    {
        nextWeaponId = 1;
    }

    // ── Roll stats ────────────────────────────────────────────
    // speedOverride = 100.f / numPlayers, computed by Arbiter and passed in
    // Call setRollNumber() before this
    virtual void initRollStats(float speedOverride = -1.f) override
    {
        srand(rollFull); // roll number is the seed per spec

        maxHp  = rollFull + 100 + (rand() % 901); // rollFull + rand(100-1000)
        Hp     = maxHp;
        demage = rollLastDig + 10;                // last digit + 10
        speed  = (speedOverride > 0.f)            // 100 / numPlayers
                 ? speedOverride : 100.f;

        MaxStamina = 100;
        stamina    = 0;
        alive      = true;
        myTurn     = false;
        stunned    = false;
        stunEndTem = 0;
    }

    // ── Sprite + position init ────────────────────────────────
    // Call after initRollStats()
    bool InitAllProperties(float spawnX, float spawnY)
    {
        switch (playerType)
        {
            case PlayerType::CHRONO:
                strncpy(name, "Chrono", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                SetOriginSprite(16.0f, 35.0f);
                SetScaleSprite(5.0f, 4.0f);
                break;

            case PlayerType::FROG:
                strncpy(name, "Frog", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                SetOriginSprite(16.0f, 24.0f);
                SetScaleSprite(5.0f, 5.8f);
                break;

            case PlayerType::MARLE:
                strncpy(name, "Marle", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                SetOriginSprite(16.0f, 35.0f);
                SetScaleSprite(5.0f, 4.0f);
                break;

            case PlayerType::MAGUS:
                strncpy(name, "Magus", sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0'   ; // Ensure null-termination
                SetOriginSprite(18.0f, 32.0f);
                SetScaleSprite(4.4f, 4.375f);
                break;

            default:
                return false;
        }

        setXPos(spawnX);
        setYPos(spawnY);
        return true;
    }

    // ── Heal action (10% of maxHp per spec) ──────────────────
    void heal()
    {
        RegainHealth(maxHp / 10);
    }

    // ── Ultimate check ────────────────────────────────────────
    bool canUseUltimate() const
    {
        return inventory.ownsWeaponType(WeaponType::ARTIFACT);
        //ADD SOLAR AND LUNAR CHECKS LATER IF WE DECIDE TO MAKE EM SEPARATE
    }

    // ── Getters ───────────────────────────────────────────────
    std::string  getName()       const { return name; }
    Inventory&   getInventory()        { return inventory; }
    Backpack&    getBackpack()         { return backpack; }
    int          generateWeaponId()    { return nextWeaponId++; }

    virtual void DoAction() override
    {
        // Hooked to HIP turn logic
    }

    // ── Weapon management ─────────────────────────────────────
    bool pickupWeapon(const Weapon& weapon)
    {
        if (inventory.insertWeapon(weapon))
            return true;

        std::vector<int> toRemove =
            inventory.findBestWeaponsToRemove(weapon.getSlotSize());
        if (toRemove.empty()) return false;

        for (int id : toRemove)
        {
            Weapon removed;
            if (inventory.removeWeapon(id, removed))
                backpack.addWeapon(removed);
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

        std::vector<int> toRemove =
            inventory.findBestWeaponsToRemove(weapon.getSlotSize());
        if (toRemove.empty()) return false;

        for (int id : toRemove)
        {
            Weapon removed;
            if (inventory.removeWeapon(id, removed))
                backpack.addWeapon(removed);
        }

        if (inventory.insertWeapon(weapon))
        {
            backpack.removeWeaponAt(backpackIndex);
            return true;
        }
        return false;
    }
    

    // ── Movement ──────────────────────────────────────────────
    bool movement(bool& completed_section)
    {
        if (completed_section)
        {
            if (!path_to_follow.empty())
            {
                auto next_coord   = path_to_follow.front();
                nextPos           = sf::Vector2f(next_coord.first, next_coord.second);
                path_to_follow.pop_front();
                completed_section = false;
            }
            else return true;
        }

        sf::Vector2f direction = nextPos - pos;
        float dist = std::sqrt(direction.x * direction.x +
                               direction.y * direction.y);

        if (dist > 0)
        {
            if (speed >= dist) { pos = nextPos; completed_section = true; }
            else               pos += (direction / dist) * speed;
        }
        else completed_section = true;

        return false;
    }

    std::deque<std::pair<float, float>> getPath(int level, int round)
    {
        std::string filename = "Cooking on a weekend like usual.txt";
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("Movement file not found: " + filename);

        path_to_follow.clear();
        float x, y;
        while (file >> x >> y)
            path_to_follow.push_back({x * 0.78125f, y * 0.78125f});

        file.close();
        return path_to_follow;
    }
    //ATTACK EM BEES
    
};