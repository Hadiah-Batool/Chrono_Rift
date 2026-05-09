#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <pthread.h>
#include <cstdarg>
#include <functional>
#include "../Characters/Player.h"
#include "../Characters/Enemy.h"
#include "Map.h"
#include "../shared/shared_types.h"
#include "EnemyRenderer.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ACTION CONSTANTS
// ─────────────────────────────────────────────────────────────────────────────
#define ACTION_STRIKE       0
#define ACTION_EXHAUST      1
#define ACTION_USE_WEAPON   2
#define ACTION_SWAP_IN      3
#define ACTION_HEAL         4
#define ACTION_SKIP         5

// ─────────────────────────────────────────────────────────────────────────────
//  FONT SIZE CONSTANTS
// ─────────────────────────────────────────────────────────────────────────────
constexpr unsigned FONT_XS = 14;
constexpr unsigned FONT_SM = 16;
constexpr unsigned FONT_MD = 18;
constexpr unsigned FONT_LG = 28;
constexpr unsigned FONT_XL = 32;

// ─────────────────────────────────────────────────────────────────────────────
//  Layout constants
// ─────────────────────────────────────────────────────────────────────────────
constexpr float WIN_W = 1280.f;
constexpr float WIN_H = 800.f;
constexpr float MAP_W = 860.f;
constexpr float MAP_H = 800.f;
constexpr float SB_X  = 860.f;
constexpr float SB_W  = 420.f;
constexpr float SB_H  = 800.f;
constexpr float PAD   = 8.f;
// Active player section — compressed
constexpr float SEC_ACTIVE_Y = 0.f;
constexpr float SEC_ACTIVE_H = 160.f;   // was 290 — now compact

// Log — smaller
constexpr float LOG_Y       = SEC_ACTIVE_Y + SEC_ACTIVE_H + 2.f;
constexpr float LOG_H       = 72.f;     // was 120
constexpr float LOG_PREVIEW = 3;
// Tab buttons
constexpr float BTN_Y = LOG_Y + LOG_H + 2.f;
constexpr float BTN_H = 26.f;
constexpr float BTN_W = (SB_W - PAD * 2 - 4.f) / 3.f;
// Panel — gets the rest of the space
constexpr float PANEL_Y = BTN_Y + BTN_H + 2.f;
constexpr float PANEL_H = SB_H - PANEL_Y;

// Enemy card sizing — dynamic, fits 4-9
constexpr float ENEMY_CARD_H = 70.f;
constexpr float ENEMY_CARD_GAP = 4.f;

// ── Player static sprites ─────────────────────────────────────────────
sf::Texture m_playerTextures[4];   // one per player slot
sf::Sprite  m_playerSprites[4];
bool        m_playerSpritesLoaded = false;

static constexpr const char* PLAYER_SPRITE_PATHS[] = {
    "../Players/Chrono_sprite_back_frame1.png",   // CHRONO
    "../Players/Frog_sprite_backframe1.png",     // FROG
    "../Players/Marle_sprite_backframe1.png",    // MARLE
    "../Players/Magus_sprite_backframe1.png",    // MAGUS
};

// ─────────────────────────────────────────────────────────────────────────────
//  Colours
// ─────────────────────────────────────────────────────────────────────────────
namespace Colour
{
    const sf::Color SidebarBg     = {  18,  18,  28, 255 };
    const sf::Color SectionBg     = {  28,  28,  42, 255 };
    const sf::Color Divider       = {  60,  60,  90, 255 };
    const sf::Color HpBack        = {  45,  45,  45, 255 };
    const sf::Color HpFull        = {  50, 205,  50, 255 };
    const sf::Color HpLow         = { 220,  50,  50, 255 };
    const sf::Color StamBack      = {  30,  30,  60, 255 };
    const sf::Color StamFull      = {  50, 150, 255, 255 };
    const sf::Color StunBadge     = { 255, 200,   0, 255 };
    const sf::Color AliveBadge    = {  50, 205,  50, 255 };
    const sf::Color DeadBadge     = { 150,  30,  30, 255 };
    const sf::Color TxtPrimary    = { 230, 230, 230, 255 };
    const sf::Color TxtMuted      = { 140, 140, 160, 255 };
    const sf::Color TxtName       = { 255, 220, 100, 255 };
    const sf::Color BtnActive     = {  70,  70, 130, 255 };
    const sf::Color BtnInactive   = {  35,  35,  60, 255 };
    const sf::Color BtnBorder     = {  90,  90, 150, 255 };
    const sf::Color WeaponCard    = {  30,  30,  50, 255 };
    const sf::Color DmgColour     = { 255, 100, 100, 255 };
    const sf::Color CountBadge    = { 255, 180,  50, 255 };
    const sf::Color SelectedEnemy = { 255, 220,  50,  80 };
    const sf::Color ActiveTurn    = { 255, 220,  50, 255 };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Renderer — fully inline, no render.cpp needed
// ─────────────────────────────────────────────────────────────────────────────
class Renderer
{
public:
    enum class SidebarMode { ENEMIES, INVENTORY, BACKPACK };

    // ── Constructor A: HIP / shm mode (production) ───────────────────────────
     Renderer(SharedMemoryBlock* block, Map* map)
        : m_block(block)
        , m_shm(block ? &block->state : nullptr)
        , m_map(map)
        , m_sidebarMode(SidebarMode::ENEMIES)
    {
        pthread_mutex_init(&m_stopMutex, nullptr);
        std::cout << "[Renderer] Created in shm mode\n";
    }

    // ── Constructor B: local pointer mode (unit testing only) ────────────────
    Renderer(std::vector<Player*>    players,
             std::vector<Character*> enemies,
             Map*                    map)
        : m_localPlayers(std::move(players))
        , m_localEnemies(std::move(enemies))
        , m_map(map)
        , m_sidebarMode(SidebarMode::ENEMIES)
    {
        pthread_mutex_init(&m_stopMutex, nullptr);
        std::cout << "[Renderer] Created in local mode\n";
    }

    ~Renderer()
    {
        pthread_mutex_destroy(&m_stopMutex);
    }

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    // ── Public API ────────────────────────────────────────────────────────────

    // Called by turnWatcherThread in hip.cpp after arbiter broadcasts a new turn
    void setActiveTurn(int entityId, bool isPlayer)
    {
        m_activeTurnId   = entityId;
        m_activeIsPlayer = isPlayer;
        if (isPlayer) m_activeIdx = entityId;
        std::cout << "[Renderer] Active turn → entity=" << entityId
                  << (isPlayer ? " (player)\n" : " (enemy)\n");
    }

    // Called by hip.cpp to wire keypress → submitAction pipeline
    void setActionCallback(std::function<void(Action, int, int)> cb)
    {
        m_actionCallback = cb;
        std::cout << "[Renderer] Action callback registered\n";
    }

    void advanceMapScreen() { if (m_map) m_map->nextScreen();     }
    void retreatMapScreen() { if (m_map) m_map->previousScreen(); }

    void requestStop()
    {
        pthread_mutex_lock(&m_stopMutex);
        m_stop = true;
        pthread_mutex_unlock(&m_stopMutex);
    }

    // Blocks until window closes — called from renderThread in hip.cpp
    void run()
    {
        m_window.create(
            sf::VideoMode((unsigned)WIN_W, (unsigned)WIN_H),
            "Chrono Rift",
            sf::Style::Titlebar | sf::Style::Close
        );
                // spin until arbiter has populated enemies (non-blocking poll)
        while (m_shm->num_active_enemies == 0)
            sf::sleep(sf::milliseconds(10));
        m_window.setFramerateLimit(60);
        loadAssets();
        std::cout << "[Renderer] Window open — entering game loop\n";
        loadEnemyRenderers();
        while (m_window.isOpen())
        {
            pthread_mutex_lock(&m_stopMutex);
            bool stop = m_stop;
            pthread_mutex_unlock(&m_stopMutex);
            if (stop) { m_window.close(); break; }
            float dt = m_clock.restart().asSeconds();  
            handleEvents();
            m_window.clear(Colour::SidebarBg);
            drawAll(dt);
            
           
            m_window.display();
        }

        std::cout << "[Renderer] Game loop exited\n";
    }

// ─────────────────────────────────────────────────────────────────────────────
private:
// ─────────────────────────────────────────────────────────────────────────────

    // ── Data ──────────────────────────────────────────────────────────────────

    // shm mode
    SharedMemoryBlock* m_block = nullptr;
    GameState*         m_shm   = nullptr;

    // local mode (testing)
    std::vector<Player*>    m_localPlayers;
    std::vector<Character*> m_localEnemies;

    Map* m_map = nullptr;

    // Turn tracking
    int  m_activeTurnId   = 0;
    bool m_activeIsPlayer = true;
    int  m_activeIdx      = 0;     // which player the sidebar shows
    int m_lastActivePlayerIdx=0;

    SidebarMode m_sidebarMode;

    // Selection cursors (arrow keys)
    int m_selectedEnemy  = 0;
    int m_selectedWeapon = 0;

    // SFML
    sf::RenderWindow m_window;
    sf::Font         m_font;
    bool             m_fontLoaded = false;
    sf::Clock        m_clock;

    std::unordered_map<std::string, sf::Texture> m_weaponTextures;

    // Stop flag (set by renderThread on window close)
    bool            m_stop         = false;
    pthread_mutex_t m_stopMutex;

    // Mouse click edge detection
    bool m_prevMouseDown = false;

    // Log overlay toggle
    bool m_logExpanded = false;


    // Callback wired by hip.cpp
    std::function<void(Action, int, int)> m_actionCallback;

    // In Renderer class — private members
    EnemyRenderer m_enemyRenderers[9];   // matches MAX_ENEMIES in GameState
    bool          m_enemiesLoaded = false;


    // ─────────────────────────────────────────────────────────────────────────
    //  Mode helpers
    // ─────────────────────────────────────────────────────────────────────────

    bool isShmMode() const { return m_shm != nullptr; }

    int totalPlayerCount() const
    {
        return isShmMode()
            ? m_shm->num_active_players
            : (int)m_localPlayers.size();
    }

    int totalEnemyCount() const
    {
        return isShmMode()
            ? m_shm->num_active_enemies
            : (int)m_localEnemies.size();
    }

    int aliveEnemyCount() const
    {
        int n = 0;
        for (int i = 0; i < totalEnemyCount(); i++)
            if (isEnemyAlive(i)) n++;
        return n;
    }

    bool isEnemyAlive(int idx) const
    {
        if (isShmMode())
            return idx >= 0
                && idx < m_shm->num_active_enemies
                && m_shm->enemies[idx].isAlive();
        return idx >= 0
            && idx < (int)m_localEnemies.size()
            && m_localEnemies[idx]
            && m_localEnemies[idx]->isAlive();
    }
    // Call once when enemies are first populated (after SETUP_GAME handshake)
void loadEnemyRenderers()
{
    if (!m_shm) return;

    int count = m_shm->num_active_enemies;
    if (count <= 0) return;

    for (int i = 0; i < count; i++)
    {
        m_enemyRenderers[i] = EnemyRenderer{};          // reset first
        m_enemyRenderers[i].init(m_shm->enemies[i]);
    }

    m_enemiesLoaded = true;
    std::cout << "[RENDERER] Loaded " << count << " enemy renderers\n";
}


// Call every frame inside drawAll(), before drawEnemySection()
void updateAndDrawEnemies(sf::RenderWindow& window, float dt)
{
    if (!m_enemiesLoaded || !isShmMode()) return;

    // iterate over ALL slots, not just num_active_enemies
    // because dead enemies still sit in their index
    int total = m_shm->num_active_enemies;

    for (int i = 0; i < total; i++)
    {
        const Enemy& e = m_shm->enemies[i];
        if (!e.isAlive()) continue;

        m_enemyRenderers[i].update(dt, e);
        m_enemyRenderers[i].draw(window, e);
    }
}


    bool isPlayerAlive(int idx) const
    {
        if (isShmMode())
            return idx >= 0
                && idx < m_shm->num_active_players
                && m_shm->players[idx].isAlive();
        return idx >= 0
            && idx < (int)m_localPlayers.size()
            && m_localPlayers[idx]
            && m_localPlayers[idx]->isAlive();
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Unified getters — all draw functions use these, never touch shm directly
    // ─────────────────────────────────────────────────────────────────────────

    std::string getPlayerName(int i) const
    {
        if (isShmMode()) return std::string(m_shm->players[i].getName());
        return (m_localPlayers[i]) ? m_localPlayers[i]->getName() : "???";
    }

    int getPlayerHp(int i) const
    {
        if (isShmMode()) return m_shm->players[i].getHp();
        return m_localPlayers[i] ? m_localPlayers[i]->getHp() : 0;
    }

    int getPlayerMaxHp(int i) const
    {
        if (isShmMode()) return m_shm->players[i].getMaxHp();
        return m_localPlayers[i] ? m_localPlayers[i]->getMaxHp() : 1;
    }

    int getPlayerStamina(int i) const
    {
        if (isShmMode()) return m_shm->players[i].getStamina();
        return m_localPlayers[i] ? m_localPlayers[i]->getStamina() : 0;
    }

    int getPlayerMaxStamina(int i) const
    {
        if (isShmMode()) return m_shm->players[i].getMaxStamina();
        return m_localPlayers[i] ? m_localPlayers[i]->getMaxStamina() : 1;
    }
    bool getPlayerAlive(int i) const
    {
        if (isShmMode()) return m_shm->players[i].isAlive();
        return m_localPlayers[i] ? m_localPlayers[i]->isAlive() : false;
    }

    bool getPlayerStunned(int i) const
    {
        if (isShmMode()) return m_shm->players[i].isStunned();
        return m_localPlayers[i] ? m_localPlayers[i]->isStunned() : false;
    }

    int getPlayerStunEnd(int i) const
    {
        if (isShmMode()) return m_shm->players[i].getStunEndTem();
        return m_localPlayers[i] ? m_localPlayers[i]->getStunEndTem() : 0;
    }

    std::string getEnemyName(int i) const
    {
        if (isShmMode()) return std::string(m_shm->enemies[i].getName());
        return m_localEnemies[i] ? dynamic_cast<Enemy*>(m_localEnemies[i])->getName() : "Enemy";
    }

    int getEnemyHp(int i) const
    {
        if (isShmMode()) return m_shm->enemies[i].getHp();
        return m_localEnemies[i] ? m_localEnemies[i]->getHp() : 0;
    }

    int getEnemyMaxHp(int i) const
    {
        if (isShmMode()) return m_shm->enemies[i].getMaxHp();
        return m_localEnemies[i] ? m_localEnemies[i]->getMaxHp() : 1;
    }

    bool getEnemyStunned(int i) const
    {
        if (isShmMode()) return m_shm->enemies[i].isStunned();
        return m_localEnemies[i] ? m_localEnemies[i]->isStunned() : false;
    }


    void loadPlayerSprites()
{
    int count = isShmMode()
                ? m_shm->num_active_players
                : (int)m_localPlayers.size();

    for (int i = 0; i < count; i++)
    {
        int typeIdx = isShmMode()
                      ? (int)m_shm->players[i].getPlayerType()
                      : (int)m_localPlayers[i]->getPlayerType();

        if (m_playerTextures[i].loadFromFile(PLAYER_SPRITE_PATHS[typeIdx]))
        {
            m_playerSprites[i].setTexture(m_playerTextures[i]);
            std::cout << "[RENDERER] Player " << i << " sprite loaded\n";
        }
        else
        {
            std::cerr << "[RENDERER] Failed to load player sprite: "
                      << PLAYER_SPRITE_PATHS[typeIdx] << "\n";
        }
    }
    m_playerSpritesLoaded = true;
}






    // ─────────────────────────────────────────────────────────────────────────
    //  Asset loading
    // ─────────────────────────────────────────────────────────────────────────

    void loadAssets()
    {
        m_fontLoaded = m_font.loadFromFile("../DisplayRendering/BlockBlueprint.ttf");
        if (!m_fontLoaded)
            std::cerr << "[Renderer] Font missing — text will not render\n";
        else
            std::cout << "[Renderer] Font loaded OK\n";

        struct WEntry { const char* name; const char* path; };
        WEntry weaponPaths[] = {
            { "Solar Core",     "../Weapons_sprites/Solar_Core.png"      },
            { "Lunar Blade",    "../Weapons_sprites/Lunar_Blade.png"     },
            { "Iron Halberd",   "../Weapons_sprites/Iron_Halberd.png"    },
            { "Venom Dagger",   "../Weapons_sprites/Venom_Dagger.png"    },
            { "Thunderstaff",   "../Weapons_sprites/Thunder_staff.png"   },
            { "Obsidian Axe",   "../Weapons_sprites/Obsidian_Axe.png"    },
            { "Frostbow",       "../Weapons_sprites/Frost_Bow.png"       },
            { "Splinter Stick", "../Weapons_sprites/Splinster_Stick.png" },
        };
        loadPlayerSprites();
        for (auto& e : weaponPaths)
        {
            sf::Texture tex;
            if (tex.loadFromFile(e.path))
            {
                m_weaponTextures[std::string(e.name)] = std::move(tex);
                std::cout << "[Renderer] Weapon sprite loaded: " << e.name << "\n";
            }
            else
                std::cerr << "[Renderer] Missing sprite: " << e.path << "\n";
        }
    }
// ─────────────────────────────────────────────────────────────────────────
    //  fireCallback
    //  FIX: validate it's actually this player's turn before firing
    // ─────────────────────────────────────────────────────────────────────────
    void fireCallback(Action action, int target, int weapon)
    {
        if (isShmMode())
        {
            if (!m_shm->is_player_turn)
            {
                std::cout << "[Renderer] Input ignored — not a player turn\n";
                return;
            }
            // FIX: make sure the active turn owner matches who we think is active
            if (m_shm->current_turn_owner_id != m_activeIdx)
            {
                std::cout << "[Renderer] Input ignored — turn owner mismatch ("
                          << m_shm->current_turn_owner_id
                          << " vs activeIdx=" << m_activeIdx << ")\n";
                return;
            }
        }
        if (m_actionCallback)
            m_actionCallback(action, target, weapon);
        else
            std::cerr << "[Renderer] WARNING: no action callback set!\n";
    }



    // ─────────────────────────────────────────────────────────────────────────
    //  handleEvents
    //  FIX: W key sends weapon ID (not slot index)
    //       Tab only fires in BACKPACK mode and sends backpack slot index
    //       Up/Down clamp against correct list for current sidebar mode
    // ─────────────────────────────────────────────────────────────────────────
    void handleEvents()
    {
        sf::Event e{};
        while (m_window.pollEvent(e))
        {
            if (e.type == sf::Event::Closed)
            {
                std::cout << "[Renderer] Window close requested\n";
                requestStop();
                m_window.close();
            }

            if (e.type == sf::Event::KeyPressed)
            {
                int total = totalEnemyCount();

                switch (e.key.code)
                {
                    // ── Enemy cursor ──────────────────────────────────────────
                    case sf::Keyboard::Right:
                        if (total > 0 && aliveEnemyCount() > 0)
                        {
                            do {
                                m_selectedEnemy = (m_selectedEnemy + 1) % total;
                            } while (!isEnemyAlive(m_selectedEnemy));
                            std::cout << "[Renderer] Selected enemy → "
                                      << m_selectedEnemy << "\n";
                        }
                        break;

                    case sf::Keyboard::Left:
                        if (total > 0 && aliveEnemyCount() > 0)
                        {
                            do {
                                m_selectedEnemy = (m_selectedEnemy - 1 + total) % total;
                            } while (!isEnemyAlive(m_selectedEnemy));
                            std::cout << "[Renderer] Selected enemy → "
                                      << m_selectedEnemy << "\n";
                        }
                        break;

                    // ── Weapon cursor — FIX: clamp against correct list ───────
                    case sf::Keyboard::Up:
                    {
                        int limit = (m_sidebarMode == SidebarMode::BACKPACK)
                            ? (int)getActivePlayerBackpack().size()
                            : (int)getActivePlayerInventory().size();
                        if (limit > 0)
                            m_selectedWeapon = (m_selectedWeapon - 1 + limit) % limit;
                        std::cout << "[Renderer] Selected weapon → "
                                  << m_selectedWeapon << "\n";
                        break;
                    }
                    case sf::Keyboard::Down:
                    {
                        int limit = (m_sidebarMode == SidebarMode::BACKPACK)
                            ? (int)getActivePlayerBackpack().size()
                            : (int)getActivePlayerInventory().size();
                        if (limit > 0)
                            m_selectedWeapon = (m_selectedWeapon + 1) % limit;
                        std::cout << "[Renderer] Selected weapon → "
                                  << m_selectedWeapon << "\n";
                        break;
                    }

                    // ── Actions ───────────────────────────────────────────────
                    case sf::Keyboard::Space:
                        std::cout << "[Renderer] SPACE → STRIKE  enemy="
                                  << m_selectedEnemy << "\n";
                        fireCallback(Action::STRIKE, m_selectedEnemy, -1);
                        break;

                    case sf::Keyboard::W:
                    {
                        // FIX: send the weapon's actual ID, not the slot index
                        // arbiter's USE_WEAPON calls getWeaponById(weapon_id)
                        std::vector<Weapon> inv = getActivePlayerInventory();
                        if (m_selectedWeapon >= 0
                            && m_selectedWeapon < (int)inv.size())
                        {
                            int wid = inv[m_selectedWeapon].getWeaponId();
                            std::cout << "[Renderer] W → USE_WEAPON  enemy="
                                      << m_selectedEnemy
                                      << "  weaponId=" << wid << "\n";
                            fireCallback(Action::USE_WEAPON, m_selectedEnemy, wid);
                        }
                        else
                            std::cout << "[Renderer] W ignored — no weapon selected\n";
                        break;
                    }

                    case sf::Keyboard::E:
                        std::cout << "[Renderer] E → EXHAUST  enemy="
                                  << m_selectedEnemy << "\n";
                        fireCallback(Action::EXHAUST, m_selectedEnemy, -1);
                        break;

                    case sf::Keyboard::H:
                        std::cout << "[Renderer] H → HEAL\n";
                        fireCallback(Action::HEAL, -1, -1);
                        break;

                    case sf::Keyboard::Tab:
                    {
                        // FIX: only fire in BACKPACK mode
                        // send backpack slot index (arbiter's swapInFromBackpack takes index)
                        if (m_sidebarMode == SidebarMode::BACKPACK)
                        {
                            std::vector<Weapon> bp = getActivePlayerBackpack();
                            if (m_selectedWeapon >= 0
                                && m_selectedWeapon < (int)bp.size())
                            {
                                std::cout << "[Renderer] TAB → SWAP_IN  backpackSlot="
                                          << m_selectedWeapon << "\n";
                                fireCallback(Action::SWAP_IN, -1, m_selectedWeapon);
                            }
                            else
                                std::cout << "[Renderer] TAB ignored — no backpack item selected\n";
                        }
                        else
                            std::cout << "[Renderer] TAB ignored — switch to BACKPACK tab first\n";
                        break;
                    }

                    case sf::Keyboard::Escape:
                        std::cout << "[Renderer] ESC → SKIP\n";
                        fireCallback(Action::SKIP, -1, -1);
                        break;

                    default: break;
                }
            }
        }

        // ── Mouse click — sidebar tab buttons + log toggle ────────────────────
        bool mouseDown  = sf::Mouse::isButtonPressed(sf::Mouse::Left);
        bool clicked    = mouseDown && !m_prevMouseDown;
        m_prevMouseDown = mouseDown;

        if (clicked)
        {
            sf::Vector2i mp = sf::Mouse::getPosition(m_window);
            float mx = (float)mp.x;
            float my = (float)mp.y;

            if (my >= BTN_Y && my < BTN_Y + BTN_H)
            {
                float b1x = SB_X + PAD;
                float b2x = b1x + BTN_W + 2.f;
                float b3x = b2x + BTN_W + 2.f;

                if      (mx >= b1x && mx < b1x + BTN_W)
                { m_sidebarMode = SidebarMode::ENEMIES;    m_selectedWeapon = 0; }
                else if (mx >= b2x && mx < b2x + BTN_W)
                { m_sidebarMode = SidebarMode::INVENTORY;  m_selectedWeapon = 0; }
                else if (mx >= b3x && mx < b3x + BTN_W)
                { m_sidebarMode = SidebarMode::BACKPACK;   m_selectedWeapon = 0; }
            }

            float logBtnX = SB_X + SB_W - PAD - 24.f;
            float logBtnY = LOG_Y + 4.f;
            if (mx >= logBtnX && mx < logBtnX + 22.f
             && my >= logBtnY && my < logBtnY + 18.f)
                m_logExpanded = !m_logExpanded;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawAll — master draw call
    // ─────────────────────────────────────────────────────────────────────────
    void drawAll(float dt)
    {
        if (isShmMode())
        {
            int  owner    = m_shm->current_turn_owner_id;
            bool isPlayer = m_shm->is_player_turn;
            int  numP     = m_shm->num_active_players;

            if (isPlayer && owner >= 0 && owner < numP)
            {
                m_activeTurnId        = owner;
                m_activeIsPlayer      = true;
                m_activeIdx           = owner;   // FIX: keep in sync
                m_lastActivePlayerIdx = owner;
            }
            else if (!isPlayer && owner >= numP)
            {
                m_activeTurnId   = owner;
                m_activeIsPlayer = false;
                // m_activeIdx and m_lastActivePlayerIdx stay on last player
                // so sidebar keeps showing that player's stats during enemy turn
            }
        }

        if (m_map) m_map->draw(m_window);
        else       std::cerr << "[Renderer] Map pointer is null\n";

        drawPlayers();
        updateAndDrawEnemies(m_window, dt);
        drawTurnBanner();
        drawSidebarBg();
        drawActiveSection();
        drawToggleButtons();
        drawActionLog();

        switch (m_sidebarMode)
        {
            case SidebarMode::ENEMIES:   drawEnemySection();   break;
            case SidebarMode::INVENTORY: drawInventoryPanel(); break;
            case SidebarMode::BACKPACK:  drawBackpackPanel();  break;
        }

        drawHUD();
    }

std::vector<Weapon> getActivePlayerInventory() const
    {
        std::vector<Weapon> out;
        int pi = m_activeIdx;

        if (isShmMode())
        {
            if (pi < 0 || pi >= m_shm->num_active_players) return out;
            for (const auto& pair :
                 m_shm->players[pi].getInventory().getEquippedWeapons())
                out.push_back(pair.second);
            return out;
        }
        if (pi < 0 || pi >= (int)m_localPlayers.size()) return out;
        Player* p = m_localPlayers[pi];
        if (!p) return out;
        for (const auto& pair : p->getInventory().getEquippedWeapons())
            out.push_back(pair.second);
        return out;
    }

    std::vector<Weapon> getActivePlayerBackpack() const
    {
        int pi = m_activeIdx;
        if (isShmMode())
        {
            if (pi < 0 || pi >= m_shm->num_active_players) return {};
            return m_shm->players[pi].getBackpack().getWeapons();
        }
        if (pi < 0 || pi >= (int)m_localPlayers.size()) return {};
        Player* p = m_localPlayers[pi];
        if (!p) return {};
        return p->getBackpack().getWeapons();
    }










void drawPlayers()
{
    if (!m_playerSpritesLoaded) return;

    int count = isShmMode()
                ? m_shm->num_active_players
                : (int)m_localPlayers.size();

    for (int i = 0; i < count; i++)
    {
        // Pull position + scale straight from shm each frame
        float x, y, sx, sy;
        bool  alive;

        if (isShmMode())
        {
            const Player& p = m_shm->players[i];
            if (!p.isAlive()) continue;
            x  = p.getXPos();
            y  = p.getYPos();
            sx = p.getScaleX();
            sy = p.getScaleY();

        }
        else
        {
            const Player* p = static_cast<Player*>(m_localPlayers[i]);
            if (!p->isAlive()) continue;
            x  = p->getXPos();
            y  = p->getYPos();
            sx = p->getScaleX();
            sy = p->getScaleY();
        }

        m_playerSprites[i].setPosition(x, y);
        m_playerSprites[i].setScale(sx, sy);
        m_window.draw(m_playerSprites[i]);
    }
}




    // ─────────────────────────────────────────────────────────────────────────
    //  drawTurnBanner
    // ─────────────────────────────────────────────────────────────────────────

    void drawTurnBanner()
    {
        std::string banner;
        if (m_activeIsPlayer)
        {
            if (m_activeTurnId < totalPlayerCount())
                banner = getPlayerName(m_activeTurnId) + "'s TURN";
        }
        else
        {
            int eIdx = m_activeTurnId - totalPlayerCount();
            if (eIdx >= 0 && eIdx < totalEnemyCount())
                banner = getEnemyName(eIdx) + "'s TURN";
            else
                banner = "ENEMY TURN";
        }

        drawRect(0.f, 0.f, MAP_W, 32.f, sf::Color(0, 0, 0, 160));
        drawText(banner, 12.f, 6.f, FONT_MD, Colour::ActiveTurn);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawHUD
    // ─────────────────────────────────────────────────────────────────────────
void drawHUD()
{
    if (isShmMode() && !m_shm->is_player_turn) return;

    drawRect(0.f, WIN_H - 26.f, MAP_W, 26.f, sf::Color(0, 0, 0, 180));
    drawText(
        "SPACE=Strike  W=Weapon  E=Exhaust  H=Heal  TAB=Swap  ESC=Skip   </>=Target  ^/v=Weapon",
        8.f, WIN_H - 22.f, FONT_XS, Colour::TxtMuted
    );
}


    // ─────────────────────────────────────────────────────────────────────────
    //  drawSidebarBg
    // ─────────────────────────────────────────────────────────────────────────

    void drawSidebarBg()
    {
        drawRect(SB_X, 0.f, SB_W, SB_H, Colour::SidebarBg);
        drawRect(SB_X, 0.f, 2.f,  SB_H, Colour::Divider);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawActiveSection — active player's HP / stamina / status
    // ─────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────
    //  drawActiveSection
    //  FIX: removed dangling displayIdx, removed duplicate total declaration,
    //       all stats now read from pi (= m_activeIdx, clamped)
    // ─────────────────────────────────────────────────────────────────────────
    void drawActiveSection()
    {
        drawRect(SB_X, SEC_ACTIVE_Y, SB_W, SEC_ACTIVE_H, Colour::SectionBg);
        drawDivider(SEC_ACTIVE_Y + SEC_ACTIVE_H);

        int total = totalPlayerCount();
        if (total == 0) return;

        // FIX: derive display index from m_activeIdx, clamped
        int pi = std::max(0, std::min(m_activeIdx, total - 1));

        float x = SB_X + PAD;
        float y = SEC_ACTIVE_Y + 6.f;

        // ── Name + status badge ───────────────────────────────────────────────
        drawText(getPlayerName(pi), x, y, FONT_MD, Colour::TxtName);
        drawStatusBadge(SB_X + SB_W - PAD - 70.f, y + 1.f,
                        getPlayerAlive(pi),
                        getPlayerStunned(pi));
        y += 24.f;

        // ── HP ────────────────────────────────────────────────────────────────
        bool      stunned = getPlayerStunned(pi);
        float     hpRatio = (float)getPlayerHp(pi)
                          / (float)std::max(1, getPlayerMaxHp(pi));
        sf::Color hpCol   = stunned        ? Colour::StunBadge
                          : hpRatio < 0.3f ? Colour::HpLow
                                           : Colour::HpFull;

        drawText("HP  " + std::to_string(getPlayerHp(pi))
                 + " / " + std::to_string(getPlayerMaxHp(pi)),
                 x, y, FONT_XS, Colour::TxtMuted);
        y += 14.f;
        drawBar(x, y, SB_W - PAD * 2, 12.f,
                getPlayerHp(pi), getPlayerMaxHp(pi),
                Colour::HpBack, hpCol);
        y += 18.f;

        // ── Stamina ───────────────────────────────────────────────────────────
        drawText("STM " + std::to_string(getPlayerStamina(pi))
                 + " / " + std::to_string(getPlayerMaxStamina(pi)),
                 x, y, FONT_XS, Colour::TxtMuted);
        y += 14.f;
        drawBar(x, y, SB_W - PAD * 2, 8.f,
                getPlayerStamina(pi), getPlayerMaxStamina(pi),
                Colour::StamBack, Colour::StamFull);
        y += 16.f;

        // ── Party mini-row ────────────────────────────────────────────────────
        drawText("PARTY", x, y, FONT_XS, Colour::TxtMuted);
        y += 14.f;

        float miniW = (SB_W - PAD * 2 - (total - 1) * 4.f) / (float)total;
        for (int i = 0; i < total; i++)
        {
            float mx = x + i * (miniW + 4.f);
            sf::Color miniCol = (i == pi)
                ? Colour::ActiveTurn
                : (isPlayerAlive(i) ? Colour::HpFull : Colour::DeadBadge);

            drawBar(mx, y, miniW, 10.f,
                    getPlayerHp(i), std::max(1, getPlayerMaxHp(i)),
                    Colour::HpBack, miniCol);

            std::string abbr = getPlayerName(i).substr(0, 3);
            drawText(abbr, mx + 2.f, y + 12.f, FONT_XS - 2, Colour::TxtMuted);
        }
        y += 28.f;

        if (stunned)
            drawText("STUNNED — clears T"
                     + std::to_string(getPlayerStunEnd(pi)),
                     x, y, FONT_XS, Colour::StunBadge);
    }


    // ─────────────────────────────────────────────────────────────────────────
    //  drawActionLog
    // ─────────────────────────────────────────────────────────────────────────

    void drawActionLog()
    {
        drawRect(SB_X, LOG_Y, SB_W, LOG_H,
                 sf::Color(18, 18, 35), Colour::Divider, 1.f);
        drawText("ACTION LOG", SB_X + PAD, LOG_Y + 6.f, FONT_XS, Colour::TxtMuted);

        // Expand/collapse button
        float btnX = SB_X + SB_W - PAD - 24.f;
        float btnY = LOG_Y + 4.f;
        drawRect(btnX, btnY, 22.f, 18.f,
                 Colour::BtnInactive, Colour::BtnBorder, 1.f);
        drawText(m_logExpanded ? "^" : "v",
                 btnX + 6.f, btnY + 1.f, FONT_XS, Colour::TxtName);

        if (!m_block)
        {
            drawText("(local mode — no log)",
                     SB_X + PAD, LOG_Y + 26.f, FONT_XS, Colour::TxtMuted);
            return;
        }

        ActionLog& log = m_block->state.action_log;
        if (log.count == 0)
        {
            drawText("No actions yet.",
                     SB_X + PAD, LOG_Y + 26.f, FONT_XS, Colour::TxtMuted);
            return;
        }

        int   start = std::max(0, log.count - (int)LOG_PREVIEW);
        float lineY = LOG_Y + 26.f;
        for (int i = start; i < log.count; i++)
        {
            int       idx = (log.head + i) % ACTION_LOG_SIZE;
            float     t   = (float)(i - start) / (float)LOG_PREVIEW;
            sf::Uint8 a   = (sf::Uint8)(120 + 135 * t);
            drawText(log.messages[idx], SB_X + PAD, lineY, FONT_XS,
                     sf::Color(210, 210, 230, a));
            lineY += 20.f;
        }

        if (m_logExpanded) drawLogOverlay(log);
    }

    void drawLogOverlay(ActionLog& log)
    {
        constexpr float OV_W = 580.f;
        constexpr float OV_H = 240.f;
        constexpr float OV_X = SB_X - OV_W - 8.f;
        constexpr float OV_Y = LOG_Y;

        drawRect(OV_X + 4.f, OV_Y + 4.f, OV_W, OV_H, sf::Color(0, 0, 0, 120));
        drawRect(OV_X, OV_Y, OV_W, OV_H,
                 sf::Color(12, 12, 24, 245), Colour::Divider, 1.f);
        drawText("[ Full Action Log ]",
                 OV_X + PAD, OV_Y + 8.f, FONT_SM, Colour::TxtName);

        float lineY = OV_Y + 30.f;
        for (int i = 0; i < log.count; i++)
        {
            int       idx = (log.head + i) % ACTION_LOG_SIZE;
            float     t   = (float)i / std::max(1, log.count - 1);
            sf::Uint8 a   = (sf::Uint8)(80 + 175 * t);
            drawText(log.messages[idx], OV_X + PAD, lineY, FONT_XS,
                     sf::Color(210, 210, 230, a));
            lineY += 20.f;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawToggleButtons
    // ─────────────────────────────────────────────────────────────────────────

    void drawToggleButtons()
    {
        struct Btn { const char* label; SidebarMode mode; };
        Btn btns[3] = {
            { "ENEMIES",   SidebarMode::ENEMIES   },
            { "INVENTORY", SidebarMode::INVENTORY },
            { "BACKPACK",  SidebarMode::BACKPACK  },
        };

        float bx = SB_X + PAD;
        for (int i = 0; i < 3; i++)
        {
            bool active = (m_sidebarMode == btns[i].mode);
            drawRect(bx, BTN_Y, BTN_W, BTN_H,
                     active ? Colour::BtnActive : Colour::BtnInactive,
                     Colour::BtnBorder, 1.f);
            drawText(btns[i].label,
                     bx + 6.f, BTN_Y + (BTN_H - FONT_XS) / 2.f - 2.f,
                     FONT_XS,
                     active ? Colour::TxtName : Colour::TxtMuted);
            bx += BTN_W + 2.f;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawEnemySection
    // ─────────────────────────────────────────────────────────────────────────

void drawEnemySection()
{
    drawRect(SB_X, PANEL_Y, SB_W, PANEL_H, Colour::SidebarBg);

    int count = totalEnemyCount();

    // Header with count
    drawText("ENEMIES  [" + std::to_string(aliveEnemyCount())
             + "/" + std::to_string(count) + " alive]",
             SB_X + PAD, PANEL_Y + 4.f, FONT_XS, Colour::TxtMuted);

    if (count == 0)
    {
        drawText("No enemies", SB_X + PAD, PANEL_Y + 24.f, FONT_SM, Colour::TxtMuted);
        return;
    }

    // Dynamic card height — shrink if many enemies
    int   visible  = count;
    float maxH     = PANEL_H - 22.f;
    float cardH    = std::min(ENEMY_CARD_H,
                              (maxH - (visible - 1) * ENEMY_CARD_GAP) / (float)visible);
    cardH          = std::max(cardH, 44.f);   // never smaller than 44px

    float cardY = PANEL_Y + 22.f;

    for (int i = 0; i < count; ++i)
    {
        if (cardY + cardH > PANEL_Y + PANEL_H) break;

        bool isSelected = (i == m_selectedEnemy);
        bool alive      = isEnemyAlive(i);
        bool stunned    = getEnemyStunned(i);

        sf::Color cardBg = isSelected
            ? sf::Color(60, 55, 20, 220)
            : (alive ? Colour::SectionBg : sf::Color(35, 20, 20, 200));

        float cx = SB_X + PAD;
        float cw = SB_W - PAD * 2;
        drawRect(cx, cardY, cw, cardH, cardBg, Colour::Divider, 1.f);

        float tx = cx + 6.f;
        float ty = cardY + 4.f;

        // ── Name + selector arrow ─────────────────────────────────────────
        std::string prefix = isSelected ? "> " : "  ";
        sf::Color nameCol  = alive ? Colour::TxtPrimary : Colour::TxtMuted;
        drawText(prefix + getEnemyName(i), tx, ty, FONT_SM, nameCol);

        // ── Status badge (right side) ─────────────────────────────────────
        drawStatusBadge(SB_X + SB_W - PAD - 72.f, ty, alive, stunned);

        ty += 20.f;

        // ── HP bar — only if card is tall enough ──────────────────────────
        if (cardH >= 44.f)
        {
            int hp    = getEnemyHp(i);
            int maxhp = std::max(1, getEnemyMaxHp(i));

            sf::Color eHpCol = !alive   ? Colour::DeadBadge
                             : stunned  ? Colour::StunBadge
                             : ((float)hp / maxhp < 0.3f ? Colour::HpLow : Colour::HpFull);

            // HP numbers inline
            drawText(std::to_string(hp) + "/" + std::to_string(maxhp),
                     tx, ty, FONT_XS, Colour::TxtMuted);
            ty += 13.f;

            float barW = cw - 12.f;
            drawBar(tx, ty, barW, 8.f, hp, maxhp, Colour::HpBack, eHpCol);
        }

        cardY += cardH + ENEMY_CARD_GAP;
    }
}


    // ─────────────────────────────────────────────────────────────────────────
    //  drawWeaponPanel
    // ─────────────────────────────────────────────────────────────────────────
void drawWeaponPanel(const std::vector<Weapon>& weapons, const char* header)
{
    drawRect(SB_X, PANEL_Y, SB_W, PANEL_H, Colour::SidebarBg);
    drawText(header, SB_X + PAD, PANEL_Y + 6.f, FONT_XS, Colour::TxtMuted);

    // Guard: snapshot the vector size once, never re-query mid-draw
    const int weaponCount = (int)weapons.size();

    if (weaponCount == 0)
    {
        drawText("No weapons equipped.",
                 SB_X + PAD, PANEL_Y + 28.f, FONT_SM, Colour::TxtMuted);
        return;
    }

    // Clamp selected weapon cursor so it never goes out of bounds
    if (m_selectedWeapon >= weaponCount)
        m_selectedWeapon = weaponCount - 1;
    if (m_selectedWeapon < 0)
        m_selectedWeapon = 0;

    std::unordered_map<std::string, int> countByName;
    std::vector<const Weapon*>           unique;
    for (const Weapon& w : weapons)
    {
        if (countByName.find(w.getName()) == countByName.end())
            unique.push_back(&w);
        countByName[w.getName()]++;
    }

    constexpr float CARD_H  = 66.f;
    constexpr float ICON_SZ = 50.f;
    constexpr float GAP     = 6.f;
    float cardY = PANEL_Y + 24.f;

    for (int wi = 0; wi < (int)unique.size(); wi++)
    {
        if (cardY + CARD_H > PANEL_Y + PANEL_H) break;
        const Weapon* w = unique[wi];

        sf::Color cardBg = (wi == m_selectedWeapon)
                           ? sf::Color(20, 50, 70, 220)
                           : Colour::WeaponCard;

        float cx = SB_X + PAD;
        drawRect(cx, cardY, SB_W - PAD * 2, CARD_H,
                 cardBg, Colour::Divider, 1.f);

        std::string nameKey = w->getName();
        if (m_weaponTextures.count(nameKey))
        {
            sf::Sprite icon(m_weaponTextures.at(nameKey));
            icon.setPosition(cx + 6.f, cardY + 8.f);
            icon.setScale(
                ICON_SZ / icon.getTexture()->getSize().x,
                ICON_SZ / icon.getTexture()->getSize().y
            );
            m_window.draw(icon);
        }
        else
        {
            drawRect(cx + 6.f, cardY + 8.f, ICON_SZ, ICON_SZ,
                     {60, 60, 100}, Colour::BtnBorder, 1.f);
            drawText(std::string(1, w->getName()[0]),
                     cx + 22.f, cardY + 18.f, FONT_LG, Colour::TxtName);
        }

        float tx = cx + ICON_SZ + 16.f;
        float ty = cardY + 8.f;

        std::string prefix = (wi == m_selectedWeapon) ? "> " : "  ";
        drawText(prefix + w->getName(), tx, ty, FONT_SM, Colour::TxtPrimary);
        ty += 18.f;
        drawText("DMG: "   + std::to_string(w->getDamage()),
                 tx, ty, FONT_SM, Colour::DmgColour);
        ty += 16.f;
        drawText("Slots: " + std::to_string(w->getSlotSize()),
                 tx, ty, FONT_XS, Colour::TxtMuted);

        int cnt = countByName[w->getName()];
        if (cnt > 1)
        {
            float badgeX = SB_X + SB_W - PAD - 32.f;
            float badgeY = cardY + 8.f;
            drawRect(badgeX, badgeY, 30.f, 20.f, Colour::CountBadge);
            drawText("x" + std::to_string(cnt),
                     badgeX + 5.f, badgeY + 3.f,
                     FONT_XS, sf::Color::Black);
        }

        cardY += CARD_H + GAP;
    }
}

void drawInventoryPanel()
{
    std::string header = "INVENTORY";
    if (m_activeIdx >= 0 && m_activeIdx < totalPlayerCount())
        header = "INVENTORY  [" + getPlayerName(m_activeIdx) + "]";

    drawWeaponPanel(getActivePlayerInventory(), header.c_str());
}

void drawBackpackPanel()
{
    std::string header = "BACKPACK (LTS)";
    if (m_activeIdx >= 0 && m_activeIdx < totalPlayerCount())
        header = "BACKPACK  [" + getPlayerName(m_activeIdx) + "]";

    drawWeaponPanel(getActivePlayerBackpack(), header.c_str());
}




    // ─────────────────────────────────────────────────────────────────────────
    //  Primitive helpers
    // ─────────────────────────────────────────────────────────────────────────

    void drawBar(float x, float y, float w, float h,
                 int current, int max,
                 sf::Color bgCol, sf::Color fillCol)
    {
        float pct = (max > 0)
            ? std::clamp((float)current / (float)max, 0.f, 1.f)
            : 0.f;
        drawRect(x, y, w, h, bgCol);
        if (pct > 0.f) drawRect(x, y, w * pct, h, fillCol);
        drawRect(x, y, w, h, sf::Color::Transparent, Colour::Divider, 1.f);
    }

    void drawStatusBadge(float x, float y, bool alive, bool stunned)
    {
        std::string lbl;
        sf::Color   col;
        if      (!alive)  { lbl = "DEAD";    col = Colour::DeadBadge;  }
        else if (stunned) { lbl = "STUNNED"; col = Colour::StunBadge;  }
        else              { lbl = "ALIVE";   col = Colour::AliveBadge; }
        drawRect(x, y, 66.f, 18.f, col);
        drawText(lbl, x + 4.f, y + 2.f, FONT_XS, sf::Color::Black);
    }

    void drawDivider(float y)
    {
        drawRect(SB_X, y, SB_W, 1.f, Colour::Divider);
    }

    void drawText(const std::string& s, float x, float y,
                  unsigned int size, sf::Color col)
    {
        if (!m_fontLoaded || s.empty()) return;
        sf::Text t;
        t.setFont(m_font);
        t.setString(s);
        t.setCharacterSize(size);
        t.setFillColor(col);
        t.setPosition(x, y);
        m_window.draw(t);
    }

    void drawRect(float x, float y, float w, float h,
                  sf::Color fill,
                  sf::Color outline      = sf::Color::Transparent,
                  float     outlineThick = 0.f)
    {
        sf::RectangleShape r({w, h});
        r.setPosition(x, y);
        r.setFillColor(fill);
        r.setOutlineColor(outline);
        r.setOutlineThickness(outlineThick);
        m_window.draw(r);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Testing helper
// ─────────────────────────────────────────────────────────────────────────────
inline void seedTestWeapons(Player* p)
{
    struct WDef { WeaponType t; const char* name; int slots; int dmg; };
    WDef defs[] = {
        { WeaponType::ARTIFACT,       "Solar Core",     10, 95 },
        { WeaponType::ARTIFACT,       "Lunar Blade",    10, 90 },
        { WeaponType::IRON_HALBERD,   "Iron Halberd",    7, 55 },
        { WeaponType::VENOM_DAGGER,   "Venom Dagger",    4, 30 },
        { WeaponType::SPLINTER_STICK, "Splinter Stick",  2, 12 },
        { WeaponType::SPLINTER_STICK, "Splinter Stick",  2, 12 },
    };
    int id = 1;
    for (auto& d : defs)
    {
        Weapon w(id++, d.t, d.name, d.slots, d.dmg);
        p->pickupWeapon(w);
    }
}
