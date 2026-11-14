#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <string>
#include <optional>

#include "MazeSize.h"
#include "GameStatistics.h"

enum class MenuAction
{
    None,
    Continue,
    NewGame,
    RestartMaze,
    HowToPlay,
    Settings,
    Multiplayer,
    HostGame,
    JoinGame,
    ApplySettings,
    Back,
    Exit
};

class Menu
{
public:
    explicit Menu(const sf::Font& font);

    void resize(sf::Vector2u size);
    void setHasActiveGame(bool active);
    void setCanRestart(bool available);
    void showMain();
    void showHowToPlay();
    void showSettings(const MazeSize& selectedSize, std::optional<std::uint32_t> seed = std::nullopt);
    void showMultiplayer();
    const std::string& multiplayerAddress() const noexcept { return multiplayerAddress_; }
    void showVictory(const MazeSize& completedSize, const GameStatistics& statistics,
        std::optional<std::uint32_t> seed = std::nullopt);
    void showMessage(const std::string& message);
    MazeSize draftSize() const noexcept;
    bool draftSeed(std::optional<std::uint32_t>& seed) const noexcept;

    MenuAction handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void update(sf::Vector2f mousePosition);
    void draw(sf::RenderTarget& target) const;

private:
    enum class Screen { Main, HowToPlay, Settings, Multiplayer, Victory };

    struct Button
    {
        sf::RectangleShape shape;
        sf::Text label;
        MenuAction action;
        bool enabled = true;

        Button(const sf::Font& font, const std::string& caption, MenuAction action);
        void setBounds(sf::Vector2f position, sf::Vector2f size);
        bool hit(sf::Vector2f point) const;
        void update(sf::Vector2f mousePosition);
        void draw(sf::RenderTarget& target) const;
    };

    void refreshSettings();
    void refreshGuide();
    int guidePage_ = 0;

    const sf::Font& font_;
    Screen screen_ = Screen::Main;
    sf::Vector2u viewport_{ 900u, 600u };
    MazeSize draftSize_{ MIN_MAZE_SIZE, MIN_MAZE_SIZE };
    bool hasActiveGame_ = false;
    bool canRestart_ = false;
    std::string notice_;
    std::string draftSeedText_;
    std::string multiplayerAddress_ = "127.0.0.1:24816";
    bool seedFocused_ = false;

    sf::Text title_;
    sf::Text subtitle_;
    sf::Text rules_;
    sf::Text message_;
    sf::Text victorySummary_;
    sf::Text victoryStatistics_;
    sf::Text victorySeed_;
    sf::Text rowName_;
    sf::Text colName_;
    sf::Text rowValue_;
    sf::Text colValue_;
    sf::Text sizeSummary_;
    sf::Text seedLabel_;
    sf::Text seedValue_;
    sf::Text addressLabel_;
    sf::Text addressValue_;
    sf::RectangleShape seedField_;
    sf::RectangleShape addressField_;
    std::array<Button, 7> mainButtons_;
    std::array<Button, 4> sizeButtons_;
    Button backButton_;
    Button applyButton_;
    Button hostButton_;
    Button joinButton_;
    Button guidePrevious_, guideNext_;
};
