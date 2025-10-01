#include "../game.h"
#include <pthread.h>
#include <atomic>

void* ballThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);

        pthread_mutex_lock(&gMutex);

        while (!gStopAll.load() && cfg->running && cfg->step != 1) {
            pthread_cond_wait(&gTickCV, &gMutex);
        }

        if (cfg->running && !cfg->paused && cfg->ballLaunched) {
            // Aplicar el multiplicador de velocidad
            cfg->ballX += cfg->ballVX * cfg->ballSpeed;
            cfg->ballY += cfg->ballVY * cfg->ballSpeed;
        }

        if (cfg->running) {
            cfg->step = 2;
            pthread_cond_broadcast(&gTickCV);
        }

        pthread_mutex_unlock(&gMutex);
    }

    return nullptr;
}