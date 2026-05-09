// test_players.cpp
// Standalone visual test — no arbiter, no shm, no HIP
// Tests: player spawn positions, static sprites
// Run: ./test_players

#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include "../DisplayRendering/Map.h"
#include "../Characters/Player.h"

// ─── Window size ─────────────────────────────────────────────────────────────
constexpr float T_WIN_W = 1200.f;
constexpr float T_WIN_H = 800.f;
constexpr float T_MAP_W = 860.f;
constexpr float T_MAP_H = 800.f;

// ─────────────────────────────────────────────────────────────────────────────
//  Sprite paths — one PNG per player type, edit to match your asset folder
// ─────────────────────────────────────────────────────────────────────────────
static const char* playerSpritePaths[] = {
    "../Players/Chrono_sprite_back_frame1.png",   // CHRONO
    "../Players/Frog_sprite_backframe1.png",     // FROG
    "../Players/Marle_sprite_backframe1.png",    // MARLE
    "../Players/Magus_sprite_backframe1.png",    // MAGUS
};

// ─────────────────────────────────────────────────────────────────────────────
//  TEST PLAYER TABLE — edit x, y freely to find good spawn positions
// ─────────────────────────────────────────────────────────────────────────────
struct TestPlayer
{
    float      x, y;
    PlayerType type;
    int        id;
};

static TestPlayer g_testPlayers[] = {
    { 150.f, 500.f, PlayerType::CHRONO, 0 },
    { 220.f, 540.f, PlayerType::FROG,   1 },
    { 100.f, 560.f, PlayerType::MARLE,  2 },
    { 180.f, 460.f, PlayerType::MAGUS,  3 },
};
static constexpr int NUM_TEST_PLAYERS =
    sizeof(g_testPlayers) / sizeof(g_testPlayers[0]);

// ─────────────────────────────────────────────────────────────────────────────
//  Each player needs its own texture — can't share across sf::Sprite instances
// ─────────────────────────────────────────────────────────────────────────────
struct PlayerVisual
{
    sf::Texture texture;
    sf::Sprite  sprite;
    bool        loaded = false;
};

// ─────────────────────────────────────────────────────────────────────────────
void printPositions()
{
    std::cout << "\n=== CURRENT PLAYER POSITIONS (copy into txt file) ===\n";
    std::cout << NUM_TEST_PLAYERS << "\n";
    for (int i = 0; i < NUM_TEST_PLAYERS; i++)
    {
        std::cout << (int)g_testPlayers[i].x << " "
                  << (int)g_testPlayers[i].y << " "
                  << (int)g_testPlayers[i].type
                  << "\n";
    }
    std::cout << "======================================================\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    // ── Window ────────────────────────────────────────────────────────────────
    sf::RenderWindow window(
        sf::VideoMode((unsigned)T_WIN_W, (unsigned)T_WIN_H),
        "Player Position Tester — Chrono Rift",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    // ── Map ───────────────────────────────────────────────────────────────────
    Map map(0.f, 0.f, T_MAP_W, T_MAP_H);
    if (!map.loadScreens({ "../MapsNScreen/Fiaona'aForest_Lvl_tile2.png" }))
        std::cerr << "[TEST] Map failed to load — continuing without bg\n";

    // ── Font ──────────────────────────────────────────────────────────────────
    sf::Font font;
    bool fontLoaded = font.loadFromFile("../DisplayRendering/BlockBlueprint.ttf");

    // ── Build players + visuals ───────────────────────────────────────────────
    std::vector<Player>       players;
    std::vector<PlayerVisual> visuals(NUM_TEST_PLAYERS);
    players.reserve(NUM_TEST_PLAYERS);

    for (int i = 0; i < NUM_TEST_PLAYERS; i++)
    {
        // ── Logic object ──────────────────────────────────────────────────────
        Player p(g_testPlayers[i].type);
        p.setRollNumber(0607, 7, 7);
        p.initRollStats(25.f);
        p.setAlive(true);
        p.InitAllProperties(g_testPlayers[i].x, g_testPlayers[i].y);
        players.push_back(std::move(p));

        // ── Visual object (texture lives here, not in Player) ─────────────────
        int typeIdx = (int)g_testPlayers[i].type;
        if (visuals[i].texture.loadFromFile(playerSpritePaths[typeIdx]))
        {
            visuals[i].sprite.setTexture(visuals[i].texture);

            // Apply the same scale that InitAllProperties set
            visuals[i].sprite.setScale(
                players[i].getScaleX(),
                players[i].getScaleY()
            );
            visuals[i].loaded = true;
            std::cout << "[TEST] Loaded sprite for "
                      << players[i].getName() << "\n";
        }
        else
        {
            std::cerr << "[TEST] Failed to load sprite: "
                      << playerSpritePaths[typeIdx] << "\n";
        }
    }

    printPositions();
    std::cout << "[TEST] Controls:\n"
              << "  1-4    — select player\n"
              << "  WASD   — nudge 5px\n"
              << "  Shift+WASD — nudge 20px\n"
              << "  P      — print positions\n"
              << "  ESC    — quit\n\n";

    int selected = 0;

    // ─────────────────────────────────────────────────────────────────────────
    //  Game loop
    // ─────────────────────────────────────────────────────────────────────────
    while (window.isOpen())
    {
        // ── Events ────────────────────────────────────────────────────────────
        sf::Event ev{};
        while (window.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
                window.close();

            if (ev.type == sf::Event::KeyPressed)
            {
                float step = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)
                             ? 20.f : 5.f;

                switch (ev.key.code)
                {
                    case sf::Keyboard::Escape: window.close();     break;
                    case sf::Keyboard::P:      printPositions();   break;

                    case sf::Keyboard::Num1: selected = 0; break;
                    case sf::Keyboard::Num2: selected = 1; break;
                    case sf::Keyboard::Num3: selected = 2; break;
                    case sf::Keyboard::Num4: selected = 3; break;

                    case sf::Keyboard::A:
                        g_testPlayers[selected].x -= step;
                        players[selected].setXPos(g_testPlayers[selected].x);
                        break;
                    case sf::Keyboard::D:
                        g_testPlayers[selected].x += step;
                        players[selected].setXPos(g_testPlayers[selected].x);
                        break;
                    case sf::Keyboard::W:
                        g_testPlayers[selected].y -= step;
                        players[selected].setYPos(g_testPlayers[selected].y);
                        break;
                    case sf::Keyboard::S:
                        g_testPlayers[selected].y += step;
                        players[selected].setYPos(g_testPlayers[selected].y);
                        break;

                    default: break;
                }

                selected = std::min(selected, NUM_TEST_PLAYERS - 1);
            }
        }

        // ── Sync sprite positions to player data ──────────────────────────────
        for (int i = 0; i < NUM_TEST_PLAYERS; i++)
        {
            if (visuals[i].loaded)
                visuals[i].sprite.setPosition(
                    players[i].getXPos(),
                    players[i].getYPos()
                );
        }

        // ── Draw ──────────────────────────────────────────────────────────────
        window.clear(sf::Color(10, 20, 10));
        map.draw(window);

        for (int i = 0; i < NUM_TEST_PLAYERS; i++)
        {
            // ── Sprite ────────────────────────────────────────────────────────
            if (visuals[i].loaded)
                window.draw(visuals[i].sprite);
            else
            {
                // Fallback: plain colored rectangle so you still see placement
                sf::RectangleShape fallback({ 32.f, 48.f });
                fallback.setFillColor(sf::Color(100, 180, 255, 180));
                fallback.setPosition(
                    players[i].getXPos(),
                    players[i].getYPos()
                );
                window.draw(fallback);
            }

            // ── Selection ring ────────────────────────────────────────────────
            if (i == selected)
            {
                sf::CircleShape ring(22.f);
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(sf::Color(255, 220, 50, 200));
                ring.setOutlineThickness(2.f);
                ring.setPosition(
                    g_testPlayers[i].x - 22.f,
                    g_testPlayers[i].y - 22.f
                );
                window.draw(ring);
            }

            // ── Coord label ───────────────────────────────────────────────────
            if (fontLoaded)
            {
                sf::Text label;
                label.setFont(font);
                label.setCharacterSize(11);
                label.setFillColor(i == selected
                    ? sf::Color(255, 220, 50, 255)
                    : sf::Color(200, 200, 200, 160));
                label.setString(
                    "[" + std::to_string(i) + "] "
                    + players[i].getName()
                    + "\n("
                    + std::to_string((int)g_testPlayers[i].x)
                    + ", "
                    + std::to_string((int)g_testPlayers[i].y)
                    + ")"
                );
                label.setPosition(
                    g_testPlayers[i].x - 10.f,
                    g_testPlayers[i].y - 48.f
                );
                window.draw(label);
            }
        }

        // ── Sidebar ───────────────────────────────────────────────────────────
        if (fontLoaded)
        {
            sf::RectangleShape sidebar({ 400.f, T_WIN_H });
            sidebar.setPosition(T_MAP_W, 0.f);
            sidebar.setFillColor(sf::Color(10, 10, 20, 220));
            window.draw(sidebar);

            sf::Text hint;
            hint.setFont(font);
            hint.setCharacterSize(13);
            hint.setFillColor(sf::Color(180, 180, 200, 255));
            hint.setPosition(T_MAP_W + 12.f, 20.f);

            std::string info = "PLAYER POSITION TESTER\n\n";
            info += "Selected: [" + std::to_string(selected) + "] "
                  + players[selected].getName() + "\n";
            info += "X: " + std::to_string((int)g_testPlayers[selected].x)
                  + "  Y: " + std::to_string((int)g_testPlayers[selected].y)
                  + "\n";
            info += "Scale: " + std::to_string(players[selected].getScaleX())
                  + " x "    + std::to_string(players[selected].getScaleY())
                  + "\n\n";
            info += "Controls:\n";
            info += "  1-4         select player\n";
            info += "  WASD        nudge 5px\n";
            info += "  Shift+WASD  nudge 20px\n";
            info += "  P           print positions\n";
            info += "  ESC         quit\n\n";
            info += "All players:\n";
            for (int i = 0; i < NUM_TEST_PLAYERS; i++)
            {
                info += "  [" + std::to_string(i) + "] "
                      + players[i].getName()
                      + " (" + std::to_string((int)g_testPlayers[i].x)
                      + ", " + std::to_string((int)g_testPlayers[i].y)
                      + ")"
                      + (visuals[i].loaded ? "" : " [NO SPRITE]")
                      + "\n";
            }

            hint.setString(info);
            window.draw(hint);
        }

        window.display();
    }

    std::cout << "\n[TEST] Final positions on exit:\n";
    printPositions();

    return 0;
}
