#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <pthread.h>
#include "../Characters/Player.h"
#include "../Characters/Enemy.h"
#include "Map.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FONT SIZE CONSTANTS — tweak these to resize text globally
//  FONT_XS  → section headers, muted labels        (e.g. "ACTIVE PLAYER", "HP")
//  FONT_SM  → stat numbers, weapon details          (e.g. "340 / 500", "DMG: 95")
//  FONT_MD  → enemy names, button labels            (e.g. "Enemy 1", "INVENTORY")
//  FONT_LG  → player name in active section         (e.g. "Kira")
//  FONT_XL  → reserved for titles if needed
// ─────────────────────────────────────────────────────────────────────────────
constexpr unsigned FONT_XS = 14;   // was 9–10  → bump this for tiny labels
constexpr unsigned FONT_SM = 16;   // was 11–12 → bump this for stat numbers
constexpr unsigned FONT_MD = 18;   // was 13–14 → bump this for enemy/btn text
constexpr unsigned FONT_LG = 28;   // was 22    → bump this for active player name
constexpr unsigned FONT_XL = 32;   // reserved

// ─────────────────────────────────────────────────────────────────────────────
//  Layout constants
//  SEC_ACTIVE_H → height of the top active-player panel
//                 increase if large fonts cause text to clip
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
constexpr float SEC_ACTIVE_H = 290.f;   // was 260 — extra room for bigger fonts

constexpr float BTN_H  = 30.f;          // was 28 — slightly taller buttons
constexpr float BTN_Y  = SEC_ACTIVE_Y + SEC_ACTIVE_H + 4.f;
constexpr float BTN_W  = (SB_W - PAD * 2 - 4.f) / 3.f;

constexpr float PANEL_Y = BTN_Y + BTN_H + 4.f;
constexpr float PANEL_H = SB_H - PANEL_Y;

// ─────────────────────────────────────────────────────────────────────────────
//  Colours
// ─────────────────────────────────────────────────────────────────────────────
namespace Colour
{
    const sf::Color SidebarBg   = {  18,  18,  28, 255 };
    const sf::Color SectionBg   = {  28,  28,  42, 255 };
    const sf::Color Divider     = {  60,  60,  90, 255 };
    const sf::Color HpBack      = {  45,  45,  45, 255 };
    const sf::Color HpFull      = {  50, 205,  50, 255 };
    const sf::Color HpLow       = { 220,  50,  50, 255 };
    const sf::Color StamBack    = {  30,  30,  60, 255 };
    const sf::Color StamFull    = {  50, 150, 255, 255 };
    const sf::Color StunBadge   = { 255, 200,   0, 255 };
    const sf::Color AliveBadge  = {  50, 205,  50, 255 };
    const sf::Color DeadBadge   = { 150,  30,  30, 255 };
    const sf::Color TxtPrimary  = { 230, 230, 230, 255 };
    const sf::Color TxtMuted    = { 140, 140, 160, 255 };
    const sf::Color TxtName     = { 255, 220, 100, 255 };
    const sf::Color BtnActive   = {  70,  70, 130, 255 };
    const sf::Color BtnInactive = {  35,  35,  60, 255 };
    const sf::Color BtnBorder   = {  90,  90, 150, 255 };
    const sf::Color WeaponCard  = {  30,  30,  50, 255 };
    const sf::Color DmgColour   = { 255, 100, 100, 255 };
    const sf::Color CountBadge  = { 255, 180,  50, 255 };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Renderer
// ─────────────────────────────────────────────────────────────────────────────
class Renderer
{
public:
    enum class SidebarMode { ENEMIES, INVENTORY, BACKPACK };

    Renderer(std::vector<Player*>    players,
             std::vector<Character*> enemies,
             Map*                    map)
        : m_players(std::move(players))
        , m_enemies(std::move(enemies))
        , m_map(map)
        , m_sidebarMode(SidebarMode::ENEMIES)
    {
        pthread_mutex_init(&m_stopMutex, nullptr);
    }

    ~Renderer() { pthread_mutex_destroy(&m_stopMutex); }

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    void setActivePlayerIndex(int idx) { m_activeIdx = idx; }
    void advanceMapScreen()  { if (m_map) m_map->nextScreen();     }
    void retreatMapScreen()  { if (m_map) m_map->previousScreen(); }

    void requestStop()
    {
        pthread_mutex_lock(&m_stopMutex);
        m_stop = true;
        pthread_mutex_unlock(&m_stopMutex);
    }

    void run()
    {
        m_window.create(
            sf::VideoMode((unsigned)WIN_W, (unsigned)WIN_H),
            "Chrono Rift",
            sf::Style::Titlebar | sf::Style::Close
        );
        m_window.setFramerateLimit(60);
        loadAssets();

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
    }

private:
    std::vector<Player*>    m_players;
    std::vector<Character*> m_enemies;
    Map*                    m_map       = nullptr;
    int                     m_activeIdx = 0;
    SidebarMode             m_sidebarMode;

    sf::RenderWindow m_window;
    sf::Font         m_font;
    bool             m_fontLoaded = false;

    std::unordered_map<int, sf::Texture> m_weaponTextures;

    bool            m_stop = false;
    pthread_mutex_t m_stopMutex;
    bool            m_prevMouseDown = false;

    // ─────────────────────────────────────────────────────────────────────────
    void loadAssets()
    {
        m_fontLoaded = m_font.loadFromFile("../DisplayRendering/BlockBlueprint.ttf");
        if (!m_fontLoaded)
            std::cerr << "[Renderer] Font missing — text will not render\n";
        else
            std::cout << "[Renderer] Font loaded OK\n";

        struct WEntry { int typeInt; const char* path; };
        WEntry weaponPaths[] = {
            { (int)WeaponType::SOLAR_CORE,    "../Weapons_sprites/Solar_Core.png"     },
            { (int)WeaponType::LUNAR_BLADE,   "../Weapons_sprites/Lunar_Blade.png"    },
            { (int)WeaponType::IRON_HALBERD,  "../Weapons_sprites/Iron_Halberd.png"   },
            { (int)WeaponType::VENOM_DAGGER,  "../Weapons_sprites/Venom_Dagger.png"   },
            { (int)WeaponType::THUNDERSTAFF,  "../Weapons_sprites/Thunder_staff.png"  },
            { (int)WeaponType::OBSIDIAN_AXE,  "../Weapons_sprites/Obsidian_Axe.png"   },
            { (int)WeaponType::FROSTBOW,      "../Weapons_sprites/Frost_Bow.png"      },
            { (int)WeaponType::SPLINTER_STICK,"../Weapons_sprites/Splinster_Stick.png"},
        };
        for (auto& e : weaponPaths)
        {
            sf::Texture tex;
            if (tex.loadFromFile(e.path))
                m_weaponTextures[e.typeInt] = std::move(tex);
            else
                std::cerr << "[Renderer] Missing sprite: " << e.path << "\n";
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void handleEvents()
    {
        sf::Event e{};
        while (m_window.pollEvent(e))
        {
            if (e.type == sf::Event::Closed)
            {
                requestStop();
                m_window.close();
            }
            if (e.type == sf::Event::KeyPressed)
            {
                if (e.key.code == sf::Keyboard::Right) advanceMapScreen();
                if (e.key.code == sf::Keyboard::Left)  retreatMapScreen();
            }
        }

        bool mouseDown  = sf::Mouse::isButtonPressed(sf::Mouse::Left);
        bool clicked    = mouseDown && !m_prevMouseDown;
        m_prevMouseDown = mouseDown;

        if (clicked)
        {
            sf::Vector2i mp = sf::Mouse::getPosition(m_window);
            float mx = (float)mp.x, my = (float)mp.y;

            float b1x = SB_X + PAD;
            float b2x = b1x + BTN_W + 2.f;
            float b3x = b2x + BTN_W + 2.f;

            if (mx >= b1x && mx < b1x + BTN_W && my >= BTN_Y && my < BTN_Y + BTN_H)
                m_sidebarMode = SidebarMode::ENEMIES;

            if (mx >= b2x && mx < b2x + BTN_W && my >= BTN_Y && my < BTN_Y + BTN_H)
                m_sidebarMode = (m_sidebarMode == SidebarMode::INVENTORY)
                                ? SidebarMode::ENEMIES : SidebarMode::INVENTORY;

            if (mx >= b3x && mx < b3x + BTN_W && my >= BTN_Y && my < BTN_Y + BTN_H)
                m_sidebarMode = (m_sidebarMode == SidebarMode::BACKPACK)
                                ? SidebarMode::ENEMIES : SidebarMode::BACKPACK;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawAll()
    {
        if (m_map) m_map->draw(m_window);
        else       std::cerr << "Map pointer is null.\n";

        for (Player*    p : m_players) if (p) p->draw(m_window);
        for (Character* e : m_enemies) if (e) e->draw(m_window);

        drawSidebarBg();
        drawActiveSection();
        drawToggleButtons();

        switch (m_sidebarMode)
        {
            case SidebarMode::ENEMIES:   drawEnemySection();   break;
            case SidebarMode::INVENTORY: drawInventoryPanel(); break;
            case SidebarMode::BACKPACK:  drawBackpackPanel();  break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawSidebarBg()
    {
        drawRect(SB_X, 0.f, SB_W, SB_H, Colour::SidebarBg);
        drawRect(SB_X, 0.f, 2.f,  SB_H, Colour::Divider);
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawActiveSection()
    {
        drawRect(SB_X, SEC_ACTIVE_Y, SB_W, SEC_ACTIVE_H, Colour::SectionBg);
        drawDivider(SEC_ACTIVE_Y + SEC_ACTIVE_H);

        if (m_players.empty() || m_activeIdx >= (int)m_players.size()) return;
        Player* p = m_players[m_activeIdx];
        if (!p) return;

        float x = SB_X + PAD;
        float y = SEC_ACTIVE_Y + PAD;

        // "ACTIVE PLAYER" → FONT_XS  (tiny section label)
        drawText("ACTIVE PLAYER", x, y, FONT_XS, Colour::TxtMuted);
        y += 18.f;

        // Player name → FONT_LG  (most prominent text in panel)
        drawText(p->getName(), x, y, FONT_LG, Colour::TxtName);
        y += 36.f;

        drawStatusBadge(x, y, p->isAlive(), p->isStunned());
        y += 28.f;

        // ── HP ────────────────────────────────────────────────────────────────
        // "HP" label → FONT_XS   |   "340 / 500" number → FONT_SM
        drawText("HP", x, y, FONT_XS, Colour::TxtMuted);
        drawText(
            std::to_string(p->getHp()) + " / " + std::to_string(p->getMaxHp()),
            SB_X + SB_W - PAD - 72.f, y, FONT_SM, Colour::TxtMuted
        );
        y += 18.f;

        sf::Color hpCol = p->isStunned()
                        ? Colour::StunBadge
                        : ((float)p->getHp() / p->getMaxHp() < 0.3f
                           ? Colour::HpLow : Colour::HpFull);

        drawBar(x, y, SB_W - PAD * 2, 20.f,        // bar height was 18 → 20
                p->getHp(), p->getMaxHp(),
                Colour::HpBack, hpCol);
        y += 30.f;

        // ── Stamina ───────────────────────────────────────────────────────────
        // "Stamina" label → FONT_XS   |   numbers → FONT_SM
        drawText("Stamina", x, y, FONT_XS, Colour::TxtMuted);
        drawText(
            std::to_string(p->getStamina()) + " / " + std::to_string(p->getMaxStamina()),
            SB_X + SB_W - PAD - 72.f, y, FONT_SM, Colour::TxtMuted
        );
        y += 18.f;

        drawBar(x, y, SB_W - PAD * 2, 14.f,        // stamina bar slightly thinner
                p->getStamina(), p->getMaxStamina(),
                Colour::StamBack, Colour::StamFull);
        y += 26.f;

        // Stun notice → FONT_SM
        if (p->isStunned())
            drawText(
                "STUNNED  —  clears T" + std::to_string(p->getStunEndTem()),
                x, y, FONT_SM, Colour::StunBadge
            );
    }

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

            // Button labels → FONT_XS
            // increase FONT_XS if buttons feel cramped
            drawText(btns[i].label,
                     bx + 6.f, BTN_Y + (BTN_H - FONT_XS) / 2.f - 2.f,
                     FONT_XS, active ? Colour::TxtName : Colour::TxtMuted);
            bx += BTN_W + 2.f;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawEnemySection()
    {
        drawRect(SB_X, PANEL_Y, SB_W, PANEL_H, Colour::SidebarBg);

        // "ENEMIES" header → FONT_XS
        drawText("ENEMIES", SB_X + PAD, PANEL_Y + 6.f, FONT_XS, Colour::TxtMuted);

        constexpr float CARD_H = 88.f;     // was 80 — more room per enemy card
        constexpr float GAP    = 6.f;
        float cardY = PANEL_Y + 24.f;

        for (int i = 0; i < (int)m_enemies.size(); ++i)
        {
            if (cardY + CARD_H > PANEL_Y + PANEL_H) break;
            Character* e = m_enemies[i];
            if (!e) continue;

            drawRect(SB_X + PAD, cardY, SB_W - PAD * 2, CARD_H,
                     Colour::SectionBg, Colour::Divider, 1.f);

            float cx = SB_X + PAD + 8.f;
            float cy = cardY + 8.f;

            // Enemy name → FONT_MD
            drawText("Enemy " + std::to_string(i + 1), cx, cy, FONT_MD, Colour::TxtPrimary);
            drawStatusBadge(SB_X + SB_W - PAD - 70.f, cy + 1.f,
                            e->isAlive(), e->isStunned());
            cy += 26.f;

            // HP label + value → FONT_SM
            drawText("HP", cx, cy, FONT_SM, Colour::TxtMuted);
            drawText(
                std::to_string(e->getHp()) + " / " + std::to_string(e->getMaxHp()),
                cx + 30.f, cy, FONT_SM, Colour::TxtMuted
            );
            cy += 18.f;

            sf::Color eHpCol = e->isStunned()
                             ? Colour::StunBadge
                             : ((float)e->getHp() / e->getMaxHp() < 0.3f
                                ? Colour::HpLow : Colour::HpFull);

            drawBar(cx, cy, SB_W - PAD * 2 - 16.f, 12.f,
                    e->getHp(), e->getMaxHp(),
                    Colour::HpBack, eHpCol);

            cardY += CARD_H + GAP;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawWeaponPanel(const std::vector<Weapon>& weapons, const char* header)
    {
        drawRect(SB_X, PANEL_Y, SB_W, PANEL_H, Colour::SidebarBg);

        // Panel header → FONT_XS
        drawText(header, SB_X + PAD, PANEL_Y + 6.f, FONT_XS, Colour::TxtMuted);

        if (weapons.empty())
        {
            drawText("Empty", SB_X + PAD, PANEL_Y + 28.f, FONT_SM, Colour::TxtMuted);
            return;
        }

        std::unordered_map<std::string, int> countByName;
        std::vector<const Weapon*> unique;

        for (const Weapon& w : weapons)
        {
            if (countByName.find(w.getName()) == countByName.end())
                unique.push_back(&w);
            countByName[w.getName()]++;
        }

        constexpr float CARD_H  = 66.f;    // was 60 — room for bigger text
        constexpr float ICON_SZ = 50.f;    // was 48 — slightly larger icon
        constexpr float GAP     = 6.f;
        float cardY = PANEL_Y + 24.f;

        for (const Weapon* w : unique)
        {
            if (cardY + CARD_H > PANEL_Y + PANEL_H) break;

            float cx = SB_X + PAD;
            drawRect(cx, cardY, SB_W - PAD * 2, CARD_H,
                     Colour::WeaponCard, Colour::Divider, 1.f);

            // ── Icon ──────────────────────────────────────────────────────────
            int typeKey = (int)w->getType();
            if (m_weaponTextures.count(typeKey))
            {
                sf::Sprite icon(m_weaponTextures.at(typeKey));
                icon.setPosition(cx + 6.f, cardY + 8.f);
                icon.setScale(ICON_SZ / icon.getTexture()->getSize().x,
                              ICON_SZ / icon.getTexture()->getSize().y);
                m_window.draw(icon);
            }
            else
            {
                drawRect(cx + 6.f, cardY + 8.f, ICON_SZ, ICON_SZ,
                         {60, 60, 100}, Colour::BtnBorder, 1.f);
                // Fallback initial letter → FONT_LG
                drawText(std::string(1, w->getName()[0]),
                         cx + 22.f, cardY + 18.f, FONT_LG, Colour::TxtName);
            }

            // ── Text ──────────────────────────────────────────────────────────
            float tx = cx + ICON_SZ + 16.f;
            float ty = cardY + 8.f;

            // Weapon name → FONT_SM
            drawText(w->getName(), tx, ty, FONT_SM, Colour::TxtPrimary);
            ty += 18.f;

            // DMG value → FONT_SM  (DmgColour = red-ish)
            drawText("DMG: "   + std::to_string(w->getDamage()),
                     tx, ty, FONT_SM, Colour::DmgColour);
            ty += 16.f;

            // Slot count → FONT_XS  (less important detail)
            drawText("Slots: " + std::to_string(w->getSlotSize()),
                     tx, ty, FONT_XS, Colour::TxtMuted);

            // ── Count badge (x2, x3 etc.) ─────────────────────────────────────
            int cnt = countByName[w->getName()];
            if (cnt > 1)
            {
                float badgeX = SB_X + SB_W - PAD - 32.f;
                float badgeY = cardY + 8.f;
                drawRect(badgeX, badgeY, 30.f, 20.f, Colour::CountBadge);
                // Badge text → FONT_XS
                drawText("x" + std::to_string(cnt),
                         badgeX + 5.f, badgeY + 3.f, FONT_XS, sf::Color::Black);
            }

            cardY += CARD_H + GAP;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawInventoryPanel()
    {
        if (m_players.empty() || m_activeIdx >= (int)m_players.size()) return;
        Player* p = m_players[m_activeIdx];
        if (!p) return;

        std::vector<Weapon> weapons;
        for (const auto& pair : p->getInventory().getEquippedWeapons())
            weapons.push_back(pair.second);

        drawWeaponPanel(weapons, "INVENTORY");
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawBackpackPanel()
    {
        if (m_players.empty() || m_activeIdx >= (int)m_players.size()) return;
        Player* p = m_players[m_activeIdx];
        if (!p) return;

        drawWeaponPanel(p->getBackpack().getWeapons(), "BACKPACK");
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawBar(float x, float y, float w, float h,
                 int current, int max,
                 sf::Color bgCol, sf::Color fillCol)
    {
        float pct = (max > 0)
                    ? std::clamp((float)current / max, 0.f, 1.f) : 0.f;

        drawRect(x, y, w, h, bgCol);
        if (pct > 0.f)
            drawRect(x, y, w * pct, h, fillCol);
        drawRect(x, y, w, h, sf::Color::Transparent, Colour::Divider, 1.f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    void drawStatusBadge(float x, float y, bool alive, bool stunned)
    {
        std::string lbl;
        sf::Color   col;
        if      (!alive)  { lbl = "DEAD";    col = Colour::DeadBadge;  }
        else if (stunned) { lbl = "STUNNED"; col = Colour::StunBadge;  }
        else              { lbl = "ALIVE";   col = Colour::AliveBadge; }
        drawRect(x, y, 66.f, 18.f, col);                // slightly wider badge
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
//  Testing helper — call from main() to populate a player with dummy weapons
//
//  Usage:
//      Player* p = new Player(...);
//      seedTestWeapons(p);
//      Renderer r({p}, {e1, e2}, nullptr);
//      r.run();
// ─────────────────────────────────────────────────────────────────────────────
 inline void seedTestWeapons(Player* p)
{
    struct WDef { WeaponType t; const char* name; int slots; int dmg; };
    WDef defs[] = {
        { WeaponType::SOLAR_CORE,    "Solar Core",     10, 95 },
        { WeaponType::LUNAR_BLADE,   "Lunar Blade",    10, 90 },
        { WeaponType::IRON_HALBERD,  "Iron Halberd",    7, 55 },
        { WeaponType::VENOM_DAGGER,  "Venom Dagger",    4, 30 },
        { WeaponType::SPLINTER_STICK,"Splinter Stick",  2, 12 },
        { WeaponType::SPLINTER_STICK,"Splinter Stick",  2, 12 }, // triggers x2 badge
    };
    int id = 1;
    for (auto& d : defs)
    {
        Weapon w(id++, d.t, d.name, d.slots, d.dmg);
        p->pickupWeapon(w);
    }
}
