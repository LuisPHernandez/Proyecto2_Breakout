#include "../game.h"
#include <pthread.h>
#include <atomic>
#include <cmath>

static void getAngle(float& vx, float& vy) {
    const float MIN_X = 0.2f, MIN_Y = 0.4f;
    if (std::fabs(vx) < MIN_X) vx = (vx >= 0 ? MIN_X : -MIN_X);
    if (std::fabs(vy) < MIN_Y) vy = (vy >= 0 ? MIN_Y : -MIN_Y);
}

void* collisionsWallsPaddleThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        pthread_mutex_lock(&gMutex);

        while (!gStopAll.load() && cfg->running && cfg->step != 2) {
            pthread_cond_wait(&gTickCV, &gMutex);
        }

        if (cfg->running && !cfg->paused && cfg->ballLaunched) {
            // Paredes laterales
            if (cfg->ballX <= cfg->x0 + 1) { cfg->ballX = cfg->x0 + 2; cfg->ballVX = -cfg->ballVX; getAngle(cfg->ballVX, cfg->ballVY); }
            if (cfg->ballX >= cfg->x1 - 1) { cfg->ballX = cfg->x1 - 2; cfg->ballVX = -cfg->ballVX; getAngle(cfg->ballVX, cfg->ballVY); }

            // Techo
            if (cfg->ballY <= cfg->y0 + 1) { cfg->ballY = cfg->y0 + 2; cfg->ballVY = -cfg->ballVY; getAngle(cfg->ballVX, cfg->ballVY); }

            // Piso
            if (cfg->ballY > cfg->y1 - 1) {
                cfg->lives--;
                cfg->ballLaunched = false;
                cfg->ballJustReset = true;
                cfg->ballVX = 0.0f; cfg->ballVY = 0.0f;
                cfg->ballX = cfg->paddleX + cfg->paddleW / 2.0f;
                cfg->ballY = cfg->paddleY - 1.0f;
                if (cfg->lives <= 0) { cfg->lost = true; cfg->running = false; }
            }

            // Paleta
            int py = cfg->paddleY;
            if ((int)std::round(cfg->ballY) == py - 1) {
                int bx = (int)std::round(cfg->ballX);
                if (bx >= cfg->paddleX && bx <= (cfg->paddleX + (cfg->paddleW - 1))) {
                    cfg->ballY = py - 2;
                    cfg->ballVY = -std::fabs(cfg->ballVY > 0 ? cfg->ballVY : 1.0f);
                    float center = cfg->paddleX + cfg->paddleW / 2.0f;
                    float half   = std::max(1.0f, cfg->paddleW / 2.0f);
                    float rel    = (cfg->ballX - center) / half; // [-1..+1]
                    cfg->ballVX  = rel * 1.2f;
                    getAngle(cfg->ballVX, cfg->ballVY);
                }
            }
        }

        if (cfg->running) {
            cfg->step = 3;
            pthread_cond_broadcast(&gTickCV);
        }

        pthread_mutex_unlock(&gMutex);
    }
    return nullptr;
}
