#include "../game.h"
#include <pthread.h>
#include <atomic>
#include <cmath>

static void normalizeAngle(float& vx, float& vy) {
    const float MIN_X = 0.15f, MIN_Y = 0.25f;
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
            // Paredes laterales (en coordenadas de pantalla)
            if (cfg->ballX <= cfg->x0 + 1) { 
                cfg->ballX = cfg->x0 + 2; 
                cfg->ballVX = -cfg->ballVX; 
                normalizeAngle(cfg->ballVX, cfg->ballVY); 
            }
            if (cfg->ballX >= cfg->x1 - 1) { 
                cfg->ballX = cfg->x1 - 2; 
                cfg->ballVX = -cfg->ballVX; 
                normalizeAngle(cfg->ballVX, cfg->ballVY); 
            }

            // Techo
            if (cfg->ballY <= cfg->y0 + 2) { 
                cfg->ballY = cfg->y0 + 3; 
                cfg->ballVY = -cfg->ballVY; 
                normalizeAngle(cfg->ballVX, cfg->ballVY); 
            }

            // Piso (perder vida)
            if (cfg->ballY >= cfg->paddleY + 2) {
                cfg->lives--;
                cfg->ballLaunched = false;
                cfg->ballJustReset = true;
                cfg->ballVX = 0.0f; 
                cfg->ballVY = 0.0f;
                cfg->ballX = cfg->paddleX + cfg->paddleW / 2.0f;
                cfg->ballY = cfg->paddleY - 1.0f;
                
                if (cfg->lives <= 0) { 
                    cfg->lost = true; 
                    cfg->running = false;
                    pthread_cond_signal(&gCtrlCV);
                }
            }

            // Colisión con paleta
            int ballIntY = (int)std::round(cfg->ballY);
            int ballIntX = (int)std::round(cfg->ballX);
            
            if (ballIntY == cfg->paddleY - 1 || ballIntY == cfg->paddleY) {
                if (ballIntX >= cfg->paddleX && ballIntX < cfg->paddleX + cfg->paddleW) {
                    cfg->ballY = cfg->paddleY - 2;
                    cfg->ballVY = -std::fabs(cfg->ballVY);
                    
                    // Ajustar dirección horizontal según dónde golpeó
                    float center = cfg->paddleX + cfg->paddleW / 2.0f;
                    float half   = std::max(1.0f, cfg->paddleW / 2.0f);
                    float rel    = (cfg->ballX - center) / half; // [-1..+1]
                    cfg->ballVX  = rel * 0.6f;
                    normalizeAngle(cfg->ballVX, cfg->ballVY);
                }
            }

            // Colisión con paleta 2 (coop)
            if (cfg->twoPlayers && cfg->paddle2W > 0) {
                int ballIntY = (int)std::round(cfg->ballY);
                int ballIntX = (int)std::round(cfg->ballX);

                if (ballIntY == cfg->paddle2Y - 1 || ballIntY == cfg->paddle2Y) {
                    if (ballIntX >= cfg->paddle2X && ballIntX < cfg->paddle2X + cfg->paddle2W) {
                        cfg->ballY  = cfg->paddle2Y - 2;
                        cfg->ballVY = -std::fabs(cfg->ballVY);

                        // Ajustar dirección horizontal según dónde golpeó (misma fórmula que P1)
                        float center2 = cfg->paddle2X + cfg->paddle2W / 2.0f;
                        float half2   = std::max(1.0f, cfg->paddle2W / 2.0f);
                        float rel2    = (cfg->ballX - center2) / half2;   // [-1..+1]
                        cfg->ballVX   = rel2 * 0.6f;

                        normalizeAngle(cfg->ballVX, cfg->ballVY);
                    }
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