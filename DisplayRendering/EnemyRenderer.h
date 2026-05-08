#pragma once
#include "../Characters/Enemy.h"
#include "../DisplayRendering/Animator.h"
#include <SFML/Graphics.hpp>

struct EnemyRenderer
{
    int        enemyId   = -1;
    EnemyType  enemyType = EnemyType::BLOB_ENEMY;
    bool       loaded    = false;

    Animation  anim;
    sf::Sprite sprite;

    // ── Build from shm enemy ──────────────────────────────────────────────────
    void init(const Enemy& shmEnemy)
    {
        enemyId   = shmEnemy.getEnemyId();
        enemyType = shmEnemy.getEnemyType();

        int idx = static_cast<int>(enemyType);
        const EnemySheetInfo& info = sheetData[idx];

        anim.loadTexture(info.path);
        anim.setLooping(true);

        for (int i = 0; i < info.frameCount; i++)
        {
            const FrameData& f = info.frames[i];
            anim.addFrame(f.x, f.y, f.w, f.h, f.duration, f.centerX, f.centerY);
        }

        loaded = true;
    }

    // ── Call every frame ──────────────────────────────────────────────────────
    void update(float dt, const Enemy& shmEnemy)
    {
        if (!loaded || !shmEnemy.isAlive()) return;

        anim.update(dt);
        anim.applyToSprite(sprite);

        sprite.setScale(shmEnemy.getScaleX(), shmEnemy.getScaleY());
        sprite.setPosition(shmEnemy.getXPos(), shmEnemy.getYPos());
    }

    void draw(sf::RenderWindow& window, const Enemy& shmEnemy)
    {
        if (!loaded || !shmEnemy.isAlive()) return;
        window.draw(sprite);
    }
};
