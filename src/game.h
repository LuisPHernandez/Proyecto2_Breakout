/*

game.h - Estructuras y declaraciones compartidas, la verdad no sabia quer era un .h pero es
un archivo de cabecera (header file) que contiene declaraciones de estruturas, variables y funciones
que son usadas en varios archivos .cpp del proyecto. Sirve para compartir definiciones comunes
entre diferentes módulos del programa, facilitando la organización y reutilización del código.

*/ 
#ifndef GAME_H
#define GAME_H

#include <vector>
#include <pthread.h>
#include <atomic>

// Estructura de un ladrillo
struct Brick {
    int hp;      // Puntos de vida (0 = destruido)
    char ch;     // Carácter visual ('#', '%', '@', etc.)
    int points;  // Puntos que otorga al destruirse
};

// Estado general del juego
struct GameConfig {
    // Área jugable
    int top, left, bottom, right;
    int x0, y0, x1, y1, w, h;

    // Paleta
    int paddleW;
    int paddleX, paddleY;
    int desiredDir;  // -1=izq, 0=quieto, 1=der

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

// Variables globales compartidas
extern pthread_mutex_t gMutex;
extern pthread_cond_t gTickCV;
extern std::atomic<bool> gStopAll;

// Declaraciones de hilos
void* inputThread(void* arg);      // joel
void* paddleThread(void* arg);     // joel
void* ballThread(void* arg);       // LUPA
void* renderThread(void* arg);     // Miguelito
void* tickThread(void* arg);       // Coordinador

// Función auxiliar definida en paddle.cpp
unsigned long waitNextFrame(GameConfig* cfg, unsigned long lastFrame);

#endif // GAME_H