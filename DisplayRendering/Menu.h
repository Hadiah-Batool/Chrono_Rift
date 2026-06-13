#pragma once
#include <SFML/Graphics.hpp>
#include "../Characters/Player.h"
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <array>

using std::vector;
using std::string;

constexpr float MENU_WIN_W = 1280.f;
constexpr float MENU_WIN_H = 800.f;

enum class MenuOption { StartGame, LevelSelect, Options, Exit, None };
enum class MenuScreen { MAIN, LEVEL_SELECT, PARTY_SELECT, DONE };

struct LevelDescription
{
    int    levelNumber = 1;
    string title       = "Unknown";
    string mapPath     = "";
    string introText   = "";
    int    enemyCount  = 0;
    int    difficulty  = 1;
};

struct PartyConfig
{
    std::array<PlayerType, 4> players = {
        PlayerType::NONE, PlayerType::NONE,
        PlayerType::NONE, PlayerType::NONE
    };
    int  selectedLevel = 1;

    bool valid() const
    {
        for (auto& p : players)
            if (p != PlayerType::NONE) return true;
        return false;
    }

    int numPlayers() const
    {
        int n = 0;
        for (auto& p : players)
            if (p != PlayerType::NONE) n++;
        return n;
    }
};


namespace MenuColour
{
    const sf::Color Overlay        = {   0,   0,   0, 170 };
    const sf::Color BtnNormal      = {  20,  20,  40, 220 };
    const sf::Color BtnHover       = {  60,  60, 130, 240 };
    const sf::Color BtnBorder      = {  90,  90, 150, 255 };
    const sf::Color BtnBorderHover = { 180, 180, 255, 255 };
    const sf::Color TxtNormal      = { 255, 255, 255, 255 };
    const sf::Color TxtHover       = { 255, 220, 100, 255 };
    const sf::Color TxtMuted       = { 120, 120, 140, 255 };
    const sf::Color Gold           = { 255, 210,  60, 255 };
    const sf::Color Subtitle       = { 160, 160, 200, 255 };
    const sf::Color CardNormal     = {  25,  25,  45, 220 };
    const sf::Color CardHover      = {  50,  50, 100, 240 };
    const sf::Color CardPicked     = {  30,  80,  30, 240 };
    const sf::Color CardBorder     = {  70,  70, 120, 255 };
    const sf::Color CardBorderPick = {  80, 200,  80, 255 };
    const sf::Color LevelHover     = { 255, 220,  60,  60 };
    const sf::Color LevelSelected  = {  60, 200,  60,  80 };
    const sf::Color LevelBorder    = { 255, 220,  60, 200 };
    const sf::Color BLU ={8, 69, 210, 140};
}

class GameMenu
{
public:
    explicit GameMenu(sf::RenderWindow& window,
                      const string& mainBgPath   = "",
                      const string& levelMapPath = "")
        : m_window(window)
        , m_mainBgPath(mainBgPath)
        , m_levelMapPath(levelMapPath)
    {}

PartyConfig run()
{
    loadAssets();
    loadCharacterAssets();
    m_screen    = MenuScreen::MAIN;
    m_prevClick = sf::Mouse::isButtonPressed(sf::Mouse::Left);

    while (m_window.isOpen() && m_screen != MenuScreen::DONE)
    {
        sf::Event ev{};
        while (m_window.pollEvent(ev))
            if (ev.type == sf::Event::Closed)
                { m_window.close(); return {}; }

        sf::Vector2i mouse   = sf::Mouse::getPosition(m_window);
        bool         clicked = sf::Mouse::isButtonPressed(sf::Mouse::Left)
                               && !m_prevClick;
        m_prevClick = sf::Mouse::isButtonPressed(sf::Mouse::Left);

        m_window.clear(sf::Color(10, 10, 20));

        switch (m_screen)
        {
            case MenuScreen::MAIN:         tickMain(mouse, clicked);        break;
            case MenuScreen::LEVEL_SELECT: tickLevelSelect(mouse, clicked); break;
            case MenuScreen::PARTY_SELECT: tickPartySelect(mouse, clicked); break;
            default: break;
        }

        m_window.display();
    }


    return m_result;
}


// Public getter
int getSelectedLevel() const { return m_selectedLevel; }


// ─────────────────────────────────────────────────────────────────────────────
private:
// ─────────────────────────────────────────────────────────────────────────────


    sf::RenderWindow& m_window;
    string            m_mainBgPath;
    string            m_levelMapPath;

    sf::Font m_font;
    bool     m_fontLoaded = false;

    sf::Texture m_mainBgTex;
    sf::Sprite  m_mainBgSprite;
    bool        m_hasMainBg = false;

    sf::Texture m_levelMapTex;
    sf::Sprite  m_levelMapSprite;
    bool        m_hasLevelMap = false;

    MenuScreen  m_screen      = MenuScreen::MAIN;
    bool        m_prevClick   = false;
    int         m_selectedLevel = 1;
    PartyConfig m_result;
   
   

    // ── Card layout constants — one place, used by both tick and draw ─────────
    static constexpr float CARD_W   = 250.f;
    static constexpr float CARD_H   = 360.f;
    static constexpr float CARD_GAP = 22.f;
    static constexpr float CARD_X0  = (MENU_WIN_W - (4.f * CARD_W + 3.f * CARD_GAP)) / 2.f;
    static constexpr float CARD_Y   = 120.f;

    // ── Image box inside each card ────────────────────────────────────────────
    static constexpr float IMG_W = 160.f;
    static constexpr float IMG_H = 160.f;



    // ── Character card data ───────────────────────────────────────────────────
    struct CharCard
    {
        PlayerType type;
        string     name;
        string     role;
        string     flavour;
        sf::Color  accent;
    };
    CharCard m_cards[4] = {
        { PlayerType::CHRONO, "CHRONO", "Warrior", "Balanced fighter, strong strikes.", { 100, 180, 255 } },
        { PlayerType::FROG,   "FROG",   "Knight",  "High defence, heavy blows.",         {  80, 220, 120 } },
        { PlayerType::MARLE,  "MARLE",  "Healer",  "Support magic, keeps party alive.",  { 255, 160, 200 } },
        { PlayerType::MAGUS,  "MAGUS",  "Mage",    "Devastating magic, low HP.",          { 180,  80, 255 } },
    };
// ─────────────────────────────────────────────────────────────────────────
//  drawSpriteClipped — viewport scissor so sprite can't bleed outside box
// ─────────────────────────────────────────────────────────────────────────
void drawSpriteClipped(sf::Sprite& spr,
                       float clipX, float clipY,
                       float clipW, float clipH)
{
    sf::View oldView = m_window.getView();

    sf::View clipView(sf::FloatRect(clipX, clipY, clipW, clipH));
    clipView.setViewport(sf::FloatRect(
        clipX / MENU_WIN_W,
        clipY / MENU_WIN_H,
        clipW / MENU_WIN_W,
        clipH / MENU_WIN_H
    ));

    m_window.setView(clipView);
    m_window.draw(spr);
    m_window.setView(oldView);
}



    // ── Character sprite assets ───────────────────────────────────────────────
    struct CharAsset
    {
        sf::Texture texture;
        sf::Sprite  sprite;
        bool        loaded    = false;
        sf::IntRect frameRect = { 0, 0, 0, 0 };
    };
    CharAsset m_charAssets[4];

    // ─────────────────────────────────────────────────────────────────────────
    //  loadAssets
    // ─────────────────────────────────────────────────────────────────────────
    void loadAssets()
    {
        m_fontLoaded = m_font.loadFromFile("../DisplayRendering/BlockBlueprint.ttf");
        if (!m_fontLoaded)
            m_fontLoaded = m_font.loadFromFile("BlockBlueprint.ttf");

        if (!m_mainBgPath.empty() && m_mainBgTex.loadFromFile(m_mainBgPath))
        {
            m_mainBgSprite.setTexture(m_mainBgTex);
            auto sz = m_mainBgTex.getSize();
            m_mainBgSprite.setScale(MENU_WIN_W / sz.x, MENU_WIN_H / sz.y);
            m_hasMainBg = true;
        }

        if (!m_levelMapPath.empty() && m_levelMapTex.loadFromFile(m_levelMapPath))
        {
            m_levelMapSprite.setTexture(m_levelMapTex);
            auto sz = m_levelMapTex.getSize();
            m_levelMapSprite.setScale(MENU_WIN_W / sz.x, MENU_WIN_H / sz.y);
            m_hasLevelMap = true;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  loadCharacterAssets
    // ─────────────────────────────────────────────────────────────────────────
void loadCharacterAssets()
{
    struct CharLoad { const char* path; sf::IntRect frame; };

CharLoad defs[4] = {
    { "../Players/Chrono_sprite_frame1.png", { 0, 0, 0, 0 } },  // 0,0 = use full texture
    { "../Players/Frog_sprite_frame1.png",   { 0, 0, 0, 0 } },
    { "../Players/Marle_idle.png",           { 0, 0, 0, 0 } },
    { "../Players/Magus_sprite_frame1.png",  { 0, 0, 0, 0 } },
};

    for (int i = 0; i < 4; i++)
    {
        if (!m_charAssets[i].texture.loadFromFile(defs[i].path))
        {
            std::cerr << "[GameMenu] Missing sprite: " << defs[i].path << "\n";
            m_charAssets[i].loaded = false;
            continue;
        }

        m_charAssets[i].loaded    = true;
        m_charAssets[i].frameRect = defs[i].frame;
        m_charAssets[i].sprite.setTexture(m_charAssets[i].texture);

        // Crop to frame if one was specified
        sf::IntRect fr = defs[i].frame;
        if (fr.width > 0 && fr.height > 0)
            m_charAssets[i].sprite.setTextureRect(fr);
    }
}

    // ─────────────────────────────────────────────────────────────────────────
    //  SCREEN 1 — Main Menu
    // ─────────────────────────────────────────────────────────────────────────
void tickMain(sf::Vector2i mouse, bool clicked)
{
    if (m_hasMainBg) m_window.draw(m_mainBgSprite);
    else             drawRect(0, 0, MENU_WIN_W, MENU_WIN_H, sf::Color(10, 10, 20));

    drawRect(0.f, 285.f,
             MENU_WIN_W / 2.f, MENU_WIN_H - 100.f,
             sf::Color(0, 0, 0, 0));

    constexpr float LEFT_X  = 0.f;
    constexpr float LEFT_W  = MENU_WIN_W / 2.f;
    constexpr float TITLE_Y = 200.f;

    // ── Buttons — NO isExit flag, just screens ────────────────────────────────
    struct Btn { const char* label; MenuScreen next; };
    Btn btns[] = {
        { "NEW GAME", MenuScreen::LEVEL_SELECT },
        { "OPTIONS",  MenuScreen::MAIN         },
        { "EXIT",     MenuScreen::DONE         },   // DONE exits the run() loop cleanly
    };

    constexpr float BTN_W   = 300.f;
    constexpr float BTN_H   = 60.f;
    constexpr float BTN_X   = LEFT_X + (LEFT_W - BTN_W) / 2.f;
    constexpr float BTN_Y0  = TITLE_Y + 140.f;
    constexpr float BTN_GAP = 18.f;

    for (int i = 0; i < 3; i++)
    {
        float by  = BTN_Y0 + i * (BTN_H + BTN_GAP);
        bool  over = hovered(mouse, BTN_X, by, BTN_W, BTN_H);

        sf::Color lineCol = over ? MenuColour::BLU : sf::Color(0, 0, 0, 180);
        sf::Color txtCol  = over ? MenuColour::BLU : sf::Color(0, 0, 0, 180);
        float     lineThk = over ? 2.f : 1.f;

        drawRect(BTN_X, by + BTN_H - lineThk, BTN_W, lineThk, lineCol);

        if (over)
            drawRect(BTN_X - 10.f, by + BTN_H / 2.f - 8.f, 3.f, 16.f, MenuColour::BLU);

        drawText(btns[i].label,
                 BTN_X, by + (BTN_H - 36.f) / 2.f,
                 36, txtCol, true, BTN_W);

        // ── Single clean action — no window.close() here ──────────────────────
        if (over && clicked)
            m_screen = btns[i].next;
    }

    drawText("v0.1", 12.f, MENU_WIN_H - 22.f, 12, MenuColour::TxtMuted);
}



    // ─────────────────────────────────────────────────────────────────────────
    //  SCREEN 2 — Level Select
    // ─────────────────────────────────────────────────────────────────────────
void tickLevelSelect(sf::Vector2i mouse, bool clicked)
{
    if (m_hasLevelMap) m_window.draw(m_levelMapSprite);
    else               drawRect(0, 0, MENU_WIN_W, MENU_WIN_H, sf::Color(10, 30, 20));

    // ── Header ────────────────────────────────────────────────────────────────
    drawRect(0, 0, MENU_WIN_W, 52.f, sf::Color(0, 0, 0, 160));
    drawText("SELECT A LEVEL", 0.f, 10.f, 28, MenuColour::Gold, true, MENU_WIN_W);

    // ── Back button ───────────────────────────────────────────────────────────
    bool backHover = hovered(mouse, 12.f, 10.f, 90.f, 32.f);
    drawRect(12.f, 10.f, 90.f, 32.f,
             backHover ? MenuColour::BtnHover : MenuColour::BtnNormal,
             MenuColour::BtnBorder, 1.f);
    drawText("< BACK", 16.f, 14.f, 14,
             backHover ? MenuColour::TxtHover : MenuColour::TxtNormal);
    if (backHover && clicked) m_screen = MenuScreen::MAIN;

    // ── Level entries — each at its own fixed position ────────────────────────
    struct LevelEntry
    {
        int         level;
        const char* title;
        const char* subtitle;
        float       x, y;        // ← individual positions
    };
    LevelEntry levels[] = {
        { 1, "GUARDIA FOREST",    "A quiet wood hiding ancient danger.", 230.f, 500.f },
        { 2, "MANORIA CATHEDRAL", "Dark halls, darker intentions.",      450.f, 250.f },
        { 3, "HECKRAN CAVE",      "Depths of magic and madness.",        900.f, 550.f },
    };

    constexpr float ENTRY_W = 220.f;   // hit area width — adjust if titles clip
    constexpr float ENTRY_H = 64.f;   // hit area height

    for (int i = 0; i < 3; i++)
    {
        float ex         = levels[i].x;
        float ey         = levels[i].y;
        bool  isSelected = (m_selectedLevel == levels[i].level);
        bool  over       = hovered(mouse, ex - 18.f, ey, ENTRY_W + 36.f, ENTRY_H);

        // ── Hover glow wash ───────────────────────────────────────────────
        if (over && !isSelected)
            drawRect(ex - 18.f, ey, ENTRY_W + 36.f, ENTRY_H,
                     sf::Color(255, 210, 60, 18));

        // ── Left accent bar when selected ─────────────────────────────────
        if (isSelected)
            drawRect(ex - 14.f, ey + 6.f, 4.f, ENTRY_H - 12.f,
                     MenuColour::Gold);

        // ── Title ─────────────────────────────────────────────────────────
        sf::Color titleCol = isSelected ? MenuColour::Gold
                           : over       ? sf::Color(255, 235, 140, 255)
                                        : MenuColour::TxtNormal;
        unsigned  titleSz  = (isSelected || over) ? 26 : 22;

        drawText(levels[i].title, ex, ey + 4.f, titleSz, titleCol);

        // ── Underline ─────────────────────────────────────────────────────
        float lineW = isSelected ? ENTRY_W
                    : over       ? ENTRY_W * 0.45f
                                 : 0.f;
        if (lineW > 0.f)
            drawRect(ex, ey + 4.f + titleSz + 4.f,
                     lineW, isSelected ? 2.f : 1.f,
                     isSelected ? MenuColour::Gold
                                : sf::Color(255, 235, 140, 160));

        // ── Subtitle ──────────────────────────────────────────────────────
        drawText(levels[i].subtitle, ex, ey + 36.f, 12,
                 isSelected ? sf::Color(200, 180, 100, 200)
                            : MenuColour::TxtNormal);

        // ── Difficulty dots ───────────────────────────────────────────────
        drawDifficultyDots(ex, ey + 50.f, levels[i].level);

        // ── Click ─────────────────────────────────────────────────────────
        if (over && clicked)
        {
            m_selectedLevel = levels[i].level;
            m_screen        = MenuScreen::PARTY_SELECT;
        }
    }

    // ── Footer ────────────────────────────────────────────────────────────────
    drawRect(0, MENU_WIN_H - 36.f, MENU_WIN_W, 36.f, sf::Color(0, 0, 0, 160));
    drawText("Click a level to continue",
             0.f, MENU_WIN_H - 26.f, 14, MenuColour::TxtMuted, true, MENU_WIN_W);
}


    void drawDifficultyDots(float x, float y, int level)
    {
        for (int d = 0; d < 3; d++)
        {
            sf::CircleShape dot(6.f);
            dot.setFillColor(d < level ? sf::Color(255, 180, 50, 255)
                                       : sf::Color(80,  80,  80, 180));
            dot.setPosition(x + d * 18.f, y);
            m_window.draw(dot);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  SCREEN 3 — Party Select
    //  tickPartySelect ONLY loops to call drawCharacterCard — nothing else
    // ─────────────────────────────────────────────────────────────────────────
    void tickPartySelect(sf::Vector2i mouse, bool clicked)
    {
        // Background
        drawRect(0, 0, MENU_WIN_W, MENU_WIN_H, sf::Color(10, 10, 20));

        // ── Header ────────────────────────────────────────────────────────────
        drawText("CHOOSE YOUR PARTY",
                 0.f, 20.f, 38, MenuColour::Gold, true, MENU_WIN_W);

        string sub = "Level " + std::to_string(m_selectedLevel)
                   + "   |   Select 1 to 4 heroes   |   click to toggle";
        drawText(sub, 0.f, 70.f, 15, MenuColour::Subtitle, true, MENU_WIN_W);

        drawText("Selected: " + std::to_string(m_result.players.size()) + " / 4",
                 0.f, 94.f, 14, MenuColour::TxtMuted, true, MENU_WIN_W);

        // ── Back button ───────────────────────────────────────────────────────
        bool backHover = hovered(mouse, 12.f, 14.f, 90.f, 32.f);
        drawRect(12.f, 14.f, 90.f, 32.f,
                 backHover ? MenuColour::BtnHover : MenuColour::BtnNormal,
                 MenuColour::BtnBorder, 1.f);
        drawText("< BACK", 16.f, 20.f, 14,
                 backHover ? MenuColour::TxtHover : MenuColour::TxtNormal);
        if (backHover && clicked)
        {
            m_result.players.fill(PlayerType::NONE);  // reset to default (not really needed)
            m_screen = MenuScreen::LEVEL_SELECT;
        }

        // ── Cards — drawCharacterCard owns ALL card logic ─────────────────────
        for (int i = 0; i < 4; i++)
        {
            float cx = CARD_X0 + i * (CARD_W + CARD_GAP);
            drawCharacterCard(i, cx, CARD_Y, mouse, clicked);
        }

        // ── Stat bars below cards ─────────────────────────────────────────────
        drawStatsTable(CARD_X0, CARD_Y + CARD_H + 14.f, CARD_W, CARD_GAP);

        // ── Confirm button ────────────────────────────────────────────────────
        constexpr float CONF_W = 280.f;
        constexpr float CONF_H = 48.f;
        constexpr float CONF_X = (MENU_WIN_W - CONF_W) / 2.f;
        constexpr float CONF_Y = MENU_WIN_H - 62.f;

        bool hasPlayers = !m_result.players.empty();
        bool confHover  = hasPlayers && hovered(mouse, CONF_X, CONF_Y, CONF_W, CONF_H);

        drawRect(CONF_X, CONF_Y, CONF_W, CONF_H,
                 !hasPlayers ? sf::Color(30, 30, 30, 180)
                 : confHover  ? MenuColour::BtnHover
                              : MenuColour::BtnNormal,
                 confHover ? MenuColour::BtnBorderHover : MenuColour::BtnBorder, 1.5f);

        drawText(!hasPlayers ? "SELECT A HERO FIRST" : "CONFIRM PARTY",
                 CONF_X, CONF_Y + 12.f, 20,
                 !hasPlayers ? MenuColour::TxtMuted
                 : confHover  ? MenuColour::TxtHover
                              : MenuColour::TxtNormal,
                 true, CONF_W);

        if (confHover && clicked)
        {
            m_result.selectedLevel = m_selectedLevel;
            m_screen               = MenuScreen::DONE;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  drawCharacterCard — single source of truth for one card
    //  Draws background, sprite, text, badges AND handles click toggle
    // ─────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────
//  drawCharacterCard — owns ALL drawing + click logic for one card
// ─────────────────────────────────────────────────────────────────────────
void drawCharacterCard(int i, float cx, float cy,
                       sf::Vector2i mouse, bool clicked)
{
    PlayerType t    = m_cards[i].type;
    bool isPicked   = std::find(m_result.players.begin(),
                                m_result.players.end(), t)
                      != m_result.players.end();
    bool over       = hovered(mouse, cx, cy, CARD_W, CARD_H);

    // ── Card background ───────────────────────────────────────────────────
    sf::Color fill   = isPicked ? MenuColour::CardPicked
                     : over     ? MenuColour::CardHover
                                : MenuColour::CardNormal;
    sf::Color border = isPicked ? MenuColour::CardBorderPick
                     : over     ? MenuColour::BtnBorderHover
                                : MenuColour::CardBorder;
    drawRect(cx, cy, CARD_W, CARD_H, fill, border, 2.f);

    // ── Left accent stripe ────────────────────────────────────────────────
    drawRect(cx, cy, 5.f, CARD_H, m_cards[i].accent);

    // ── Sprite image box ──────────────────────────────────────────────────
    float imgX = cx + (CARD_W - IMG_W) / 2.f;
    float imgY = cy + 14.f;

if (m_charAssets[i].loaded)
{
    sf::Sprite& spr    = m_charAssets[i].sprite;
    sf::FloatRect bnds = spr.getLocalBounds();

    // 1. Fit sprite to IMG_H, preserve aspect ratio
    float scale = IMG_H / bnds.height;
    float scaledW = bnds.width * scale;
    float scaledH = bnds.height * scale;   // == IMG_H

    // 2. Center horizontally in the image box
    float drawX = imgX + (IMG_W - scaledW) / 2.f;

    // 3. Anchor feet to a shared ground line (bottom of image box)
    float groundY = imgY + IMG_H;
    float drawY   = groundY - scaledH;

    spr.setScale(scale, scale);
    spr.setPosition(drawX, drawY);

    spr.setColor(isPicked ? sf::Color(180, 255, 180)
                 : over   ? sf::Color(220, 220, 255)
                          : sf::Color::White);

    drawSpriteClipped(spr, imgX, imgY, IMG_W, IMG_H);
}
    else
    {
        // Placeholder box with accent tint + big initial letter
        drawRect(imgX, imgY, IMG_W, IMG_H,
                 sf::Color(m_cards[i].accent.r,
                           m_cards[i].accent.g,
                           m_cards[i].accent.b, 35),
                 m_cards[i].accent, 1.f);
        drawText(string(1, m_cards[i].name[0]),
                 imgX, imgY + IMG_H / 2.f - 28.f,
                 52, m_cards[i].accent, true, IMG_W);
        drawText("no sprite",
                 imgX, imgY + IMG_H - 18.f,
                 10, MenuColour::TxtMuted, true, IMG_W);
    }

    // ── Divider ───────────────────────────────────────────────────────────
    float ty = imgY + IMG_H + 8.f;
    drawRect(cx + 10.f, ty, CARD_W - 20.f, 1.f, m_cards[i].accent);
    ty += 10.f;

    // ── Name ──────────────────────────────────────────────────────────────
    drawText(m_cards[i].name, cx + 10.f, ty, 22,
             isPicked ? MenuColour::CardBorderPick
             : over   ? MenuColour::TxtHover
                      : MenuColour::TxtNormal);
    ty += 30.f;

    // ── Role pill ─────────────────────────────────────────────────────────
    drawRect(cx + 10.f, ty, 94.f, 20.f,
             sf::Color(m_cards[i].accent.r,
                       m_cards[i].accent.g,
                       m_cards[i].accent.b, 50),
             m_cards[i].accent, 1.f);
    drawText(m_cards[i].role, cx + 14.f, ty + 3.f, 12, m_cards[i].accent);
    ty += 28.f;

    // ── Flavour text ──────────────────────────────────────────────────────
    drawText(m_cards[i].flavour, cx + 10.f, ty, 11, MenuColour::TxtMuted);

    // ── P-order badge top-right ───────────────────────────────────────────
    if (isPicked)
    {
        int order = 1;
        for (auto& s : m_result.players) { if (s == t) break; order++; }
        drawRect(cx + CARD_W - 36.f, cy + 10.f, 28.f, 22.f,
                 MenuColour::CardBorderPick);
        drawText("P" + std::to_string(order),
                 cx + CARD_W - 31.f, cy + 12.f, 12, sf::Color::Black);
    }

    // ── Hover hint strip ──────────────────────────────────────────────────
    if (over)
    {
        drawRect(cx, cy + CARD_H - 22.f, CARD_W, 22.f, sf::Color(0, 0, 0, 130));
        drawText(isPicked ? "click to remove" : "click to select",
                 cx, cy + CARD_H - 18.f, 11,
                 isPicked ? sf::Color(255, 100, 100, 220) : MenuColour::TxtMuted,
                 true, CARD_W);
    }

    // ── Toggle on click ───────────────────────────────────────────────────
// ── Toggle on click ───────────────────────────────────────────────────
if (over && clicked)
{
    auto it = std::find(m_result.players.begin(),
                        m_result.players.end(), t);
    if (it != m_result.players.end())
    {
        // Already selected — deselect
        *it = PlayerType::NONE;
    }
    else
    {
        // Find first empty slot
        auto empty = std::find(m_result.players.begin(),
                               m_result.players.end(), PlayerType::NONE);
        if (empty != m_result.players.end())
            *empty = t;

    }
}

}

    // ─────────────────────────────────────────────────────────────────────────
    //  drawStatsTable — HP / DMG / DEF bars aligned under each card
    // ─────────────────────────────────────────────────────────────────────────
    void drawStatsTable(float x0, float y, float cardW, float gap)
    {
        struct StatRow { const char* label; int vals[4]; sf::Color col; };
        StatRow rows[] = {
            { "HP ",  { 85, 95, 70, 60 }, sf::Color( 50, 205,  50) },
            { "DMG",  { 70, 80, 55, 95 }, sf::Color(220,  80,  80) },
            { "DEF",  { 65, 90, 60, 40 }, sf::Color( 80, 160, 255) },
        };

        for (int r = 0; r < 3; r++)
        {
            float ry = y + r * 24.f;
            drawText(rows[r].label, x0 - 42.f, ry + 3.f, 11, MenuColour::TxtMuted);
            for (int i = 0; i < 4; i++)
            {
                float bx  = x0 + i * (cardW + gap);
                float pct = rows[r].vals[i] / 100.f;
                drawRect(bx + 5.f, ry, cardW - 10.f, 16.f, sf::Color(30, 30, 50));
                drawRect(bx + 5.f, ry, (cardW - 10.f) * pct, 16.f, rows[r].col);
                drawRect(bx + 5.f, ry, cardW - 10.f, 16.f,
                         sf::Color::Transparent, MenuColour::CardBorder, 1.f);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Primitive helpers
    // ─────────────────────────────────────────────────────────────────────────
    bool hovered(sf::Vector2i m, float x, float y, float w, float h)
    {
        return m.x >= x && m.x < x + w && m.y >= y && m.y < y + h;
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

    void drawText(const string& s,
                  float x, float y, unsigned sz, sf::Color col,
                  bool centreX = false, float centreW = 0.f)
    {
        if (!m_fontLoaded || s.empty()) return;
        sf::Text t;
        t.setFont(m_font);
        t.setString(s);
        t.setCharacterSize(sz);
        t.setFillColor(col);
        if (centreX && centreW > 0.f)
            x = x + (centreW - t.getLocalBounds().width) / 2.f;
        t.setPosition(x, y);
        m_window.draw(t);
    }
};
