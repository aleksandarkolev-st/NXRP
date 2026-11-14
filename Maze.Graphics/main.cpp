#include <SFML/Graphics.hpp>
#include <exception>
#include <iostream>

#include "ExecutablePath.h"
#include "MazeApplication.h"

int main()
{
    try
    {
        const auto fontPath = executableDirectory() / "assets" / "fonts" / "SourceSans3-Semibold.ttf";
        sf::Font font;
        if (!font.openFromFile(fontPath))
        {
            std::cerr << "Could not load the font: " << fontPath << '\n';
            return 1;
        }
        font.setSmooth(false);

        sf::ContextSettings context;
        context.depthBits = 24;
        sf::RenderWindow window(sf::VideoMode({ 900u, 600u }), "WAYWARD - The Living Labyrinth", sf::Style::Default,
            sf::State::Windowed, context);
        window.setMinimumSize(sf::Vector2u{ 640u, 560u });
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        MazeApplication application(font);
        application.resize(window.getSize());
        while (window.isOpen())
        {
            while (const auto event = window.pollEvent())
            {
                application.handleEvent(*event, window);
                if (!window.isOpen()) break;
            }
            if (!window.isOpen()) break;

            application.update(window);
            window.clear(sf::Color{ 13, 19, 17 });
            application.draw(window);
            window.display();
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NXRP could not start: " << error.what() << '\n';
        return 1;
    }
}
