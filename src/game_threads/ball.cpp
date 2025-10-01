#include "../game.h"
#include <pthread.h>     
#include <atomic>       
#include <mutex>         
#include <cstddef>

void* ballThread(void* arg) {
    GameConfig* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);

        pthread_mutex_lock(&gMutex);
        if (cfg->running && !cfg->paused && cfg->ballLaunched) {
            while (!gStopAll.load() && cfg->step != 0) {
                pthread_cond_wait(&gTickCV, &gMutex);
            }
            // Calculo de movimiento por frame
            cfg->ballX += cfg->ballVX;
            cfg->ballY += cfg->ballVY;

            cfg->step = 1; // Se marca que se movió la bola
            pthread_cond_broadcast(&gTickCV);
        }
        pthread_mutex_unlock(&gMutex);
    }
    return nullptr;
}