/*
game.h - Archivo "header" que sirve para declarar funciones, clases, estructuras y constantes que 
luego serán implementadas en archivos .cpp; de esta forma permiten separar la interfaz de la implementación, 
reutilizar código en varios módulos y mantener el programa organizado y más fácil de mantener.
*/ 
#ifndef GAME_H
#define GAME_H

#include <vector>
#include <pthread.h>
#include <atomic>
#include <ncurses.h>
#include <string>

// Estructura de un ladrillo
struct Brick {
    int hp;      // Puntos de vida
    char ch;     // Carácter visual
    int points;  // Puntos que otorga
};

// Estado general del juego
struct GameConfig {
    // Área jugable
    WINDOW* winPlay = nullptr;   // ventana del área jugable
    int top, left, bottom, right;
    int x0, y0, x1, y1, w, h;

    // Paleta
    int paddleW;
    int paddleX, paddleY;
    int desiredDir;

    // Paleta 2 
    int paddle2W;
    int paddle2X, paddle2Y;
    int desiredDir2;

    // Bola
    float ballX, ballY;
    float ballVX, ballVY;
    float ballSpeed;      // Multiplicador de velocidad
    bool ballLaunched;
    bool ballJustReset;

    // Ladrillos
    int rows, cols, gapX, gapY, brickH;
    std::vector<std::vector<Brick>> grid;
    std::vector<std::string> brickBuffer; // Buffer "pre-renderizado" de ladrillos
    bool brickBufferReady = false;  // Indica que brickBuffer ya está construido

    // Estado general
    int score;
    int lives;
    bool paused;
    bool running;
    bool restartRequested;
    bool won;
    bool lost;
    bool gridDirty;
    bool frameDrawn;
    int level;
    bool twoPlayers;

    // Timing
    int tick_ms;
    int step;
    unsigned long frameCounter;
};

// Variables globales compartidas
extern pthread_mutex_t gMutex;
extern pthread_cond_t gTickCV;
extern pthread_cond_t gCtrlCV;
extern std::atomic<bool> gStopAll;

// Declaraciones de hilos
void* tickThread(void* arg); // Coordinador de frames
void* inputThread(void* arg); // Teclado
void* paddleThread(void* arg); // Paleta
void* ballThread(void* arg); // Pelota
void* collisionsWallsPaddleThread(void* arg); // Colisiones con paredes y paleta
void* collisionsBricksThread(void* arg); // Colisiones con ladrillos
void* renderThread(void* arg); // Dibujo
void* stateThread(void* arg); // Estado del juego
void* speedThread(void* arg); // Control de velocidad

// Función auxiliar
unsigned long waitNextFrame(GameConfig* cfg, unsigned long lastFrame);

// Función principal del juego
void runGameplay(bool twoPlayers = false);

// Función para obtener el score final del juego
int getGameScore();

#endif // GAME_H