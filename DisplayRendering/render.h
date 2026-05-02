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
constexpr float WIN_W = 1200.f;
constexpr float WIN_H = 800.f;
constexpr float MAP_W = 800.f;
constexpr float MAP_H = 800.f;
constexpr float SB_X  = 800.f;
constexpr float SB_W  = 400.f;
constexpr float SB_H  = 800.f;
constexpr float PAD   = 10.f;

constexpr float SEC_ACTIVE_Y = 0.f;
constexpr float SEC_ACTIVE_H = 290.f;

constexpr float LOG_Y       = SEC_ACTIVE_Y + SEC_ACTIVE_H + 4.f;
constexpr float LOG_H       = 120.f;
constexpr float LOG_PREVIEW = 4;

constexpr float BTN_Y = LOG_Y + LOG_H + 4.f;
constexpr float BTN_H = 30.f;
constexpr float BTN_W = (SB_W - PAD * 2 - 4.f) / 3.f;

constexpr float PANEL_Y = BTN_Y + BTN_H + 4.f;
constexpr float PANEL_H = SB_H - PANEL_Y;

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
        m_window.setFramerateLimit(60);
        loadAssets();
        std::cout << "[Renderer] Window open — entering game loop\n";

        while (m_window.isOpen())
        {
            pthread_mutex_lock(&m_stopMutex);
            bool stop = m_stop;
            pthread_mutex_unlock(&m_stopMutex);
            if (stop) { m_window.close(); break; }

            handleEvents();
            m_window.clear(Colour::SidebarBg);
            drawAll();
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

    SidebarMode m_sidebarMode;

    // Selection cursors (arrow keys)
    int m_selectedEnemy  = 0;
    int m_selectedWeapon = 0;

    // SFML
    sf::RenderWindow m_window;
    sf::Font         m_font;
    bool             m_fontLoaded = false;

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

    // ─────────────────────────────────────────────────────────────────────────
    //  Weapon helpers — local mode only
    //  In shm mode weapon data lives in the arbiter, not accessible here
    // ─────────────────────────────────────────────────────────────────────────

    std::vector<Weapon> getActivePlayerInventory() const
    {
        if (isShmMode()) return {};
        if (m_activeIdx >= (int)m_localPlayers.size()) return {};
        Player* p = m_localPlayers[m_activeIdx];
        if (!p) return {};
        std::vector<Weapon> out;
        for (const auto& pair : p->getInventory().getEquippedWeapons())
            out.push_back(pair.second);
        return out;
    }

    std::vector<Weapon> getActivePlayerBackpack() const
    {
        if (isShmMode()) return {};
        if (m_activeIdx >= (int)m_localPlayers.size()) return {};
        Player* p = m_localPlayers[m_activeIdx];
        if (!p) return {};
        return p->getBackpack().getWeapons();
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
    //  fireCallback — guards against enemy-turn keypresses
    // ─────────────────────────────────────────────────────────────────────────

    void fireCallback(Action action, int target, int weapon)
    {
        if (isShmMode() && !m_shm->is_player_turn)
        {
            std::cout << "[Renderer] Input ignored — not a player turn\n";
            return;
        }
        if (m_actionCallback)
            m_actionCallback(action, target, weapon);
        else
            std::cerr << "[Renderer] WARNING: no action callback set!\n";
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  handleEvents
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

                    // ── Weapon cursor ─────────────────────────────────────────
                    case sf::Keyboard::Up:
                        m_selectedWeapon = std::max(0, m_selectedWeapon - 1);
                        std::cout << "[Renderer] Selected weapon → "
                                  << m_selectedWeapon << "\n";
                        break;

                    case sf::Keyboard::Down:
                        m_selectedWeapon++;
                        std::cout << "[Renderer] Selected weapon → "
                                  << m_selectedWeapon << "\n";
                        break;

                    // ── Actions ───────────────────────────────────────────────
                    case sf::Keyboard::Space:
                        std::cout << "[Renderer] SPACE → STRIKE  enemy="
                                  << m_selectedEnemy << "\n";
                        fireCallback(Action::STRIKE, m_selectedEnemy, -1);
                        break;

                    case sf::Keyboard::W:
                        std::cout << "[Renderer] W → USE_WEAPON  enemy="
                                  << m_selectedEnemy
                                  << "  weapon=" << m_selectedWeapon << "\n";
                        fireCallback(Action::USE_WEAPON,
                                     m_selectedEnemy, m_selectedWeapon);
                        break;

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
                        std::cout << "[Renderer] TAB → SWAP_IN  slot="
                                  << m_selectedWeapon << "\n";
                        fireCallback(Action::SWAP_IN, -1, m_selectedWeapon);
                        break;

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

            float b1x = SB_X + PAD;
            float b2x = b1x + BTN_W + 2.f;
            float b3x = b2x + BTN_W + 2.f;

            if (mx >= b1x && mx < b1x + BTN_W
             && my >= BTN_Y && my < BTN_Y + BTN_H)
            {
                m_sidebarMode = SidebarMode::ENEMIES;
                std::cout << "[Renderer] Tab → ENEMIES\n";
            }
            if (mx >= b2x && mx < b2x + BTN_W
             && my >= BTN_Y && my < BTN_Y + BTN_H)
            {
                m_sidebarMode = (m_sidebarMode == SidebarMode::INVENTORY)
                                ? SidebarMode::ENEMIES
                                : SidebarMode::INVENTORY;
                std::cout << "[Renderer] Tab → INVENTORY\n";
            }
            if (mx >= b3x && mx < b3x + BTN_W
             && my >= BTN_Y && my < BTN_Y + BTN_H)
            {
                m_sidebarMode = (m_sidebarMode == SidebarMode::BACKPACK)
                                ? SidebarMode::ENEMIES
                                : SidebarMode::BACKPACK;
                std::cout << "[Renderer] Tab → BACKPACK\n";
            }

            float logBtnX = SB_X + SB_W - PAD - 24.f;
            float logBtnY = LOG_Y + 4.f;
            if (mx >= logBtnX && mx < logBtnX + 22.f
             && my >= logBtnY && my < logBtnY + 18.f)
            {
                m_logExpanded = !m_logExpanded;
                std::cout << "[Renderer] Log expanded → "
                          << (m_logExpanded ? "YES" : "NO") << "\n";
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawAll — master draw call, called every frame
    // ─────────────────────────────────────────────────────────────────────────

    void drawAll()
    {
        // Map background
        if (m_map) m_map->draw(m_window);
        else       std::cerr << "[Renderer] Map pointer is null\n";

        // Local mode only — draw character sprites via their own draw()
        if (!isShmMode())
        {
            for (Player*    p : m_localPlayers) if (p) p->draw(m_window);
            for (Character* e : m_localEnemies) if (e) e->draw(m_window);
        }

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
        // Only show controls when it's a player's turn
        if (isShmMode() && !m_shm->is_player_turn) return;

        drawRect(0.f, WIN_H - 26.f, MAP_W, 26.f, sf::Color(0, 0, 0, 180));
        drawText(
            "SPACE=Strike  W=Weapon  E=Exhaust  H=Heal"
            "  TAB=Swap  ESC=Skip   ←/→=Target  ↑/↓=Weapon",
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

    void drawActiveSection()
    {
        drawRect(SB_X, SEC_ACTIVE_Y, SB_W, SEC_ACTIVE_H, Colour::SectionBg);
        drawDivider(SEC_ACTIVE_Y + SEC_ACTIVE_H);

        int total = totalPlayerCount();
        if (total == 0 || m_activeIdx >= total) return;

        float x = SB_X + PAD;
        float y = SEC_ACTIVE_Y + PAD;

        drawText("ACTIVE PLAYER", x, y, FONT_XS, Colour::TxtMuted);
        y += 18.f;

        drawText(getPlayerName(m_activeIdx), x, y, FONT_LG, Colour::TxtName);
        y += 36.f;

        drawStatusBadge(x, y,
                        isPlayerAlive(m_activeIdx),
                        getPlayerStunned(m_activeIdx));
        y += 28.f;

        // HP row
        drawText("HP", x, y, FONT_XS, Colour::TxtMuted);
        drawText(
            std::to_string(getPlayerHp(m_activeIdx)) + " / " +
            std::to_string(getPlayerMaxHp(m_activeIdx)),
            SB_X + SB_W - PAD - 72.f, y, FONT_SM, Colour::TxtMuted
        );
        y += 18.f;

        bool stunned = getPlayerStunned(m_activeIdx);
        float hpRatio = (float)getPlayerHp(m_activeIdx)
                      / (float)getPlayerMaxHp(m_activeIdx);
        sf::Color hpCol = stunned ? Colour::StunBadge
                        : (hpRatio < 0.3f ? Colour::HpLow : Colour::HpFull);

        drawBar(x, y, SB_W - PAD * 2, 20.f,
                getPlayerHp(m_activeIdx), getPlayerMaxHp(m_activeIdx),
                Colour::HpBack, hpCol);
        y += 30.f;

        // Stamina row
        drawText("Stamina", x, y, FONT_XS, Colour::TxtMuted);
        drawText(
            std::to_string(getPlayerStamina(m_activeIdx)) + " / " +
            std::to_string(getPlayerMaxStamina(m_activeIdx)),
            SB_X + SB_W - PAD - 72.f, y, FONT_SM, Colour::TxtMuted
        );
        y += 18.f;

        drawBar(x, y, SB_W - PAD * 2, 14.f,
                getPlayerStamina(m_activeIdx), getPlayerMaxStamina(m_activeIdx),
                Colour::StamBack, Colour::StamFull);
        y += 26.f;

        if (stunned)
            drawText(
                "STUNNED  —  clears T" +
                std::to_string(getPlayerStunEnd(m_activeIdx)),
                x, y, FONT_SM, Colour::StunBadge
            );
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
        drawText("ENEMIES", SB_X + PAD, PANEL_Y + 6.f, FONT_XS, Colour::TxtMuted);

        constexpr float CARD_H = 88.f;
        constexpr float GAP    = 6.f;
        float cardY = PANEL_Y + 24.f;

        int count = totalEnemyCount();
        for (int i = 0; i < count; ++i)
        {
            if (cardY + CARD_H > PANEL_Y + PANEL_H) break;

            sf::Color cardBg = (i == m_selectedEnemy)
                               ? sf::Color(60, 55, 20, 200)
                               : Colour::SectionBg;

            drawRect(SB_X + PAD, cardY, SB_W - PAD * 2, CARD_H,
                     cardBg, Colour::Divider, 1.f);

            float cx = SB_X + PAD + 8.f;
            float cy = cardY + 8.f;

            std::string prefix = (i == m_selectedEnemy) ? "> " : "  ";
            drawText(prefix + getEnemyName(i), cx, cy, FONT_MD, Colour::TxtPrimary);

            bool alive   = isEnemyAlive(i);
            bool stunned = getEnemyStunned(i);
            drawStatusBadge(SB_X + SB_W - PAD - 70.f, cy + 1.f, alive, stunned);
            cy += 26.f;

            int hp    = getEnemyHp(i);
            int maxhp = getEnemyMaxHp(i);

            drawText("HP", cx, cy, FONT_SM, Colour::TxtMuted);
            drawText(std::to_string(hp) + " / " + std::to_string(maxhp),
                     cx + 30.f, cy, FONT_SM, Colour::TxtMuted);
            cy += 18.f;

            sf::Color eHpCol = stunned
                ? Colour::StunBadge
                : ((float)hp / maxhp < 0.3f ? Colour::HpLow : Colour::HpFull);

            drawBar(cx, cy, SB_W - PAD * 2 - 16.f, 12.f,
                    hp, maxhp, Colour::HpBack, eHpCol);

            cardY += CARD_H + GAP;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawWeaponPanel
    // ─────────────────────────────────────────────────────────────────────────

    void drawWeaponPanel(const std::vector<Weapon>& weapons, const char* header)
    {
        drawRect(SB_X, PANEL_Y, SB_W, PANEL_H, Colour::SidebarBg);
        drawText(header, SB_X + PAD, PANEL_Y + 6.f, FONT_XS, Colour::TxtMuted);

        if (weapons.empty())
        {
            std::string msg = isShmMode()
                ? "Weapon data owned by arbiter"
                : "Empty";
            drawText(msg, SB_X + PAD, PANEL_Y + 28.f, FONT_SM, Colour::TxtMuted);
            return;
        }

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
        drawWeaponPanel(getActivePlayerInventory(), "INVENTORY");
    }

    void drawBackpackPanel()
    {
        drawWeaponPanel(getActivePlayerBackpack(), "BACKPACK");
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
