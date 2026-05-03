#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>

using std::string, std::vector, std::cout, std::endl;
struct FrameData
{
    int   x, y, w, h;
    float duration;
    float centerX, centerY;
};
class AnimationFrame
{
private:
    sf::IntRect frame;
    float duration;
    sf::Vector2f Origin;   // usually width/2, height to make uneven shi centered

public:
    AnimationFrame(sf::IntRect frameRect, float frameDuration, float centerX, float centerY)
        : frame(frameRect), duration(frameDuration), Origin(centerX, centerY)
    {
    }

    sf::IntRect getFrame() const
    {
        return frame;
    }

    float getDuration() const
    {
        return duration;
    }

    sf::Vector2f getCenter() const
    {
        return Origin;
    }
};

class Animation
{
private:
    sf::Texture spriteSheet;
    vector<AnimationFrame> frames;

    float elapsedTime = 0.0f;
    int currentFrame = 0;

    bool isLooping = true;
    bool isFinished = false;
    bool isPlaying = true;

public:
    Animation() = default;
    Animation(const string& path)
    {
        spriteSheet.loadFromFile(path);
    }

    bool loadTexture(const string& path)
    {
        if (!spriteSheet.loadFromFile(path))
        {
            cout << "Failed to load texture: " << path << endl;
            return false;
        }
        return true;
    }

    void addFrame(const sf::IntRect& frameRect, float duration, float centerX, float centerY)
    {
        frames.emplace_back(frameRect, duration, centerX, centerY);
    }

    void addFrame(int x, int y, int width, int height, float duration, float centerX, float centerY)
    {
        frames.emplace_back(sf::IntRect(x, y, width, height), duration, centerX, centerY);
    }

    void update(float deltaTime)
    {
        if (!isPlaying || isFinished || frames.empty())
            return;

        elapsedTime += deltaTime;

        while (elapsedTime >= frames[currentFrame].getDuration())
        {
            elapsedTime -= frames[currentFrame].getDuration();
            currentFrame++;

            if (currentFrame >= static_cast<int>(frames.size()))
            {
                if (isLooping)
                {
                    currentFrame = 0;
                }
                else
                {
                    currentFrame = static_cast<int>(frames.size()) - 1;
                    isFinished = true;
                    isPlaying = false;
                    break;
                }
            }
        }
    }

    void applyToSprite(sf::Sprite& sprite)
    {
        if (frames.empty())
            return;

        sprite.setTexture(spriteSheet);
        sprite.setTextureRect(frames[currentFrame].getFrame());
        sprite.setOrigin(frames[currentFrame].getCenter());
    }
    // Add this inside the Animation class, after applyToSprite()
    void draw(sf::RenderWindow& window, sf::Sprite& sprite)
    {
        if (frames.empty()) return;
        applyToSprite(sprite);
        window.draw(sprite);
    }


    void reset()
    {
        elapsedTime = 0.0f;
        currentFrame = 0;
        isFinished = false;
    }

    void play()
    {
        isPlaying = true;
    }

    void stop()
    {
        isPlaying = false;
    }

    void restart()
    {
        reset();
        isPlaying = true;
    }

    void setLooping(bool looping)
    {
        isLooping = looping;
    }

    bool getLooping() const
    {
        return isLooping;
    }

    bool getFinished() const
    {
        return isFinished;
    }

    bool getPlaying() const
    {
        return isPlaying;
    }

    int getCurrentFrameIndex() const
    {
        return currentFrame;
    }

    int getFrameCount() const
    {
        return static_cast<int>(frames.size());
    }

    const sf::Texture& getTexture() const
    {
        return spriteSheet;
    }

    const AnimationFrame& getCurrentFrame() const
    {
        return frames[currentFrame];
    }

    bool empty() const
    {
        return frames.empty();
    }

    void clearFrames()
    {
        frames.clear();
        elapsedTime = 0.0f;
        currentFrame = 0;
        isFinished = false;
    }
    
};
