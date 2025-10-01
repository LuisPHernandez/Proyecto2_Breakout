#include "../game.h"
#include <pthread.h>
#include <atomic>
#include <ncurses.h>
#include <cstring>
#include <cmath>

void* renderThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);

        // Snapshot rápido
        GameConfig local;
        pthread_mutex_lock(&gMutex);
        local = *cfg;
        pthread_mutex_unlock(&gMutex);

        // 1) Marco + HUD una vez
        if (!local.frameDrawn) {
            clear();
            
            // Dibuja el marco
            for (int x = local.left; x <= local.right; ++x) {
                mvaddch(local.top, x, '=');
                mvaddch(local.bottom, x, '=');
            }
            for (int y = local.top; y <= local.bottom; ++y) {
                mvaddch(y, local.left, '|');
                mvaddch(y, local.right, '|');
            }
            mvaddch(local.top, local.left, '+');
            mvaddch(local.top, local.right, '+');
            mvaddch(local.bottom, local.left, '+');
            mvaddch(local.bottom, local.right, '+');

            // Título
            const char* title = "BREAKOUT";
            int titleLen = (int)std::strlen(title);
            mvprintw(local.top, local.left + (local.w - titleLen) / 2, "%s", title);

            // HUD inferior persistente
            mvprintw(local.bottom + 1, local.left + 2,
                "Flechas/A-D: Mover | SPACE: Lanzar | P: Pausa | R: Reiniciar | Q/ESC: Salir");

            pthread_mutex_lock(&gMutex);
            cfg->frameDrawn = true;
            pthread_mutex_unlock(&gMutex);
        }

        // 2) Limpiar área de juego completa (entre el marco)
        for (int y = local.y0 + 1; y < local.y1; ++y) {
            for (int x = local.x0 + 1; x < local.x1; ++x) {
                mvaddch(y, x, ' ');
            }
        }

        // 3) HUD dinámico (score/vidas/paused)
        mvprintw(local.top + 1, local.left + 2, " Score: %d | Lives: %d | Level: %d | %s ",
                 local.score, local.lives, local.level, local.paused ? "PAUSED" : "PLAYING");

        // 4) Dibujar ladrillos
        int totalGaps = (local.cols - 1) * local.gapX;
        int usableW   = local.w - 2;
        int brickW    = (usableW - totalGaps) / local.cols;
        int remainder = (usableW - totalGaps) - (brickW * local.cols);
        int startY    = local.y0 + 2;

        for (int r = 0; r < local.rows; ++r) {
            int by = startY + r * (local.brickH + local.gapY);
            int x = local.x0 + 1;
            for (int c = 0; c < local.cols; ++c) {
                if (local.grid[r][c].hp > 0) {
                    int thisW = brickW + (c < remainder ? 1 : 0);
                    for (int k = 0; k < thisW; ++k) {
                        for (int h = 0; h < local.brickH; ++h) {
                            mvaddch(by + h, x + k, local.grid[r][c].ch);
                        }
                    }
                }
                x += brickW + (c < remainder ? 1 : 0);
                if (c < local.cols - 1) x += local.gapX;
            }
        }

        // Resetear gridDirty después de dibujar
        if (local.gridDirty) {
            pthread_mutex_lock(&gMutex);
            cfg->gridDirty = false;
            pthread_mutex_unlock(&gMutex);
        }

        // 5) Paleta
        int paddleScreenY = local.paddleY;
        int paddleScreenX = local.paddleX;
        for (int i = 0; i < local.paddleW; ++i) {
            mvaddch(paddleScreenY, paddleScreenX + i, '=');
        }

        if (local.twoPlayers) {
            for (int i = 0; i < local.paddle2W; ++i) {
                mvwaddch(local.winPlay,
                 local.paddle2Y - local.y0 - 1,
                 local.paddle2X - local.x0 - 1 + i, '=');
            }
        }


        // 6) Pelota
        int ballScreenY = (int)std::round(local.ballY);
        int ballScreenX = (int)std::round(local.ballX);
        mvaddch(ballScreenY, ballScreenX, 'o');

        // 7) Mensajes centrados
        if (!local.ballLaunched) {
            const char* msg = "Presiona ESPACIO para lanzar la bola";
            int msgLen = strlen(msg);
            mvprintw(local.y0 + local.h/2, local.x0 + (local.w - msgLen)/2, "%s", msg);
        }
        if (local.won) {
            const char* msg = "¡GANASTE! Presiona R";
            int msgLen = strlen(msg);
            mvprintw(local.y0 + local.h/2, local.x0 + (local.w - msgLen)/2, "%s", msg);
        }
        if (local.lost) {
            const char* msg = "PERDISTE - Presiona R";
            int msgLen = strlen(msg);
            mvprintw(local.y0 + local.h/2, local.x0 + (local.w - msgLen)/2, "%s", msg);
        }

        refresh();
    }

    return nullptr;
}