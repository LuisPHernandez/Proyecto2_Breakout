#include "../game.h"
#include <pthread.h>
#include <atomic>       
#include <ncurses.h>     
#include <unistd.h>      
#include <cstddef>     
#include <chrono>

void* inputThread(void* arg) {
    auto* cfg = (GameConfig*)arg;

    // Configurar getch() en modo no bloqueante
    nodelay(stdscr, TRUE);
    unsigned long lastFrame = 0;

    using clock = std::chrono::steady_clock;
    auto lastInput = clock::now();
    
    while (!gStopAll.load()) {
        // Esperar siguiente frame
        lastFrame = waitNextFrame(cfg, lastFrame);
        usleep(10'000);
        int ch = getch();
        
        if (ch != ERR) {
            pthread_mutex_lock(&gMutex);
            
            switch (ch) {
                // MOVIMIENTO IZQUIERDA
                case KEY_LEFT:
                case 'a':
                case 'A':
                    cfg->desiredDir = -1;
                    lastInput = clock::now();
                    break;
                
                // MOVIMIENTO DERECHA
                case KEY_RIGHT:
                case 'd':
                case 'D':
                    cfg->desiredDir = 1;
                    lastInput = clock::now();
                    break;
                
                // PAUSA
                case 'p':
                case 'P':
                    cfg->paused = !cfg->paused;
                    if (!cfg->paused) {
                        // Reanudar todos los hilos que esperan
                        pthread_cond_broadcast(&gTickCV);
                    }
                    lastInput = clock::now();
                    break;
                
                // LANZAR BOLA
                case ' ':  // Espacio
                    if (!cfg->ballLaunched && cfg->running) {
                        cfg->ballLaunched = true;
                        // Velocidad inicial de la bola (para arriba)
                        cfg->ballVX = 0;
                        cfg->ballVY = -1.0f; // Hacia arriba
                    }
                    lastInput = clock::now();
                    break;
                
                // REINICIAR NIVEL
                case 'r':
                case 'R':
                    if (cfg->running) {
                        cfg->restartRequested = true;
                        pthread_cond_broadcast(&gTickCV);
                    }
                    lastInput = clock::now();
                    break;
                
                // SALIR
                case 'q':
                case 'Q':
                case 27:  // "ESC"
                    cfg->running = false;
                    gStopAll.store(true);
                    pthread_cond_broadcast(&gTickCV);
                    lastInput = clock::now();
                    break;
            }
            
            pthread_mutex_unlock(&gMutex);
        } else {
            // No hubo tecla, paramos la paleta
            auto now = clock::now();
            if (now - lastInput > std::chrono::milliseconds(80)) {
                pthread_mutex_lock(&gMutex);
                cfg->desiredDir = 0;
                pthread_mutex_unlock(&gMutex);
            }
        }
    }
    
    return nullptr;
}