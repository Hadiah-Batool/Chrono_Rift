#include "../DisplayRendering/Map.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include "../DisplayRendering/Animator.h"
#include "../DisplayRendering/render.h"
#include "../Characters/Player.h" 
#include "../Characters/Enemy.h"

int main()
{
    // ── Players ───────────────────────────────────────────────────────────────
    Player chrono(PlayerType::CHRONO);
    Player frog(PlayerType::FROG);
    Player marle(PlayerType::MARLE);
    chrono.loadTexture("../Players/Chrono_sprite_frame1.png");
    frog.loadTexture("../Players/Frog_sprite_frame1.png");
    marle.loadTexture("../Players/Marle_sprite_frame1.png");
    Enemy e1( 1, EnemyType::RATnGERMLIN_ENEMY);
    e1.loadTexture("../Enemies/RatNGremlin_enemy_frame1.png");
    e1.InitAllProperties(200,400);
    e1.setRollNumber(123, 3, 23);
    e1.initRollStats();
    e1.setAlive(true);


    chrono.InitAllProperties(200.f, 700.f);
    chrono.setAlive(true);
    chrono.setRollNumber(805, 5, 5);
    frog.InitAllProperties(400.f, 400.f);
    marle.InitAllProperties(600.f, 400.f);

    // Mess with some values so the bars look interesting
    chrono.setHp(45);          // low HP — bar should go red
    frog.setStamina(70);
    marle.setStunned(true, 0);    // stunned — badge + bar goes yellow
    marle.setStunEndTem(3);

    // // ── Enemies ───────────────────────────────────────────────────────────────
    // Enemy e1, e2, e3;
    // e1.setHp(20);              // almost dead
    // e2.setStunned(true);
    // e2.setStunEndTem(2);
    // // e3 is full health

    // ── Map ───────────────────────────────────────────────────────────────────
    Map map(0.0f, 0.0f, 800, 800);
    map.loadScreens(
        {
        "../MapsNScreen/Fiaona'aForest_Lvl_tile1.png"
        // "../MapsNScreen/Fiaona'aForest_Lvl_tile2.png"
    });

    std::vector<Character*> enemies ={&e1};
    // ── Wire up renderer ──────────────────────────────────────────────────────
    std::vector<Player*>    players = { &chrono, &frog, &marle };
    // std::vector<Character*> enemies = { &e1, &e2, &e3 };

    Renderer renderer(players, enemies, &map);
    seedTestWeapons(&chrono);

    // Start on player 0 (chrono) — change this to test different active players
    renderer.setActivePlayerIndex(0);

    // run() blocks until window is closed
    renderer.run();

    return 0;
}