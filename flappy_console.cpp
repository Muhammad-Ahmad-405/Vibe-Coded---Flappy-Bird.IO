
#include <iostream>
#include <sstream>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <random>
#include <string>

// ---------------------------------------------------------------------------
// Platform-specific non-blocking keyboard input
// ---------------------------------------------------------------------------
#if defined(_WIN32)
    #include <conio.h>
    bool keyHit() { return _kbhit(); }
    char readKey() { return _getch(); }
    void initTerminal() {}
    void restoreTerminal() {}
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    struct termios origTermios;
    void restoreTerminal() {
        tcsetattr(STDIN_FILENO, TCSANOW, &origTermios);
        fcntl(STDIN_FILENO, F_SETFL, 0); // restore blocking mode
    }
    void initTerminal() {
        tcgetattr(STDIN_FILENO, &origTermios);
        termios raw = origTermios;
        raw.c_lflag &= ~(ICANON | ECHO); // no line buffering, no echo
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // non-blocking reads
    }
    bool keyHit() {
        char c;
        int n = read(STDIN_FILENO, &c, 1);
        if (n == 1) { ungetc(c, stdin); return true; }
        return false;
    }
    char readKey() {
        char c = 0;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        (void)n;
        return c;
    }
#endif

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
constexpr int WIDTH = 40;   // playfield width (characters)
constexpr int HEIGHT = 20;  // playfield height (characters)
constexpr int BIRD_X = 8;

constexpr double GRAVITY = 22.0;      // rows/s^2
constexpr double FLAP_VELOCITY = -7.5; // rows/s (negative = up)
constexpr double MAX_FALL_SPEED = 18.0;

constexpr int PIPE_GAP = 6;
constexpr int PIPE_SPACING = 12; // columns between pipe pairs
constexpr double TICK_SECONDS = 1.0 / 20.0; // 20 updates/sec (pipes move 1 col/tick)

// ---------------------------------------------------------------------------
struct Pipe {
    int x;
    int gapTop; // row index where the gap starts
    bool scored = false;
};

std::string clearScreenCode() {
#if defined(_WIN32)
    return ""; // handled via system("cls") for reliability on Windows
#else
    return "\x1B[2J\x1B[H"; // ANSI clear + cursor home
#endif
}

int main() {
    initTerminal();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> gapDist(2, HEIGHT - PIPE_GAP - 3);

    double birdY = HEIGHT / 2.0;
    double birdVel = 0.0;
    std::deque<Pipe> pipes;
    int score = 0, best = 0;
    bool alive = true;
    bool started = false;

    auto resetGame = [&]() {
        birdY = HEIGHT / 2.0;
        birdVel = 0.0;
        pipes.clear();
        int startX = WIDTH + 4;
        for (int i = 0; i < 3; ++i) {
            pipes.push_back({startX + i * PIPE_SPACING, gapDist(rng), false});
        }
        score = 0;
        alive = true;
        started = false;
    };
    resetGame();

    bool quit = false;
    while (!quit) {
        // ---- Input ----
        while (keyHit()) {
            char c = readKey();
            if (c == 'q' || c == 'Q') { quit = true; break; }
            if (c == ' ') {
                if (!alive) { resetGame(); }
                else {
                    started = true;
                    birdVel = FLAP_VELOCITY;
                }
            }
        }
        if (quit) break;

        // ---- Update ----
        if (started && alive) {
            birdVel += GRAVITY * TICK_SECONDS;
            if (birdVel > MAX_FALL_SPEED) birdVel = MAX_FALL_SPEED;
            birdY += birdVel * TICK_SECONDS;

            for (auto& p : pipes) p.x -= 1;

            if (!pipes.empty() && pipes.front().x < 0) {
                pipes.pop_front();
                int maxX = pipes.back().x;
                pipes.push_back({maxX + PIPE_SPACING, gapDist(rng), false});
            }

            for (auto& p : pipes) {
                if (!p.scored && p.x < BIRD_X) {
                    p.scored = true;
                    score++;
                }
            }

            // Collisions: ceiling / floor
            int birdRow = static_cast<int>(birdY + 0.5);
            if (birdRow < 0 || birdRow >= HEIGHT) {
                alive = false;
                best = std::max(best, score);
            }

            // Collisions: pipes (bird occupies column BIRD_X)
            for (auto& p : pipes) {
                if (p.x == BIRD_X) {
                    if (birdRow < p.gapTop || birdRow >= p.gapTop + PIPE_GAP) {
                        alive = false;
                        best = std::max(best, score);
                    }
                }
            }
        }

        // ---- Render ----
        std::vector<std::string> grid(HEIGHT, std::string(WIDTH, ' '));

        for (auto& p : pipes) {
            if (p.x < 0 || p.x >= WIDTH) continue;
            for (int row = 0; row < HEIGHT; ++row) {
                bool inGap = (row >= p.gapTop && row < p.gapTop + PIPE_GAP);
                if (!inGap) grid[row][p.x] = '|';
            }
        }

        int birdRow = static_cast<int>(birdY + 0.5);
        if (birdRow >= 0 && birdRow < HEIGHT) {
            char birdChar = !alive ? 'X' : (birdVel < -1.0 ? '^' : (birdVel > 6.0 ? 'v' : 'O'));
            grid[birdRow][BIRD_X] = birdChar;
        }

        std::ostringstream out;
#if defined(_WIN32)
        std::system("cls");
#else
        out << clearScreenCode();
#endif
        out << "FLAPPY BIRD (console)   Score: " << score << "   Best: " << best << "\n";
        out << std::string(WIDTH, '=') << "\n";
        for (auto& row : grid) out << row << "\n";
        out << std::string(WIDTH, '=') << "\n";

        if (!started) {
            out << "Press SPACE to start flapping. 'q' to quit.\n";
        } else if (!alive) {
            out << "GAME OVER — Score: " << score << "  Best: " << best << "\n";
            out << "Press SPACE to play again, or 'q' to quit.\n";
        } else {
            out << "SPACE = flap   q = quit\n";
        }

        std::cout << out.str() << std::flush;

        std::this_thread::sleep_for(std::chrono::duration<double>(TICK_SECONDS));
    }

    restoreTerminal();
    std::cout << "\nThanks for playing! Final best score: " << best << "\n";
    return 0;
}
