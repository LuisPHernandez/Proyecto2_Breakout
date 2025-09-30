#include <pthread.h>
#include <atomic>
#include <vector>


// Estado general del juego
struct GameConfig {
    // Área jugable
    int top, left, bottom, right;
    int x0, y0, x1, y1, w, h;

    // Paleta
    int paddleW;
    int paddleX, paddleY;
    int desiredDir;

    // Bola
    float ballX, ballY;
    float ballVX, ballVY;
    bool ballLaunched;

    // Ladrillos
    int rows, cols, gapX, gapY, brickH;
    std::vector<std::vector<Brick>> grid;

    // Estado general
    int score;
    int lives;
    bool paused;
    bool running;
    bool restartRequested;
    bool won;
    bool lost;

    // Timing
    int tick_ms;
    unsigned long frameCounter;
};


pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gTickCV = PTHREAD_COND_INITIALIZER;
std::atomic<bool> gStopAll(false);


// Función auxiliar que espera hasta que toque la siguiente frame
static unsigned long waitNextFrame(GameConfig* cfg, unsigned long lastFrame) {
    pthread_mutex_lock(&gMutex);
    while (!gStopAll.load() &&
           (cfg->paused || cfg->frameCounter == lastFrame || !cfg->running)) {
        pthread_cond_wait(&gTickCV, &gMutex);
    }
    unsigned long f = cfg->frameCounter;
    pthread_mutex_unlock(&gMutex);
    return f;
}

// LUPA
void* ballThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
}
// LUPA

// JOEL

// JOEL

// MIGUEL

// MIGUEL