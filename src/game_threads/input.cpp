#include "../game.h"
#include <pthread.h>
#include <atomic>       
#include <ncurses.h>     
#include <unistd.h>      
#include <cstddef>      

void* inputThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    
    // Configurar getch() en modo no bloqueante
    nodelay(stdscr, TRUE);
    
    while (!gStopAll.load()) {
        int ch = getch();
        
        if (ch != ERR) {
            pthread_mutex_lock(&gMutex);
            
            switch (ch) {
                // MOVIMIENTO IZQUIERDA
                case KEY_LEFT:
                case 'a':
                case 'A':
                    cfg->desiredDir = -1;
                    break;
                
                // MOVIMIENTO DERECHA
                case KEY_RIGHT:
                case 'd':
                case 'D':
                    cfg->desiredDir = 1;
                    break;
                
                // DETENER MOVIMIENTO 
                case KEY_DOWN:
                case 's':
                case 'S':
                    cfg->desiredDir = 0;
                    break;
                
                // PAUSA
                case 'p':
                case 'P':
                    cfg->paused = !cfg->paused;
                    if (!cfg->paused) {
                        // Reanudar todos los hilos que esperan
                        pthread_cond_broadcast(&gTickCV);
                    }
                    break;
                
                // LANZAR BOLA
                case ' ':  // Espacio
                    if (!cfg->ballLaunched && cfg->running) {
                        cfg->ballLaunched = true;
                        // Velocidad inicial de la bola (pa arriba)
                        cfg->ballVX = 0.5f;  // tiene un poco de  inclinación
                        cfg->ballVY = -1.0f; // Hacia arriba
                    }
                    break;
                
                // REINICIAR NIVEL
                case 'r':
                case 'R':
                    if (cfg->running) {
                        cfg->restartRequested = true;
                        pthread_cond_broadcast(&gTickCV);
                    }
                    break;
                
                // SALIR
                case 'q':
                case 'Q':
                case 27:  // con "ESC"
                    cfg->running = false;
                    gStopAll.store(true);
                    pthread_cond_broadcast(&gTickCV);
                    break;
            }
            
            pthread_mutex_unlock(&gMutex);
        }
        
        //pausa para no presionar mucho al CPU
        usleep(10000);  // 10ms
    }
    
    return nullptr;
}