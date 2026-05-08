// test_main.cpp
// Standalone visual test — no arbiter, no shm, no HIP
// Tests: enemy spawn positions, animations, map background
// Run: ./test_game

#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include "../DisplayRendering/Map.h"
#include "../Characters/Enemy.h"

// ─── Window size matches your renderer ───────────────────────────────────────
constexpr float T_WIN_W = 1200.f;
constexpr float T_WIN_H = 800.f;
constexpr float T_MAP_W = 800.f;
constexpr float T_MAP_H = 800.f;

// ─────────────────────────────────────────────────────────────────────────────
//  TEST ENEMY TABLE — edit x, y, type freely to find good spawn positions
//  These coords are what you'll paste into level_1_sublevel_1.txt later
// ─────────────────────────────────────────────────────────────────────────────
struct TestEnemy
{
    float     x, y;
    EnemyType type;
    int       id;
};

static TestEnemy g_testEnemies[] = {
    // { 150.f, 400.f, EnemyType::GOBLIN_OGAN_ENEMY, 0 },
    { 350.f, 310.f, EnemyType::IMPS_ENEMY,         1 },
    { 495.f, 450.f, EnemyType::BEAST_ENEMY,         2 },
    { 365.f, 270.f, EnemyType::ALIEN_ENEMY,         3 },
    // ── Add / comment out enemies here to test positions ──────────────────
    { 275.f, 545.f, EnemyType::CYBOT_ENEMY,      4 },
    { 220.f, 420.f, EnemyType::MUTANT_ENEMY,     5 },
    { 620.f, 410.f, EnemyType::NIZBELN_ENEMY,    6 },
    { 580.f, 520.f, EnemyType::LAVOSCORE_ENEMY, 8 },
    // { 400.f, 600.f, EnemyType::MOTHERnBRAIN_ENEMY, 9 },
    //  { 300.f, 300.f, EnemyType::DRAGONTANK_ENEMY, 7 },
     { 485.f, 385.f, EnemyType::BLOB_ENEMY, 11 }
};
static constexpr int NUM_TEST_ENEMIES =
    sizeof(g_testEnemies) / sizeof(g_testEnemies[0]);

// ─────────────────────────────────────────────────────────────────────────────
//  printPositions — dumps current positions to terminal so you can copy-paste
//  into your txt file
// ─────────────────────────────────────────────────────────────────────────────
void printPositions()
{
    std::cout << "\n=== CURRENT ENEMY POSITIONS (copy into txt file) ===\n";
    std::cout << NUM_TEST_ENEMIES << "\n";
    for (int i = 0; i < NUM_TEST_ENEMIES; i++)
    {
        std::cout << (int)g_testEnemies[i].x << " "
                  << (int)g_testEnemies[i].y << " "
                  << (int)g_testEnemies[i].type
                  << "\n";
    }
    std::cout << "=====================================================\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    // ── Window ────────────────────────────────────────────────────────────────
    sf::RenderWindow window(
        sf::VideoMode((unsigned)T_WIN_W, (unsigned)T_WIN_H),
        "Enemy Position Tester — Fiona's Forest",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    // ── Map ───────────────────────────────────────────────────────────────────
    Map map(0.f, 0.f, T_MAP_W, T_MAP_H);
    if (!map.loadScreens({ "../MapsNScreen/Fiaona'aForest_Lvl_tile2.png" }))
        std::cerr << "[TEST] Map failed to load — continuing without bg\n";

    // ── Font (for overlay labels) ─────────────────────────────────────────────
    sf::Font font;
    bool fontLoaded = font.loadFromFile("../DisplayRendering/BlockBlueprint.ttf");

    // ── Build enemies ─────────────────────────────────────────────────────────
    std::vector<Enemy> enemies;
    enemies.reserve(NUM_TEST_ENEMIES);

    for (int i = 0; i < NUM_TEST_ENEMIES; i++)
    {
        Enemy e(g_testEnemies[i].id, g_testEnemies[i].type);

        // Give it some stats so it doesn't crash on getters
        e.setRollNumber(240607, 7, 7);
        e.initRollStats();
        e.setAlive(true);
        e.InitAllProperties(g_testEnemies[i].x, g_testEnemies[i].y);

        // Load animation from the global sheetData table
        int sheetIdx = (int)g_testEnemies[i].type;
        e.initAnimation(sheetData[sheetIdx]);

        enemies.push_back(std::move(e));
    }

    // ── Print initial positions ───────────────────────────────────────────────
    printPositions();
    std::cout << "[TEST] Controls:\n"
              << "  P      — print current positions to terminal\n"
              << "  1-9    — select enemy (by index)\n"
              << "  WASD   — nudge selected enemy 5px\n"
              << "  Shift+WASD — nudge 20px\n"
              << "  ESC    — quit\n\n";

    // ── Selection state ───────────────────────────────────────────────────────
    int  selected  = 0;
    sf::Clock clock;

    // ─────────────────────────────────────────────────────────────────────────
    //  Game loop
    // ─────────────────────────────────────────────────────────────────────────
    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        // ── Events ────────────────────────────────────────────────────────────
        sf::Event ev{};
        while (window.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
                window.close();

            if (ev.type == sf::Event::KeyPressed)
            {
                float step = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ? 20.f : 5.f;

                switch (ev.key.code)
                {
                    case sf::Keyboard::Escape: window.close(); break;
                    case sf::Keyboard::P:      printPositions(); break;

                    // ── Select enemy by number key ─────────────────────────────
                    case sf::Keyboard::Num1: selected = 0; break;
                    case sf::Keyboard::Num2: selected = 1; break;
                    case sf::Keyboard::Num3: selected = 2; break;
                    case sf::Keyboard::Num4: selected = 3; break;
                    case sf::Keyboard::Num5: selected = 4; break;
                    case sf::Keyboard::Num6: selected = 5; break;
                    case sf::Keyboard::Num7: selected = 6; break;
                    case sf::Keyboard::Num8: selected = 7; break;
                    case sf::Keyboard::Num9: selected = 8; break;

                    // ── Nudge selected enemy ───────────────────────────────────
                    case sf::Keyboard::A:
                        if (selected < NUM_TEST_ENEMIES)
                        {
                            g_testEnemies[selected].x -= step;
                            enemies[selected].setXPos(g_testEnemies[selected].x);
                        }
                        break;
                    case sf::Keyboard::D:
                        if (selected < NUM_TEST_ENEMIES)
                        {
                            g_testEnemies[selected].x += step;
                            enemies[selected].setXPos(g_testEnemies[selected].x);
                        }
                        break;
                    case sf::Keyboard::W:
                        if (selected < NUM_TEST_ENEMIES)
                        {
                            g_testEnemies[selected].y -= step;
                            enemies[selected].setYPos(g_testEnemies[selected].y);
                        }
                        break;
                    case sf::Keyboard::S:
                        if (selected < NUM_TEST_ENEMIES)
                        {
                            g_testEnemies[selected].y += step;
                            enemies[selected].setYPos(g_testEnemies[selected].y);
                        }
                        break;

                    default: break;
                }

                selected = std::min(selected, NUM_TEST_ENEMIES - 1);
            }
        }

        // ── Update animations ─────────────────────────────────────────────────
        for (auto& e : enemies)
            e.updateAnimation(dt);

        // ── Draw ──────────────────────────────────────────────────────────────
        window.clear(sf::Color(10, 20, 10));
        map.draw(window);

        for (int i = 0; i < (int)enemies.size(); i++)
        {
            enemies[i].draw(window);

            // ── Selection highlight ring ───────────────────────────────────────
            if (i == selected)
            {
                sf::CircleShape ring(22.f);
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(sf::Color(255, 220, 50, 200));
                ring.setOutlineThickness(2.f);
                ring.setPosition(
                    g_testEnemies[i].x - 22.f,
                    g_testEnemies[i].y - 22.f
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
                    + enemies[i].getName()
                    + "\n("
                    + std::to_string((int)g_testEnemies[i].x)
                    + ", "
                    + std::to_string((int)g_testEnemies[i].y)
                    + ")"
                );
                label.setPosition(g_testEnemies[i].x - 10.f,
                                  g_testEnemies[i].y - 48.f);
                window.draw(label);
            }
        }

        // ── Sidebar hint ──────────────────────────────────────────────────────
        if (fontLoaded)
        {
            sf::RectangleShape sidebar({400.f, T_WIN_H});
            sidebar.setPosition(T_MAP_W, 0.f);
            sidebar.setFillColor(sf::Color(10, 10, 20, 220));
            window.draw(sidebar);

            sf::Text hint;
            hint.setFont(font);
            hint.setCharacterSize(13);
            hint.setFillColor(sf::Color(180, 180, 200, 255));
            hint.setPosition(T_MAP_W + 12.f, 20.f);

            std::string info = "ENEMY POSITION TESTER\n\n";
            info += "Selected: [" + std::to_string(selected) + "] "
                  + enemies[selected].getName() + "\n";
            info += "X: " + std::to_string((int)g_testEnemies[selected].x)
                  + "  Y: " + std::to_string((int)g_testEnemies[selected].y) + "\n\n";
            info += "Controls:\n";
            info += "  1-9     select enemy\n";
            info += "  WASD    nudge 5px\n";
            info += "  Shift+WASD  nudge 20px\n";
            info += "  P       print positions\n";
            info += "  ESC     quit\n\n";
            info += "All enemies:\n";
            for (int i = 0; i < NUM_TEST_ENEMIES; i++)
            {
                info += "  [" + std::to_string(i) + "] "
                      + enemies[i].getName()
                      + " (" + std::to_string((int)g_testEnemies[i].x)
                      + ", " + std::to_string((int)g_testEnemies[i].y) + ")\n";
            }

            hint.setString(info);
            window.draw(hint);
        }

        window.display();
    }

    // ── Final positions on exit ───────────────────────────────────────────────
    std::cout << "\n[TEST] Final positions on exit:\n";
    printPositions();

    return 0;
}
