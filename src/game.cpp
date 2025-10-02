#include "game.h"
#include <ncurses.h>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <cstring>

/*
DEFINICIONES DE LAS VARIABLES Y FUNCIONES GLOBALES DECLARADAS EN game.h
*/

pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gTickCV = PTHREAD_COND_INITIALIZER;
pthread_cond_t gCtrlCV = PTHREAD_COND_INITIALIZER;
std::atomic<bool> gStopAll(false);

// Variable global para guardar el score final
static int g_finalScore = 0;

// Permite a los hilos esperar al siguiente frame para sincronizarse
unsigned long waitNextFrame(GameConfig* cfg, unsigned long lastFrame) {
    pthread_mutex_lock(&gMutex);
    while (!gStopAll.load() &&
           (cfg->frameCounter == lastFrame || !cfg->running)) {
        pthread_cond_wait(&gTickCV, &gMutex);
    }
    unsigned long f = cfg->frameCounter;
    pthread_mutex_unlock(&gMutex);
    return f;
}

/*
HELPERS LOCALES DE ESTE MÓDULO
*/

// Construye el nivel 1 del juego
static void buildLevel1(GameConfig& cfg) {
    cfg.grid.assign(cfg.rows, std::vector<Brick>(cfg.cols));

    for (int r = 0; r < cfg.rows; ++r) {
        for (int c = 0; c < cfg.cols; ++c) {
            Brick b{};
            b.hp = 1; b.ch = '#'; b.points = 10;
            cfg.grid[r][c] = b;
        }
    }
}

// Construye el nivel 2 del juego
static void buildLevel2(GameConfig& cfg) {
    cfg.grid.assign(cfg.rows, std::vector<Brick>(cfg.cols));

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

// Construye el nivel 3 del juego
static void buildLevel3(GameConfig& cfg) {
    cfg.grid.assign(cfg.rows, std::vector<Brick>(cfg.cols));

    for (int r = 0; r < cfg.rows; ++r) {
        for (int c = 0; c < cfg.cols; ++c) {
            Brick b{};
            b.hp = 3; b.ch = '@'; b.points = 50;
            cfg.grid[r][c] = b;
        }
    }
}

// Calcula la geometría del área de juego
static void computePlayArea(GameConfig& cfg) {
    int rows, cols; 
    getmaxyx(stdscr, rows, cols);

    cfg.top    = rows/2 - 12;
    cfg.bottom = rows/2 + 12;
    cfg.left   = cols/2 - 40;
    cfg.right  = cols/2 + 40;

    cfg.x0 = cfg.left + 1;  
    cfg.y0 = cfg.top + 1;
    cfg.x1 = cfg.right - 1; 
    cfg.y1 = cfg.bottom - 1;
    cfg.w  = cfg.x1 - cfg.x0 + 1;
    cfg.h  = cfg.y1 - cfg.y0 + 1;

    cfg.paddleY = cfg.y1 - 1;
}

// Permite reiniciar el nivel
static void resetLevel(GameConfig& cfg) {
    cfg.score = 0;
    cfg.lives = 3;
    cfg.paused = false;
    cfg.running = true;
    cfg.restartRequested = false;
    cfg.won = false;
    cfg.lost = false;

    // Jugador 1
    cfg.paddleW = 9;
    cfg.paddleY = cfg.y1 - 2;                         // fila fija cerca del borde inferior
    cfg.paddleX = cfg.x0 + (cfg.w * 1) / 4;           // a la izquierda (puedes dejar centro si prefieres)
    cfg.desiredDir = 0;

// Jugador 2 (solo si coop)
    if (cfg.twoPlayers) {
        cfg.paddle2W = cfg.paddleW;
        cfg.paddle2Y = cfg.paddleY;                   // misma fila que P1
        cfg.paddle2X = cfg.x0 + (cfg.w * 3) / 4;      // a la derecha
        cfg.desiredDir2 = 0;
    } else {
        cfg.paddle2W = 0;                             // seguridad: no se dibuja nada
        cfg.paddle2X = cfg.paddle2Y = 0;
        cfg.desiredDir2 = 0;
    }

    cfg.ballLaunched = false;
    cfg.ballJustReset = true;
    cfg.ballSpeed = 1.0f;  // Velocidad inicial normal
    cfg.ballVX = 0.0f; 
    cfg.ballVY = 0.0f;
    cfg.ballX = cfg.paddleX + cfg.paddleW / 2.0f;
    cfg.ballY = cfg.paddleY - 1.0f;
    cfg.gridDirty = true;
    cfg.frameDrawn = false;
    cfg.brickBufferReady = false;

    if (cfg.level == 1) {
        buildLevel1(cfg);
    } else if (cfg.level == 2) {
        buildLevel2(cfg);
    } else {
        buildLevel3(cfg);
    }
    cfg.frameCounter = 0;
    cfg.step = 0;
}

// Muestra pantalla de fin de juego
static void showEndScreenBlocking(bool won) {
    clear();
    int rows, cols; 
    getmaxyx(stdscr, rows, cols);
    const char* msg1 = won ? "¡GANASTE!" : "PERDISTE";
    const char* msg2 = "Presiona ENTER para continuar";
    mvprintw(rows/2 - 1, (cols - (int)strlen(msg1))/2, "%s", msg1);
    mvprintw(rows/2 + 1, (cols - (int)strlen(msg2))/2, "%s", msg2);
    refresh();

    int ch;
    nodelay(stdscr, FALSE);
    keypad(stdscr, TRUE);
    while ((ch = getch()) != '\n' && ch != KEY_ENTER) { }
    nodelay(stdscr, TRUE);
}

/*
FUNCIÓN PRINCIPAL Y PUNTO DE ENTRADA DESDE EL MENÚ
*/

void runGameplay(bool twoPlayers) {
    std::srand((unsigned)std::time(nullptr));

    // 1) Config inicial
    GameConfig cfg{};
    cfg.tick_ms = 60000;
    cfg.twoPlayers = twoPlayers;
    cfg.rows = 4;
    cfg.cols = 10;
    cfg.gapX = 1;
    cfg.gapY = 1;
    cfg.brickH = 1;
    cfg.desiredDir = 0;
    cfg.level = 1;

    computePlayArea(cfg);
    resetLevel(cfg);
    
    if (!cfg.winPlay) {
        int playH = cfg.h;
        int playW = cfg.w;
        int playY = cfg.y0;
        int playX = cfg.x0;
        cfg.winPlay = newwin(playH, playW, playY, playX);
    }   

    // 2) Lanzar hilos
    pthread_t tTick, tInput, tPaddle, tBall, tCollisionsWP, tCollisionsB, tRender, tState, tSpeed;
    gStopAll.store(false);

    pthread_create(&tTick, nullptr, tickThread, &cfg);
    pthread_create(&tInput, nullptr, inputThread, &cfg);
    pthread_create(&tPaddle, nullptr, paddleThread, &cfg);
    pthread_create(&tBall, nullptr, ballThread, &cfg);
    pthread_create(&tCollisionsWP, nullptr, collisionsWallsPaddleThread, &cfg);
    pthread_create(&tCollisionsB, nullptr, collisionsBricksThread, &cfg);
    pthread_create(&tRender, nullptr, renderThread, &cfg);
    pthread_create(&tState, nullptr, stateThread, &cfg);
    pthread_create(&tSpeed, nullptr, speedThread, &cfg);

    // 3) Bucle de control
    pthread_mutex_lock(&gMutex);
    while (true) {
        // Espera a que algo relevante ocurra
        while (!cfg.restartRequested && cfg.running) {
            pthread_cond_wait(&gCtrlCV, &gMutex);
        }

        if (cfg.restartRequested) {
            resetLevel(cfg);
            cfg.restartRequested = false;
            pthread_cond_broadcast(&gTickCV);
            continue;
        }

        // Si no es restart, es porque terminó (won/lost)
        break;
    }
    pthread_mutex_unlock(&gMutex);

    // 4) Parar hilos y limpiar
    gStopAll.store(true);
    pthread_mutex_lock(&gMutex);
    pthread_cond_broadcast(&gTickCV);
    pthread_mutex_unlock(&gMutex);

    pthread_join(tTick, nullptr);
    pthread_join(tInput, nullptr);
    pthread_join(tPaddle, nullptr);
    pthread_join(tBall, nullptr);
    pthread_join(tCollisionsWP, nullptr);
    pthread_join(tCollisionsB, nullptr);
    pthread_join(tRender, nullptr);
    pthread_join(tState, nullptr);
    pthread_join(tSpeed, nullptr);

    bool won, lost;
    pthread_mutex_lock(&gMutex);
    won = cfg.won;
    lost = cfg.lost;
    g_finalScore = cfg.score; // Guardar score final
    pthread_mutex_unlock(&gMutex);

    if (won || lost) {
        showEndScreenBlocking(won);
    }
    
    if (cfg.winPlay) { 
        delwin(cfg.winPlay); 
        cfg.winPlay = nullptr; 
    }
}

// Función para obtener el score final del último juego
int getGameScore() {
    return g_finalScore;
}