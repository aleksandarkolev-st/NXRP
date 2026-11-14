#include "menu.h"
#include "Maze.Validation.h"
#include "GameStatisticsText.h"
#include "Maze.Generation.h"
#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace
{
    const sf::Color textColor{ 231, 226, 198 };
    const sf::Color mutedColor{ 151, 163, 144 };

    void centerText(sf::Text& text, sf::Vector2f position)
    {
        const auto bounds = text.getLocalBounds();
        text.setOrigin({
            std::round(bounds.position.x + bounds.size.x / 2.f),
            std::round(bounds.position.y + bounds.size.y / 2.f)
        });
        text.setPosition({ std::round(position.x), std::round(position.y) });
    }

    std::string wrapText(const std::string& text, const sf::Font& font,
        unsigned int characterSize, float maximumWidth)
    {
        sf::Text measure(font, "", characterSize);
        std::istringstream paragraphs(text);
        std::string paragraph;
        std::string result;
        bool firstParagraph = true;
        while (std::getline(paragraphs, paragraph))
        {
            if (!firstParagraph) result += '\n';
            firstParagraph = false;
            std::istringstream words(paragraph);
            std::string word;
            std::string line;
            while (words >> word)
            {
                const auto candidate = line.empty() ? word : line + " " + word;
                measure.setString(candidate);
                if (!line.empty() && measure.getLocalBounds().size.x > maximumWidth)
                {
                    result += line + '\n';
                    line = word;
                }
                else line = candidate;
            }
            result += line;
        }
        return result;
    }
}

Menu::Button::Button(const sf::Font& font, const std::string& caption, MenuAction value)
    : label(font, caption, 24), action(value)
{
    shape.setOutlineThickness(1.f);
    update({ -1.f, -1.f });
}

void Menu::Button::setBounds(sf::Vector2f position, sf::Vector2f size)
{
    shape.setPosition({ std::round(position.x), std::round(position.y) });
    shape.setSize(size);
    centerText(label, shape.getPosition() + size / 2.f);
}

bool Menu::Button::hit(sf::Vector2f point) const
{
    return enabled && shape.getGlobalBounds().contains(point);
}

void Menu::Button::update(sf::Vector2f mousePosition)
{
    const bool hovered = hit(mousePosition);
    shape.setFillColor(!enabled ? sf::Color{ 22, 28, 26 }
        : hovered ? sf::Color{ 67, 76, 57 } : sf::Color{ 35, 44, 37 });
    shape.setOutlineColor(!enabled ? sf::Color{ 40, 48, 42 }
        : hovered ? sf::Color{ 190, 178, 126 } : sf::Color{ 81, 94, 75 });
    label.setFillColor(enabled ? textColor : sf::Color{ 103, 113, 100 });
}

void Menu::Button::draw(sf::RenderTarget& target) const
{
    target.draw(shape);
    target.draw(label);
}

Menu::Menu(const sf::Font& font)
    : font_(font), title_(font, "", 42), subtitle_(font, "", 19),
      rules_(font, "", 20), message_(font, "", 18), victorySummary_(font, "", 24),
      victoryStatistics_(font, "", 22),
      victorySeed_(font, "", 20),
      rowName_(font, "Rows", 24), colName_(font, "Columns", 24),
      rowValue_(font, "", 28), colValue_(font, "", 28), sizeSummary_(font, "", 19),
      seedLabel_(font, "Seed", 24), seedValue_(font, "", 22),
      addressLabel_(font, "Host address", 20), addressValue_(font, "", 20),
      mainButtons_{
          Button(font, "Continue", MenuAction::Continue),
          Button(font, "Begin expedition", MenuAction::NewGame),
          Button(font, "Restart maze", MenuAction::RestartMaze),
          Button(font, "How to Play", MenuAction::HowToPlay),
          Button(font, "Settings", MenuAction::Settings),
          Button(font, "Multiplayer", MenuAction::Multiplayer),
          Button(font, "Exit", MenuAction::Exit)
      },
      sizeButtons_{
          Button(font, "-", MenuAction::None), Button(font, "+", MenuAction::None),
          Button(font, "-", MenuAction::None), Button(font, "+", MenuAction::None)
      },
      backButton_(font, "Back", MenuAction::Back),
      applyButton_(font, "Apply", MenuAction::ApplySettings),
      hostButton_(font, "Host game", MenuAction::HostGame),
      joinButton_(font, "Join game", MenuAction::JoinGame),
      guidePrevious_(font, "Previous", MenuAction::None),
      guideNext_(font, "Next", MenuAction::None)
{
    for (auto* text : { &title_, &rules_, &victorySummary_, &rowName_, &colName_, &rowValue_, &colValue_, &seedLabel_, &seedValue_ })
        text->setFillColor(textColor);
    for (auto* text : { &subtitle_, &message_, &sizeSummary_, &victoryStatistics_, &victorySeed_ })
        text->setFillColor(mutedColor);
    rules_.setLineSpacing(1.1f);
    seedField_.setFillColor({ 22, 28, 26 });
    seedField_.setOutlineThickness(1.f);
    addressLabel_.setFillColor(textColor); addressValue_.setFillColor(textColor);
    addressField_.setFillColor({ 22, 28, 26 }); addressField_.setOutlineThickness(1.f);
    addressField_.setOutlineColor({ 190, 178, 126 });
    showMain();
}

void Menu::resize(sf::Vector2u size)
{
    if (size.x == 0 || size.y == 0) return;
    viewport_ = size;
    const float width = static_cast<float>(size.x);
    const float height = static_cast<float>(size.y);
    const float center = std::floor(width / 2.f);
    title_.setFillColor(screen_ == Screen::Victory ? sf::Color{ 190, 199, 166 } : textColor);
    centerText(title_, { center, 56.f });
    centerText(subtitle_, { center, 103.f });

    const float buttonWidth = std::min(320.f, width - 80.f);
    constexpr float buttonStep = 48.f;
    const float mainHeight = buttonStep * static_cast<float>(mainButtons_.size() - 1) + 42.f;
    const float top = std::max(144.f, (height - mainHeight) / 2.f + 16.f);
    for (std::size_t index = 0; index < mainButtons_.size(); ++index)
        mainButtons_[index].setBounds({ center - buttonWidth / 2.f, top + buttonStep * static_cast<float>(index) },
            { buttonWidth, 42.f });

    message_.setString(wrapText(notice_, font_, 18, width - 80.f));
    centerText(message_, { center, height - 45.f });
    centerText(victorySummary_, { center, std::floor(height / 2.f - 100.f) });
    centerText(victoryStatistics_, { center, std::floor(height / 2.f - 64.f) });
    centerText(victorySeed_, { center, std::floor(height / 2.f - 138.f) });
    static constexpr const char* guidePages[]={
        "Explore the living labyrinth. E opens treasure caches for ingredients and crowns. New districts reward 4 crowns. Tab opens an atlas of the routes you have explored.\n\n"
        "I opens your satchel; 1-6 uses an item. Potions restore breath, conceal you or speed you up. Relics reveal paths, recall you to a refuge or cast a distracting echo.\n\n"
        "WASD moves; mouse looks; Shift runs; Space jumps; Ctrl/C crouches. F reads belongings; J rereads notes.",
        "Press E at the entrance inscription. Find three engraved stones, then follow the task: touch them in order, or light stones whose numbers total the target.\n\n"
        "E toggles a balance stone; * marks lit stones. A wrong sequence resets its progress.\n\n"
        "At the gate, transform the stone numbers using its inscription. Type 1-9, Enter to try, Backspace to edit. Walk through the open gate.",
        "Trading posts are your bases. O at the keeper's counter opens the shop. 1-6 buys an item; 7 sells one of each ingredient for 9 crowns.\n\n"
        "N inside a post opens the workbench. Recipes use dewleaf, moon salt and old brass from caches. I opens your satchel; Q closes menus. Entering a post sets your Recall destination.\n\n"
        "The warden can enter. Find an exit on the first strike; leave on the second. The third forces the door. Veil hides you from sight, but noise still carries.",
        "P at the downstairs bar deals Keeper's Draw. Press 1-5 to hold cards, then P to replace the rest once. P deals again after the result; Q puts the cards away. Scores are individual.\n\n"
        "Upstairs, G picks up or throws the basketball. Keep facing the hoop through the wind-up. K kicks the football. Scores are shared.\n\n"
        "All games are free play. The world keeps moving while you play, and card noise can give you away.",
        "Multiplayer hosts up to four survivors. Everyone needs the same build. Host listens on TCP 24816; guests enter the host's reachable address or TCP tunnel.\n\n"
        "Hold V for nearby voice chat. Walls and doors muffle speech. F8 toggles voice; F1-F4 mute individual speakers locally. Voice can attract the hunter.\n\n"
        "Esc pauses. R restarts this maze. T toggles ray-traced light; B toggles head motion; M toggles game sound. Settings chooses the next size and seed."
    };
    for (unsigned int fontSize = 21; fontSize >= 18; --fontSize)
    {
        rules_.setCharacterSize(fontSize);
        rules_.setString(wrapText(guidePages[guidePage_], font_, fontSize, width - 112.f));
        const auto body=rules_.getLocalBounds();
        if (body.position.y+body.size.y <= height - 279.f || fontSize == 18) break;
    }
    rules_.setOrigin({ 0.f, 0.f });
    rules_.setPosition({ std::round((width - rules_.getLocalBounds().size.x) / 2.f), 155.f });

    const float rowTop = std::floor(height / 2.f - 120.f);
    const float colTop = rowTop + 76.f;
    rowName_.setPosition({ center - 190.f, rowTop + 8.f });
    colName_.setPosition({ center - 190.f, colTop + 8.f });
    sizeButtons_[0].setBounds({ center - 40.f, rowTop }, { 48.f, 48.f });
    sizeButtons_[1].setBounds({ center + 100.f, rowTop }, { 48.f, 48.f });
    sizeButtons_[2].setBounds({ center - 40.f, colTop }, { 48.f, 48.f });
    sizeButtons_[3].setBounds({ center + 100.f, colTop }, { 48.f, 48.f });
    seedLabel_.setPosition({ center - 190.f, std::floor(height / 2.f + 40.f) });
    seedField_.setPosition({ center - 40.f, std::floor(height / 2.f + 32.f) });
    seedField_.setSize({ 220.f, 44.f });
    refreshSettings();
    addressLabel_.setPosition({ center - 205.f, 214.f });
    addressField_.setPosition({ center - 205.f, 245.f }); addressField_.setSize({ 410.f, 44.f });
    addressValue_.setPosition({ center - 193.f, 254.f });
    auto visibleAddress = multiplayerAddress_;
    addressValue_.setString(visibleAddress);
    while (!visibleAddress.empty() && addressValue_.getLocalBounds().size.x > 386.f)
    { visibleAddress.erase(0, 1); addressValue_.setString("..." + visibleAddress); }
    hostButton_.setBounds({ center - 205.f, 320.f }, { 195.f, 48.f });
    joinButton_.setBounds({ center + 10.f, 320.f }, { 195.f, 48.f });

    backButton_.label.setString(screen_ == Screen::Settings ? "Cancel"
        : screen_ == Screen::Victory ? "Main menu" : "Back");
    if (screen_ == Screen::Settings)
    {
        backButton_.setBounds({ center - 160.f, height - 108.f }, { 146.f, 48.f });
        applyButton_.setBounds({ center + 14.f, height - 108.f }, { 146.f, 48.f });
    }
    else if (screen_ == Screen::Victory)
    {
        mainButtons_[2].setBounds({ center - buttonWidth / 2.f, std::floor(height / 2.f - 44.f) },
            { buttonWidth, 48.f });
        mainButtons_[1].setBounds({ center - buttonWidth / 2.f, std::floor(height / 2.f + 16.f) },
            { buttonWidth, 48.f });
        backButton_.setBounds({ center - buttonWidth / 2.f, std::floor(height / 2.f + 76.f) },
            { buttonWidth, 48.f });
    }
    else backButton_.setBounds({ center - 100.f, height - 108.f }, { 200.f, 48.f });
    if(screen_==Screen::HowToPlay)
    {
        guidePrevious_.setBounds({center-280.f,height-108.f},{160.f,48.f});
        guideNext_.setBounds({center+120.f,height-108.f},{160.f,48.f});
    }
}

void Menu::setHasActiveGame(bool active)
{
    hasActiveGame_ = active;
    mainButtons_[0].enabled = active;
}

void Menu::setCanRestart(bool available)
{
    canRestart_ = available;
    mainButtons_[2].enabled = available;
}

void Menu::showMain()
{
    screen_ = Screen::Main;
    title_.setString("WAYWARD");
    subtitle_.setString("Trade. Brew. Find your way through the living labyrinth.");
    mainButtons_[0].enabled = hasActiveGame_;
    mainButtons_[2].enabled = canRestart_;
    if (hasActiveGame_) notice_ = "An active game is available.";
    else if (canRestart_) notice_ = "Maze complete. Restart it or choose New Game.";
    else notice_ = "No active game to continue.";
    resize(viewport_);
}

void Menu::showHowToPlay()
{
    screen_ = Screen::HowToPlay;
    guidePage_=0;
    title_.setString("KEEPER'S FIELD GUIDE");
    refreshGuide();
}

void Menu::refreshGuide()
{
    static constexpr const char* chapters[]={"AN EXPEDITION BEGINS","THE STONES AND THE GATE","KEEPERS AND ALCHEMY","GAMES AFTER DARK","CO-OP AND CONTROLS"};
    guidePage_=std::clamp(guidePage_,0,4);
    subtitle_.setString(std::to_string(guidePage_+1)+" / 5   -   "+chapters[guidePage_]);
    guidePrevious_.enabled=guidePage_>0;guideNext_.enabled=guidePage_<4;
    resize(viewport_);
}

void Menu::showSettings(const MazeSize& selectedSize, std::optional<std::uint32_t> seed)
{
    if (!isValidMazeSize(selectedSize))
        throw std::invalid_argument("Settings require rows and columns between 10 and 100.");
    screen_ = Screen::Settings;
    draftSize_ = selectedSize;
    draftSeedText_ = seed ? std::to_string(*seed) : "";
    seedFocused_ = true;
    title_.setString("Settings");
    subtitle_.setString("Choose size and seed for your next game.");
    resize(viewport_);
}

void Menu::showMultiplayer()
{
    screen_ = Screen::Multiplayer;
    title_.setString("Multiplayer");
    subtitle_.setString("Up to four players. Host or join a TCP address.");
    notice_ = "Host forwards port 24816 with ngrok; share the TCP address.";
    resize(viewport_);
}

void Menu::showMessage(const std::string& message)
{
    notice_ = message;
    resize(viewport_);
}

void Menu::showVictory(const MazeSize& completedSize, const GameStatistics& statistics,
    std::optional<std::uint32_t> seed)
{
    if (!isValidMazeSize(completedSize))
        throw std::invalid_argument("Victory requires valid completed maze dimensions.");
    screen_ = Screen::Victory;
    title_.setString("You escaped!");
    subtitle_.setString("The maze is complete.");
    victorySummary_.setString("Maze completed: " + std::to_string(completedSize.rows)
        + " x " + std::to_string(completedSize.cols));
    victoryStatistics_.setString(gameStatisticsText(statistics));
    victorySeed_.setString(seed ? "Seed " + std::to_string(*seed) : "Seed unavailable");
    notice_ = "R restarts this maze. Esc returns to the main menu.";
    resize(viewport_);
}

MazeSize Menu::draftSize() const noexcept
{
    return draftSize_;
}

bool Menu::draftSeed(std::optional<std::uint32_t>& seed) const noexcept
{
    return parseMazeSeed(draftSeedText_, seed);
}

void Menu::refreshSettings()
{
    rowValue_.setString(std::to_string(draftSize_.rows));
    colValue_.setString(std::to_string(draftSize_.cols));
    const float center = std::floor(static_cast<float>(viewport_.x) / 2.f);
    const float rowTop = std::floor(static_cast<float>(viewport_.y) / 2.f - 120.f);
    centerText(rowValue_, { center + 54.f, rowTop + 24.f });
    centerText(colValue_, { center + 54.f, rowTop + 100.f });
    std::optional<std::uint32_t> seed;
    const bool validSeed = draftSeed(seed);
    applyButton_.enabled = validSeed;
    seedField_.setOutlineColor(!validSeed ? sf::Color{ 236, 106, 106 }
        : seedFocused_ ? sf::Color{ 190, 178, 126 } : sf::Color{ 81, 94, 75 });
    auto visibleSeed = draftSeedText_;
    seedValue_.setString(draftSeedText_.empty() ? "Random" : visibleSeed);
    while (!visibleSeed.empty() && seedValue_.getLocalBounds().size.x > seedField_.getSize().x - 24.f)
    {
        visibleSeed.erase(0, 1);
        seedValue_.setString("..." + visibleSeed);
    }
    seedValue_.setPosition(seedField_.getPosition() + sf::Vector2f{ 12.f, 8.f });
    sizeSummary_.setFillColor(validSeed ? mutedColor : sf::Color{ 236, 106, 106 });
    sizeSummary_.setString(std::string(validSeed ? "Rows/cols: 10-100. Blank seed = random."
        : "Seed: 0-4294967295, or leave blank.")
        + "\nSize persists. Seed applies for this launch."
        + "\nHold Shift and click +/- to change by 10.");
    centerText(sizeSummary_, { center, std::floor(static_cast<float>(viewport_.y) / 2.f + 112.f) });
    sizeButtons_[0].enabled = draftSize_.rows > MIN_MAZE_SIZE;
    sizeButtons_[1].enabled = draftSize_.rows < MAX_MAZE_SIZE;
    sizeButtons_[2].enabled = draftSize_.cols > MIN_MAZE_SIZE;
    sizeButtons_[3].enabled = draftSize_.cols < MAX_MAZE_SIZE;
}

MenuAction Menu::handleEvent(const sf::Event& event, const sf::RenderWindow& window)
{
    if (screen_ == Screen::Multiplayer)
    {
        if (const auto* text = event.getIf<sf::Event::TextEntered>())
        {
            if (text->unicode == U'\b') { if (!multiplayerAddress_.empty()) multiplayerAddress_.pop_back(); }
            else if (text->unicode >= 33 && text->unicode < 127 && multiplayerAddress_.size() < 128)
                multiplayerAddress_ += static_cast<char>(text->unicode);
            resize(viewport_);
            return MenuAction::None;
        }
    }
    if (screen_ == Screen::Settings && seedFocused_)
    {
        if (const auto* text = event.getIf<sf::Event::TextEntered>())
        {
            if (text->unicode == U'\b')
            {
                if (!draftSeedText_.empty()) draftSeedText_.pop_back();
            }
            else if (text->unicode >= 32 && text->unicode != 127 && draftSeedText_.size() < 32)
                draftSeedText_ += text->unicode < 127 ? static_cast<char>(text->unicode) : '?';
            refreshSettings();
            return MenuAction::None;
        }
    }
    if (const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(screen_==Screen::HowToPlay && (key->code==sf::Keyboard::Key::Left || key->code==sf::Keyboard::Key::Right))
        {
            guidePage_+=key->code==sf::Keyboard::Key::Left?-1:1;refreshGuide();return MenuAction::None;
        }
        if (screen_ == Screen::Multiplayer)
        {
            if (key->code == sf::Keyboard::Key::Enter) return MenuAction::JoinGame;
            if (key->code == sf::Keyboard::Key::Delete) multiplayerAddress_.clear();
            if (key->control && key->code == sf::Keyboard::Key::V)
            {
                multiplayerAddress_.clear();
                for (const auto code : sf::Clipboard::getString())
                    if (code >= 33 && code < 127 && multiplayerAddress_.size() < 128)
                        multiplayerAddress_ += static_cast<char>(code);
            }
            resize(viewport_);
        }
        if (screen_ == Screen::Settings)
        {
            if (key->code == sf::Keyboard::Key::Enter && applyButton_.enabled)
                return MenuAction::ApplySettings;
            if (seedFocused_ && key->code == sf::Keyboard::Key::Delete)
                draftSeedText_.clear();
            if (seedFocused_ && key->control && key->code == sf::Keyboard::Key::V)
            {
                draftSeedText_.clear();
                for (const auto code : sf::Clipboard::getString())
                {
                    if (draftSeedText_.size() == 32) break;
                    draftSeedText_ += code >= 32 && code < 127 ? static_cast<char>(code) : '?';
                }
            }
            refreshSettings();
        }
        if (key->code == sf::Keyboard::Key::R && canRestart_
            && (screen_ == Screen::Main || screen_ == Screen::Victory))
            return MenuAction::RestartMaze;
        if (key->code == sf::Keyboard::Key::Escape && screen_ != Screen::Main)
            return MenuAction::Back;
    }
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return MenuAction::None;
    const auto point = window.mapPixelToCoords(click->position);

    if (screen_ == Screen::Main)
    {
        for (const auto& button : mainButtons_)
            if (button.hit(point)) return button.action;
    }
    else
    {
        if(screen_==Screen::HowToPlay)
        {
            if(guidePrevious_.hit(point)) {--guidePage_;refreshGuide();return MenuAction::None;}
            if(guideNext_.hit(point)) {++guidePage_;refreshGuide();return MenuAction::None;}
        }
        if (backButton_.hit(point)) return MenuAction::Back;
        if (screen_ == Screen::Victory && mainButtons_[1].hit(point)) return MenuAction::NewGame;
        if (screen_ == Screen::Victory && mainButtons_[2].hit(point)) return MenuAction::RestartMaze;
        if (screen_ == Screen::Multiplayer)
        {
            if (hostButton_.hit(point)) return MenuAction::HostGame;
            if (joinButton_.hit(point)) return MenuAction::JoinGame;
        }
        if (screen_ == Screen::Settings)
        {
            seedFocused_ = seedField_.getGlobalBounds().contains(point);
            if (applyButton_.hit(point)) return MenuAction::ApplySettings;
            const int step = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
                || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift) ? 10 : 1;
            if (sizeButtons_[0].hit(point)) draftSize_.rows = std::max(MIN_MAZE_SIZE, draftSize_.rows - step);
            else if (sizeButtons_[1].hit(point)) draftSize_.rows = std::min(MAX_MAZE_SIZE, draftSize_.rows + step);
            else if (sizeButtons_[2].hit(point)) draftSize_.cols = std::max(MIN_MAZE_SIZE, draftSize_.cols - step);
            else if (sizeButtons_[3].hit(point)) draftSize_.cols = std::min(MAX_MAZE_SIZE, draftSize_.cols + step);
            refreshSettings();
        }
    }
    return MenuAction::None;
}

void Menu::update(sf::Vector2f mousePosition)
{
    if (screen_ == Screen::Main)
    {
        for (auto& button : mainButtons_) button.update(mousePosition);
    }
    else
    {
        backButton_.update(mousePosition);
        if(screen_==Screen::HowToPlay) {guidePrevious_.update(mousePosition);guideNext_.update(mousePosition);}
        if (screen_ == Screen::Multiplayer) { hostButton_.update(mousePosition); joinButton_.update(mousePosition); }
        if (screen_ == Screen::Victory)
        {
            mainButtons_[1].update(mousePosition);
            mainButtons_[2].update(mousePosition);
        }
        if (screen_ == Screen::Settings)
        {
            applyButton_.update(mousePosition);
            for (auto& button : sizeButtons_) button.update(mousePosition);
        }
    }
}

void Menu::draw(sf::RenderTarget& target) const
{
    target.draw(title_);
    target.draw(subtitle_);
    if (screen_ == Screen::Main)
    {
        for (const auto& button : mainButtons_) button.draw(target);
        target.draw(message_);
    }
    else
    {
        backButton_.draw(target);
        if (screen_ == Screen::HowToPlay) {target.draw(rules_);guidePrevious_.draw(target);guideNext_.draw(target);}
        else if (screen_ == Screen::Multiplayer)
        {
            target.draw(addressLabel_); target.draw(addressField_); target.draw(addressValue_);
            hostButton_.draw(target); joinButton_.draw(target); target.draw(message_);
        }
        else if (screen_ == Screen::Victory)
        {
            target.draw(victorySummary_);
            target.draw(victoryStatistics_);
            target.draw(victorySeed_);
            mainButtons_[1].draw(target);
            mainButtons_[2].draw(target);
            target.draw(message_);
        }
        else
        {
            target.draw(rowName_);
            target.draw(colName_);
            target.draw(rowValue_);
            target.draw(colValue_);
            target.draw(sizeSummary_);
            target.draw(seedLabel_);
            target.draw(seedField_);
            target.draw(seedValue_);
            for (const auto& button : sizeButtons_) button.draw(target);
            applyButton_.draw(target);
        }
    }
}
