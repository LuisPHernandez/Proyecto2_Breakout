#include "../game.h"
#include <pthread.h>
#include <atomic>
#include <cmath>

void* collisionsBricksThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        pthread_mutex_lock(&gMutex);

        while (!gStopAll.load() && cfg->running && cfg->step != 3) {
            pthread_cond_wait(&gTickCV, &gMutex);
        }

        if (cfg->running && !cfg->paused && cfg->ballLaunched) {
            int totalGaps = (cfg->cols - 1) * cfg->gapX;
            int usableW   = cfg->w;
            int cols      = (cfg->cols > 0 ? cfg->cols : 1);
            int brickW    = std::max(1, (usableW - totalGaps) / cols);
            int remainder = (usableW - totalGaps) - (brickW * cols);
            int startY    = cfg->y0 + 1 + 1;

            int yInt = (int)std::round(cfg->ballY);
            if (yInt >= startY && yInt < startY + cfg->rows * (cfg->brickH + cfg->gapY)) {
                int r = -1;
                for (int i = 0; i < cfg->rows; ++i) {
                    int by = startY + i * (cfg->brickH + cfg->gapY);
                    if (yInt == by) { r = i; break; }
                }
                if (r >= 0) {
                    int x = cfg->x0 + 1;
                    int cHit = -1, hitW = 0, startX = x;
                    for (int c = 0; c < cfg->cols; ++c) {
                        int thisW = brickW + (c < remainder ? 1 : 0);
                        if ((int)std::round(cfg->ballX) >= x && (int)std::round(cfg->ballX) < x + thisW) {
                            cHit = c; hitW = thisW; startX = x; break;
                        }
                        x += thisW; if (c < cfg->cols - 1) x += cfg->gapX;
                    }
                    if (cHit >= 0) {
                        Brick &b = cfg->grid[r][cHit];
                        if (b.hp > 0) {
                            int localX = (int)std::round(cfg->ballX) - startX;
                            if (localX <= 0 || localX >= hitW - 1) cfg->ballVX = -cfg->ballVX;
                            else                                   cfg->ballVY = -cfg->ballVY;

                            b.hp--;
                            if (b.hp <= 0) { cfg->score += b.points; cfg->gridDirty = true; }
                        }
                    }
                }
            }
        }

        if (cfg->running) {
            cfg->step = 4;
            pthread_cond_broadcast(&gTickCV);
        }

        pthread_mutex_unlock(&gMutex);
    }
    return nullptr;
}