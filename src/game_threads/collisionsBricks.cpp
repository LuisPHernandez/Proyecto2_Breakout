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
            int usableW   = cfg->w - 2;
            int cols      = (cfg->cols > 0 ? cfg->cols : 1);
            int brickW    = std::max(1, (usableW - totalGaps) / cols);
            int remainder = (usableW - totalGaps) - (brickW * cols);
            int startY    = cfg->y0 + 2;

            int ballIntY = (int)std::round(cfg->ballY);
            int ballIntX = (int)std::round(cfg->ballX);

            bool collisionFound = false;

            // Buscar colisión con ladrillos
            for (int r = 0; r < cfg->rows && !collisionFound; ++r) {
                int by = startY + r * (cfg->brickH + cfg->gapY);
                
                // Verificar si la pelota está a la altura de esta fila
                bool inRow = (ballIntY >= by && ballIntY < by + cfg->brickH);
                
                if (inRow) {
                    // Buscar en qué columna está
                    int x = cfg->x0 + 1;
                    
                    for (int c = 0; c < cfg->cols && !collisionFound; ++c) {
                        int thisW = brickW + (c < remainder ? 1 : 0);
                        
                        // Verificar si la pelota está dentro de este ladrillo horizontalmente
                        bool inCol = (ballIntX >= x && ballIntX < x + thisW);
                        
                        if (inCol) {
                            Brick &brick = cfg->grid[r][c];
                            
                            if (brick.hp > 0) {
                                // Calcular posición relativa dentro del ladrillo
                                int relY = ballIntY - by;
                                int relX = ballIntX - x;
                                
                                // Determinar si golpea arriba/abajo o izquierda/derecha
                                bool hitSide = (relX == 0 || relX == thisW - 1);
                                bool hitTopBottom = (relY == 0 || relY == cfg->brickH - 1);
                                
                                if (hitSide) {
                                    cfg->ballVX = -cfg->ballVX;
                                } else {
                                    cfg->ballVY = -cfg->ballVY;
                                }

                                // Reducir HP del ladrillo
                                brick.hp--;
                                
                                // Si se destruyó, sumar puntos
                                if (brick.hp <= 0) {
                                    cfg->score += brick.points;
                                    cfg->gridDirty = true;
                                }
                                
                                collisionFound = true;
                            }
                        }
                        
                        x += thisW;
                        if (c < cfg->cols - 1) x += cfg->gapX;
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