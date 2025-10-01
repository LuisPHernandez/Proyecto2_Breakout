#include "game.h"
#include <ncurses.h>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

/*
DEFINICIONES DE LAS VARIABLES Y FUNCIONES GLOBALES DECLARADAS EN game.h
*/

pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gTickCV = PTHREAD_COND_INITIALIZER;
pthread_barrier_t gFrameBarrier;
std::atomic<bool> gStopAll(false);

// Permite a los hilos esperar al siguiente frame para sincronizarse
unsigned long waitNextFrame(GameConfig* cfg, unsigned long lastFrame) {
    pthread_mutex_lock(&gMutex);
    while (!gStopAll.load() &&
           (cfg->paused || cfg->frameCounter == lastFrame || !cfg->running)) {
        pthread_cond_wait(&gTickCV, &gMutex);
    }
    unsigned long f = cfg->frameCounter;
    pthread_mutex_unlock(&gMutex);
    return f;
}

/*
HELPERS LOCALES DE ESTE MÓDULO
*/

// Construye el nivel default del juego
static void buildDefaultLevel(GameConfig& cfg) {
    cfg.grid.assign(cfg.rows, std::vector<Brick>(cfg.cols)); // Inicializa objetos de Brick para todas las filas/columnas de ladrillos

    // Asigna un tipo de ladrillo específico a cada posición
    for (int r = 0; r < cfg.rows; ++r) {
        for (int c = 0; c < cfg.cols; ++c) {
            Brick b{};
            if (r == 0) {
                b.hp = 3; b.ch = '@'; b.points = 50;
            }
            else if (r <= 2) {
                b.hp = 2; b.ch = '%'; b.points = 30;
            }
            else {
                b.hp = 1; b.ch = '#'; b.points = 10;
            }
            cfg.grid[r][c] = b;
        }
    }
}


// Calcula la geometría del área de juego
static void computePlayArea(GameConfig& cfg) {
    int rows, cols; getmaxyx(stdscr, rows, cols); // Se obtiene el tamaño de la terminal en filas y columnas

    // Se calcula un rectangulo centrado para dibujar el marco de la aplicación
    cfg.top    = rows/2 - 12;
    cfg.bottom = rows/2 + 12;
    cfg.left   = cols/2 - 40;
    cfg.right  = cols/2 + 40;

    // Se calculan las coordenadas y medidas internas del marco
    cfg.x0 = cfg.left + 1;  cfg.y0 = cfg.top + 1;
    cfg.x1 = cfg.right - 1; cfg.y1 = cfg.bottom - 1;
    cfg.w  = cfg.x1 - cfg.x0 + 1;
    cfg.h  = cfg.y1 - cfg.y0 + 1;

    // Se pone la paleta una fila arriba del borde inferior
    cfg.paddleY = cfg.y1 - 1; 
}

// Permite reiniciar el nivel
static void resetLevel(GameConfig& cfg) {
    // Reinicia config a valores por defecto
    cfg.score = 0;
    cfg.lives = 3;
    cfg.paused = false;
    cfg.running = true;
    cfg.restartRequested = false;
    cfg.won = false;
    cfg.lost = false;

    cfg.paddleW = 9;
    cfg.paddleX = (cfg.x0 + cfg.x1) / 2;

    cfg.ballLaunched = false;
    cfg.ballVX = 0.0f; cfg.ballVY = 0.0f;
    cfg.ballX = cfg.paddleX;
    cfg.ballY = cfg.paddleY - 1;

    buildDefaultLevel(cfg);
    cfg.frameCounter = 0;
}

/*
FUNCIÓN PRINCIPAL Y PUNTO DE ENTRADA DESDE EL MENÚ
*/

void runGameplay() {
    std::srand((unsigned)std::time(nullptr));

    // 1) Config inicial
    GameConfig cfg{};
    cfg.tick_ms = 48; // ~20 FPS
    cfg.rows = 5;
    cfg.cols = 14;
    cfg.gapX = 1;
    cfg.gapY = 1;
    cfg.brickH = 1;
    cfg.desiredDir = 0;

    computePlayArea(cfg);
    resetLevel(cfg);

    // 2) Lanzar hilos
    pthread_t tTick, tInput, tPaddle, tBall, tCollisions, tRender;
    gStopAll.store(false);

    pthread_create(&tTick, nullptr, tickThread, &cfg);             // Coordinador de frames
    pthread_create(&tInput, nullptr, inputThread, &cfg);           // Teclado
    pthread_create(&tPaddle, nullptr, paddleThread, &cfg);         // Paleta
    pthread_create(&tBall, nullptr, ballThread, &cfg);             // Pelota
    pthread_create(&tCollisions, nullptr, collisionsThread, &cfg); // Física de colisiones
    pthread_create(&tRender, nullptr, renderThread, &cfg);         // Dibujo

    // 3) Bucle de control
    bool done = false;
    while (!done) {
        usleep(10'000); // Obliga al bucle a dormir un poco para no saturar el CPU

        pthread_mutex_lock(&gMutex);
        bool restart = cfg.restartRequested;
        bool running = cfg.running;
        bool won = cfg.won;
        bool lost = cfg.lost;
        pthread_mutex_unlock(&gMutex);

        if (restart) {
            pthread_mutex_lock(&gMutex);
            resetLevel(cfg);
            pthread_mutex_unlock(&gMutex);
            continue;
        }

        // Si el nivel terminó o el usuario decidió salir
        if (!running && (won || lost || !restart)) {
            done = true;
        }
    }

    // 4) Parar hilos y limpiar
    gStopAll.store(true);
    pthread_mutex_lock(&gMutex);
    pthread_cond_broadcast(&gTickCV); // Despertar a los hilos esperando frame
    pthread_mutex_unlock(&gMutex);

    pthread_join(tTick, nullptr);
    pthread_join(tInput, nullptr);
    pthread_join(tPaddle, nullptr);
    pthread_join(tBall, nullptr);
    pthread_join(tCollisions, nullptr);
    pthread_join(tRender, nullptr);
}