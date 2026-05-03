#include "../DisplayRendering/Map.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include "../DisplayRendering/Animator.h"
#include "../DisplayRendering/render.h"
#include "../Characters/Player.h" 
#include "../Characters/Enemy.h"
#include "../DisplayRendering/Menu.h"

int main()
{
    sf::RenderWindow window(
        sf::VideoMode(1200, 800),
        "Chrono Rift",
        sf::Style::Titlebar | sf::Style::Close
    );

    GameMenu menu(window,"../MapsNScreen/MenuScreen.jpg", "../MapsNScreen/Map_Overlay.png" );
    PartyConfig conf = menu.run();
    if (!conf.valid())
    {        std::cout << "No party selected, exiting.\n";
        return 0;
    }
    std::cout << "Selected level: " << conf.selectedLevel << "\n";
    std::cout << "Selected players:\n";


    // // ── Players ───────────────────────────────────────────────────────────────
    // Player chrono(PlayerType::CHRONO);
    // Player frog(PlayerType::FROG);
    // Player marle(PlayerType::MARLE);
    // Player magus(PlayerType::MAGUS);
    // chrono.loadTexture("../Players/Chrono_sprite_back_frame1.png");
    // frog.loadTexture("../Players/Frog_sprite_backframe1.png");
    // marle.loadTexture("../Players/Marle_sprite_backframe1.png");
    // magus.loadTexture("../Players/Magus_sprite_backframe1.png");
    // Enemy e1( 1, EnemyType::LAVOSCORE_ENEMY);
    // e1.loadTexture("../Enemies/LavosCore_frame1.png");
    // e1.InitAllProperties(400,400);
    // e1.setRollNumber(123, 3, 23);
    // e1.initRollStats();
    // e1.setAlive(true);
    // Enemy e2( 2, EnemyType::IMPS_ENEMY);
    // e2.loadTexture("../Enemies/MotherBrain_enemy_frame1.png");
    // e2.InitAllProperties(600,400);
    // e2.setRollNumber(456, 6, 56);
    // e2.initRollStats();
    // e2.setAlive(true);


    // chrono.InitAllProperties(260.f, 680.f);
    // chrono.setAlive(true);
    // chrono.setRollNumber(805, 5, 5);

    // magus.InitAllProperties(740.f, 700.f);
    // magus.setAlive(true);
    // magus.setRollNumber(805, 5, 5);


    // frog.InitAllProperties(450.f, 650.f);
    // marle.InitAllProperties (600.f, 620.f);
    // marle.setAlive(true);
    // marle.setRollNumber(805, 5, 5);

    // // Mess with some values so the bars look interesting
    // chrono.setHp(45);  
    // frog.setAlive(true);      // low HP — bar should go red
    // frog.setStamina(70);
    // marle.setStunned(true, 0);    // stunned — badge + bar goes yellow
    // marle.setStunEndTem(3);

    // // // ── Enemies ───────────────────────────────────────────────────────────────
    // // Enemy e1, e2, e3;
    // // e1.setHp(20);              // almost dead
    // // e2.setStunned(true);
    // // e2.setStunEndTem(2);
    // // // e3 is full health

    // // ── Map ───────────────────────────────────────────────────────────────────
    // Map map(0.0f, 0.0f, 800, 800);
    // map.loadScreens(
    //     {
    //     "../MapsNScreen/Fiaona'aForest_Lvl_tile1.png", 
    //     "../MapsNScreen/Fiaona'aForest_Lvl_tile2.png"
    // });

    // std::vector<Character*> enemies ={&e1, &e2};
    // // ── Wire up renderer ──────────────────────────────────────────────────────
    // std::vector<Player*>    players = { &chrono, &frog, &marle, &magus };
    // // std::vector<Character*> enemies = { &e1, &e2, &e3 };

    // Renderer renderer(players, enemies, &map);
    // seedTestWeapons(&chrono);

    // // // Start on player 0 (chrono) — change this to test different active players
    // // renderer.setActivePlayerIndex(0);

    // // run() blocks until window is closed
    // renderer.run();

    return 0;
}