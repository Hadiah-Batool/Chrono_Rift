#include <SFML/Graphics.hpp>

int main() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "SFML Test");
    window.setFramerateLimit(60);

    sf::CircleShape circle(50.f);
    circle.setFillColor(sf::Color::Red);
    circle.setPosition(400.f, 300.f);

    sf::Vector2f velocity(3.f, 3.f);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        // Move
        circle.move(velocity);

        // Bounce off walls
        sf::Vector2f pos = circle.getPosition();
        if (pos.x < 0 || pos.x + 100 > 800) velocity.x = -velocity.x;
        if (pos.y < 0 || pos.y + 100 > 600) velocity.y = -velocity.y;

        window.clear(sf::Color::White);
        window.draw(circle);
        window.display();
    }

    return 0;
}
