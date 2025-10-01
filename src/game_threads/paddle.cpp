#include "../game.h"
#include <pthread.h>
#include <atomic>
#include <unistd.h>

void* paddleThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        if (gStopAll.load()) break;

        pthread_mutex_lock(&gMutex);

        // Espera a step 0
        while (!gStopAll.load() && cfg->running && cfg->step != 0) {
            pthread_cond_wait(&gTickCV, &gMutex);
        }

        if (cfg->running) {
            // Mover paleta si no pausado
            if (!cfg->paused) {
                const int PADDLE_SPEED = 2;
                int newX = cfg->paddleX + cfg->desiredDir * PADDLE_SPEED;
                int minX = cfg->x0;
                int maxX = cfg->x1 - cfg->paddleW + 1;
                if (newX < minX) newX = minX;
                if (newX > maxX) newX = maxX;
                cfg->paddleX = newX;

                if (!cfg->ballLaunched && cfg->ballJustReset) {
                    cfg->ballX = cfg->paddleX + (cfg->paddleW / 2.0f);
                    cfg->ballY = cfg->paddleY - 1.0f;
                }
            }

            cfg->step = 1; // Siguiente fase
            pthread_cond_broadcast(&gTickCV);
        }

        pthread_mutex_unlock(&gMutex);
    }

    return nullptr;
}
