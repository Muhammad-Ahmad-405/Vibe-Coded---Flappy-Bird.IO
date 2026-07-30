// Flappy Bird clone using SFML
// Build instructions in README.md

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <sstream>

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
constexpr unsigned WINDOW_W = 480;
constexpr unsigned WINDOW_H = 700;

constexpr float GRAVITY = 1800.f;          // px/s^2
constexpr float FLAP_VELOCITY = -520.f;    // px/s (upward)
constexpr float MAX_FALL_SPEED = 900.f;

constexpr float BIRD_RADIUS = 16.f;
constexpr float BIRD_X = WINDOW_W * 0.3f;

constexpr float PIPE_WIDTH = 70.f;
constexpr float PIPE_GAP = 190.f;
constexpr float PIPE_SPEED = 190.f;        // px/s
constexpr float PIPE_SPACING = 260.f;      // horizontal distance between pipes
constexpr float PIPE_MIN_TOP = 80.f;
constexpr float PIPE_MAX_TOP = WINDOW_H - PIPE_GAP - 150.f;

constexpr float GROUND_HEIGHT = 80.f;

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
enum class GameState { Ready, Playing, GameOver };

struct Pipe {
    float x;
    float topHeight; // height of the top pipe (gap starts right below this)
    bool scored = false;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
float randomTopHeight() {
    float range = PIPE_MAX_TOP - PIPE_MIN_TOP;
    float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return PIPE_MIN_TOP + r * range;
}

bool circleRectIntersect(sf::Vector2f circleCenter, float radius, sf::FloatRect rect) {
    float closestX = std::max(rect.left, std::min(circleCenter.x, rect.left + rect.width));
    float closestY = std::max(rect.top, std::min(circleCenter.y, rect.top + rect.height));
    float dx = circleCenter.x - closestX;
    float dy = circleCenter.y - closestY;
    return (dx * dx + dy * dy) < (radius * radius);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H), "Flappy Bird (SFML)",
                             sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    // --- Font (tries to load a system font; falls back to built-in shapes-only if missing) ---
    sf::Font font;
    bool hasFont =
        font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf") ||
        font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf") ||
        font.loadFromFile("C:/Windows/Fonts/arial.ttf") ||
        font.loadFromFile("/System/Library/Fonts/Supplemental/Arial Bold.ttf");

    // --- Bird ---
    sf::CircleShape bird(BIRD_RADIUS);
    bird.setOrigin(BIRD_RADIUS, BIRD_RADIUS);
    bird.setFillColor(sf::Color(255, 215, 0)); // gold/yellow
    bird.setOutlineColor(sf::Color(200, 140, 0));
    bird.setOutlineThickness(2.f);

    float birdY = WINDOW_H / 2.f;
    float birdVelY = 0.f;

    // --- Pipes ---
    std::deque<Pipe> pipes;

    auto resetGame = [&]() {
        birdY = WINDOW_H / 2.f;
        birdVelY = 0.f;
        pipes.clear();
        float startX = WINDOW_W + 100.f;
        for (int i = 0; i < 4; ++i) {
            Pipe p;
            p.x = startX + i * PIPE_SPACING;
            p.topHeight = randomTopHeight();
            pipes.push_back(p);
        }
    };
    resetGame();

    GameState state = GameState::Ready;
    int score = 0;
    int bestScore = 0;

    sf::Clock clock;
    float groundScrollX = 0.f;

    // --- Text objects ---
    sf::Text scoreText, msgText, subMsgText;
    if (hasFont) {
        scoreText.setFont(font);
        scoreText.setCharacterSize(48);
        scoreText.setFillColor(sf::Color::White);
        scoreText.setOutlineColor(sf::Color::Black);
        scoreText.setOutlineThickness(3.f);

        msgText.setFont(font);
        msgText.setCharacterSize(32);
        msgText.setFillColor(sf::Color::White);
        msgText.setOutlineColor(sf::Color::Black);
        msgText.setOutlineThickness(3.f);
        msgText.setStyle(sf::Text::Bold);

        subMsgText.setFont(font);
        subMsgText.setCharacterSize(18);
        subMsgText.setFillColor(sf::Color(230, 230, 230));
        subMsgText.setOutlineColor(sf::Color::Black);
        subMsgText.setOutlineThickness(2.f);
    }

    auto centerTextX = [&](sf::Text& t) {
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.left + b.width / 2.f, b.top);
    };

    auto flap = [&]() {
        if (state == GameState::Ready) state = GameState::Playing;
        if (state == GameState::Playing) birdVelY = FLAP_VELOCITY;
        if (state == GameState::GameOver) {
            resetGame();
            score = 0;
            state = GameState::Ready;
        }
    };

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f; // clamp huge frame spikes (e.g., window drag)

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) window.close();
                if (event.key.code == sf::Keyboard::Space || event.key.code == sf::Keyboard::Up)
                    flap();
            }
            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) flap();
            }
        }

        // ---------------- Update ----------------
        groundScrollX -= PIPE_SPEED * dt;
        if (groundScrollX <= -40.f) groundScrollX += 40.f;

        if (state == GameState::Playing) {
            birdVelY += GRAVITY * dt;
            if (birdVelY > MAX_FALL_SPEED) birdVelY = MAX_FALL_SPEED;
            birdY += birdVelY * dt;

            for (auto& p : pipes) {
                p.x -= PIPE_SPEED * dt;
            }
            // Recycle pipes that scrolled off-screen
            if (!pipes.empty() && pipes.front().x < -PIPE_WIDTH) {
                pipes.pop_front();
                float maxX = pipes.back().x;
                Pipe p;
                p.x = maxX + PIPE_SPACING;
                p.topHeight = randomTopHeight();
                pipes.push_back(p);
            }

            // Scoring: bird passed pipe's right edge
            for (auto& p : pipes) {
                if (!p.scored && p.x + PIPE_WIDTH < BIRD_X) {
                    p.scored = true;
                    score++;
                }
            }

            // Collisions: ground / ceiling
            if (birdY + BIRD_RADIUS >= WINDOW_H - GROUND_HEIGHT || birdY - BIRD_RADIUS <= 0) {
                state = GameState::GameOver;
                bestScore = std::max(bestScore, score);
            }

            // Collisions: pipes
            sf::Vector2f birdCenter(BIRD_X, birdY);
            for (auto& p : pipes) {
                sf::FloatRect topRect(p.x, 0.f, PIPE_WIDTH, p.topHeight);
                sf::FloatRect botRect(p.x, p.topHeight + PIPE_GAP, PIPE_WIDTH,
                                      WINDOW_H - (p.topHeight + PIPE_GAP));
                if (circleRectIntersect(birdCenter, BIRD_RADIUS, topRect) ||
                    circleRectIntersect(birdCenter, BIRD_RADIUS, botRect)) {
                    state = GameState::GameOver;
                    bestScore = std::max(bestScore, score);
                }
            }
        }

        // ---------------- Draw ----------------
        window.clear(sf::Color(78, 192, 202)); // sky blue

        // Pipes
        for (auto& p : pipes) {
            sf::RectangleShape topPipe(sf::Vector2f(PIPE_WIDTH, p.topHeight));
            topPipe.setPosition(p.x, 0.f);
            topPipe.setFillColor(sf::Color(76, 175, 80));
            topPipe.setOutlineColor(sf::Color(46, 125, 50));
            topPipe.setOutlineThickness(3.f);
            window.draw(topPipe);

            float botY = p.topHeight + PIPE_GAP;
            sf::RectangleShape botPipe(sf::Vector2f(PIPE_WIDTH, WINDOW_H - botY));
            botPipe.setPosition(p.x, botY);
            botPipe.setFillColor(sf::Color(76, 175, 80));
            botPipe.setOutlineColor(sf::Color(46, 125, 50));
            botPipe.setOutlineThickness(3.f);
            window.draw(botPipe);
        }

        // Ground
        sf::RectangleShape ground(sf::Vector2f(WINDOW_W + 40.f, GROUND_HEIGHT));
        ground.setPosition(groundScrollX, WINDOW_H - GROUND_HEIGHT);
        ground.setFillColor(sf::Color(222, 184, 135));
        window.draw(ground);
        sf::RectangleShape groundStripe(sf::Vector2f(WINDOW_W + 40.f, 6.f));
        groundStripe.setPosition(groundScrollX, WINDOW_H - GROUND_HEIGHT);
        groundStripe.setFillColor(sf::Color(139, 105, 20));
        window.draw(groundStripe);

        // Bird (rotate slightly based on velocity for a bit of juice)
        bird.setPosition(BIRD_X, birdY);
        float rotation = std::max(-25.f, std::min(70.f, birdVelY * 0.06f));
        bird.setRotation(rotation);
        window.draw(bird);
        // simple eye
        sf::CircleShape eye(3.f);
        eye.setFillColor(sf::Color::Black);
        eye.setPosition(BIRD_X + 6.f, birdY - 6.f);
        window.draw(eye);

        // UI text
        if (hasFont) {
            std::ostringstream ss;
            ss << score;
            scoreText.setString(ss.str());
            centerTextX(scoreText);
            scoreText.setPosition(WINDOW_W / 2.f, 30.f);
            window.draw(scoreText);

            if (state == GameState::Ready) {
                msgText.setString("Flappy Bird");
                centerTextX(msgText);
                msgText.setPosition(WINDOW_W / 2.f, WINDOW_H / 2.f - 80.f);
                window.draw(msgText);

                subMsgText.setString("Click or press Space to start");
                centerTextX(subMsgText);
                subMsgText.setPosition(WINDOW_W / 2.f, WINDOW_H / 2.f - 30.f);
                window.draw(subMsgText);
            } else if (state == GameState::GameOver) {
                msgText.setString("Game Over");
                centerTextX(msgText);
                msgText.setPosition(WINDOW_W / 2.f, WINDOW_H / 2.f - 80.f);
                window.draw(msgText);

                std::ostringstream sub;
                sub << "Score: " << score << "   Best: " << bestScore
                    << "\nClick or press Space to retry";
                subMsgText.setString(sub.str());
                centerTextX(subMsgText);
                subMsgText.setPosition(WINDOW_W / 2.f, WINDOW_H / 2.f - 20.f);
                window.draw(subMsgText);
            }
        }

        window.display();
    }

    return 0;
}
