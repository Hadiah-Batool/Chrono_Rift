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
        
    void rebind() { sprite.setTexture(texture); }
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
    screens.reserve(imagePaths.size());   // no reallocation

    for (const std::string& path : imagePaths)
    {
        Screen screen;
        screen.path = path;

        if (!screen.texture.loadFromFile(path))
        {
            std::cerr << "Failed to load: " << path << std::endl;
            return false;
        }

        sf::Vector2u texSize = screen.texture.getSize();
        if (texSize.x == 0 || texSize.y == 0)
        {
            std::cerr << "Invalid texture size: " << path << std::endl;
            return false;
        }

        float scaleX = gameplayWidth  / static_cast<float>(texSize.x);
        float scaleY = gameplayHeight / static_cast<float>(texSize.y);

        screen.sprite.setTexture(screen.texture);
        screen.sprite.setScale(scaleX, scaleY);
        screen.sprite.setPosition(gameplayX, gameplayY);

        screens.push_back(std::move(screen));  //  move into vector AFTER setup
        screens.back().rebind();               //  rebind AFTER move — sprite now
                                               //    points to the moved texture
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





