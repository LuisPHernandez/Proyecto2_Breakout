#include "game.h"

void* ballThread(void* arg) {
    GameConfig* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);

        pthread_mutex_lock(&gMutex);
        if (cfg->running && !cfg->paused && cfg->ballLaunched) {
            // Integración “por frame” (puedes subir 2x si quieres más suave)
            cfg->ballX += cfg->ballVX;
            cfg->ballY += cfg->ballVY;
        }
        pthread_mutex_unlock(&gMutex);

        // Fin de fase de actualización: sincroniza con el resto
        pthread_barrier_wait(&gFrameBarrier);
    }
    return nullptr;
}