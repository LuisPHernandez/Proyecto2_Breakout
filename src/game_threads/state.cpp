#include "../game.h"
#include <pthread.h>
#include <atomic>

void* stateThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        pthread_mutex_lock(&gMutex);

        while (!gStopAll.load() && cfg->running && cfg->step != 4) {
            pthread_cond_wait(&gTickCV, &gMutex);
        }

        if (cfg->running) {
            // Verificar victoria
            bool anyAlive = false;
            for (auto &row : cfg->grid) {
                for (auto &b : row) { 
                    if (b.hp > 0) { 
                        anyAlive = true; 
                        break; 
                    } 
                }
                if (anyAlive) break;
            }
            
            if (!anyAlive) { 
                cfg->won = true; 
                cfg->running = false;
                // IMPORTANTE: Notificar al hilo de control
                pthread_cond_signal(&gCtrlCV);
            }
            
            // Si se perdió (detectado en collisionsWallsPaddle)
            if (cfg->lost) {
                pthread_cond_signal(&gCtrlCV);
            }
        }

        if (cfg->running) {
            cfg->step = 0; // Completar el ciclo
            pthread_cond_broadcast(&gTickCV);
        }

        pthread_mutex_unlock(&gMutex);
    }
    return nullptr;
}