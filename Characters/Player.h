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
#include "../shared/shared_types.h"

using std::vector;
using std::pair;

class Player : public Character
{
private:
    PlayerType playerType;
    char       name[64];

    Inventory inventory;
    Backpack  backpack;

    int nextWeaponId;
    std::deque<std::pair<float, float>> path_to_follow;
    float nextPosX;
    float nextPosY;

public:
    Player(PlayerType type)
        : Character(CharacterType::PLAYER),
          playerType(type),
          nextWeaponId(1),
          nextPosX(0.f), nextPosY(0.f)
    {
        name[0] = '\0';
        setAttributes();
        loadDefaultInventory();
    }

    void setAttributes()
    {
        switch (playerType)
        {
            case PlayerType::CHRONO:
                maxHp = 150; demage = 35; MaxStamina = 100; speed = 4; break;
            case PlayerType::FROG:
                maxHp = 110; demage = 32; MaxStamina = 110; speed = 5; break;
            case PlayerType::MARLE:
                maxHp = 170; demage = 37; MaxStamina =  80; speed = 3; break;
            case PlayerType::MAGUS:
                maxHp =  90; demage = 30; MaxStamina = 130; speed = 5; break;
            default: break;
        }
        Hp      = maxHp;
        stamina = MaxStamina;
    }

    // ── Roll stats ────────────────────────────────────────────
    virtual void initRollStats(float speedOverride = -1.f) override
    {
        srand(rollFull);

        maxHp      = rollFull + 100 + (rand() % 901); // Player HP: Roll No + random(100-1000)
        Hp         = maxHp;
        demage     = rollLastDig + 10; // Player Damage: Last digit of Roll No + 10
        speed      = (speedOverride > 0.f) ? speedOverride : 4.f; // Adjusted default speed to 4
        MaxStamina = 100; // Player Max Stamina: 100
        stamina    = 0;
        alive      = true;
        myTurn     = false;
        stunned    = false;
        stunEndTem = 0;
    }

    // ── Position + scale init ─────────────────────────────────
    bool InitAllProperties(float spawnX, float spawnY)
    {
        switch (playerType)
        {
            case PlayerType::CHRONO:
                strncpy(name, "Chrono", sizeof(name) - 1);
                setScale(5.0f, 4.0f);
                break;
            case PlayerType::FROG:
                strncpy(name, "Frog",   sizeof(name) - 1);
                setScale(5.0f, 5.8f);
                break;
            case PlayerType::MARLE:
                strncpy(name, "Marle",  sizeof(name) - 1);
                setScale(5.0f, 4.0f);
                break;
            case PlayerType::MAGUS:
                strncpy(name, "Magus",  sizeof(name) - 1);
                setScale(4.4f, 4.375f);
                break;
            default:
                return false;
        }

        name[sizeof(name) - 1] = '\0';
        setXPos(spawnX);
        setYPos(spawnY);
        return true;
    }

    // ── Heal ──────────────────────────────────────────────────
    void heal() { RegainHealth(maxHp / 10); }

    // ── Ultimate check ────────────────────────────────────────
    bool canUseUltimate() const
    {
        return inventory.ownsWeaponType(WeaponType::ARTIFACT);
    }

    // ── Getters ───────────────────────────────────────────────
    std::string getName()     const { return name; }
    Inventory&  getInventory()      { return inventory; }
    Backpack&   getBackpack()       { return backpack; }
    int         generateWeaponId()  { return nextWeaponId++; }
    PlayerType  getPlayerType()     const { return playerType; }

    virtual void DoAction() override {}

    // ── Weapon management ─────────────────────────────────────
    bool pickupWeapon(const Weapon& weapon)
    {
        if (inventory.insertWeapon(weapon)) return true;

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
                auto next_coord = path_to_follow.front();
                nextPosX        = next_coord.first;
                nextPosY        = next_coord.second;
                path_to_follow.pop_front();
                completed_section = false;
            }
            else return true;
        }

        float dx   = nextPosX - posX;
        float dy   = nextPosY - posY;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist > 0)
        {
            if (speed >= dist) { posX = nextPosX; posY = nextPosY; completed_section = true; }
            else { posX += (dx / dist) * speed; posY += (dy / dist) * speed; }
        }
        else completed_section = true;

        return false;
        usleep(50000); // Sleep for 100ms to simulate time passage (adjust as needed)
    }

    std::deque<std::pair<float, float>> getPath(int level, int round)
    {
        std::string   filename = "Cooking on a weekend like usual.txt";
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("Movement file not found: " + filename);

        path_to_follow.clear();
        float x, y;
        while (file >> x >> y)
            path_to_follow.push_back({ x * 0.78125f, y * 0.78125f });

        file.close();
        return path_to_follow;
    }

    // ── Default inventory ─────────────────────────────────────
    void loadDefaultInventory()
    {
        switch (playerType)
        {
            case PlayerType::CHRONO:
{                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::IRON_HALBERD,   "Iron Halberd",   7, 55));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::THUNDERSTAFF,   "Thunderstaff",   6, 50));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::OBSIDIAN_AXE,   "Obsidian Axe",   5, 45));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12));
                Weapon w(generateWeaponId(), WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12);
                backpack.addWeapon(w);

                break;
            }
            case PlayerType::FROG:
{                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::FROSTBOW,       "Frostbow",       6, 48));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::THUNDERSTAFF,   "Thunderstaff",   6, 50));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::VENOM_DAGGER,   "Venom Dagger",   4, 30));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12));
                Weapon w1(generateWeaponId(), WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12);
                backpack.addWeapon(w1);
                break;}
            case PlayerType::MARLE:
{                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::IRON_HALBERD,   "Iron Halberd",   7, 55));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::OBSIDIAN_AXE,   "Obsidian Axe",   5, 45));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::VENOM_DAGGER,   "Venom Dagger",   4, 30));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12));
                Weapon w2(generateWeaponId(), WeaponType::VENOM_DAGGER,   "Venom Dagger",   4, 30);
                backpack.addWeapon(w2);
                break;}
            case PlayerType::MAGUS:
{                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::IRON_HALBERD,   "Iron Halberd",   7, 55));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::FROSTBOW,       "Frostbow",       6, 48));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::OBSIDIAN_AXE,   "Obsidian Axe",   5, 45));
                inventory.insertWeapon(Weapon(generateWeaponId(), WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12));
                Weapon w3(generateWeaponId(), WeaponType::SPLINTER_STICK, "Splinter Stick", 2, 12);
                backpack.addWeapon(w3);
                break;}


            default: break;
        }
    }
};
