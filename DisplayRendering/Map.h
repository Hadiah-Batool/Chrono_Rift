#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>
    struct Screen
    {
        sf::Texture texture;
        sf::Sprite sprite;
        std::string path;
    };
class Map
{
private:


    std::vector<Screen> screens;
    int currentScreenIndex;

    float gameplayX;
    float gameplayY;
    float gameplayWidth;
    float gameplayHeight;

public:
   Map(float x, float y, float width, float height)
    : currentScreenIndex(0),
      gameplayX(x),
      gameplayY(y),
      gameplayWidth(width),
      gameplayHeight(height)
{

}

bool loadScreens(const std::vector<std::string>& imagePaths)
{
    screens.clear();
    int i=0;
    for (const std::string& path : imagePaths)
    {
        std::cout << "Trying to load: " << path << std::endl;

        screens.emplace_back();
        Screen& screen = screens.back();

        screen.path = path;

        if (!screen.texture.loadFromFile(path))
        {
            std::cerr << "Failed to load map screen: " << path << std::endl;
            screens.pop_back();
            return false;
        }

        sf::Vector2u texSize = screen.texture.getSize();
        std::cout << "Loaded: " << path << " size = "
                  << texSize.x << "x" << texSize.y << std::endl;

        if (texSize.x == 0 || texSize.y == 0)
        {
            std::cerr << "Invalid texture size for: " << path << std::endl;
            screens.pop_back();
            return false;
        }

        screen.sprite.setTexture(screen.texture);

        float scaleX = gameplayWidth / static_cast<float>(texSize.x);
        float scaleY = gameplayHeight / static_cast<float>(texSize.y);

        std::cout << "ScaleX: " << scaleX << " ScaleY: " << scaleY << std::endl;

        screen.sprite.setScale(scaleX, scaleY);
        screen.sprite.setPosition(gameplayX +  i*800, gameplayY);
        i++;
    }

    currentScreenIndex = 0;
    return true;
}



void draw(sf::RenderWindow& window)
{
    if (screens.empty())
        return;

    window.draw(screens[currentScreenIndex].sprite);
    
}

void nextScreen()
{
    if (screens.empty())
        return;

    if (currentScreenIndex < static_cast<int>(screens.size()) - 1)
        currentScreenIndex++;
}

void previousScreen()
{
    if (screens.empty())
        return;

    if (currentScreenIndex > 0)
        currentScreenIndex--;
}

void setCurrentScreen(int index)
{
    if (index >= 0 && index < static_cast<int>(screens.size()))
        currentScreenIndex = index;
}

int getCurrentScreenIndex() const
{
    return currentScreenIndex;
}

int getScreenCount() const
{
    return static_cast<int>(screens.size());
}

void printDebugInfo() const
{
    std::cout << "Map debug info:\n";
    std::cout << "Total screens: " << screens.size() << "\n";
    std::cout << "Current screen index: " << currentScreenIndex << "\n";

    for (size_t i = 0; i < screens.size(); i++)
    {
        std::cout << "Screen " << i << ": " << screens[i].path << "\n";
    }
}

};





