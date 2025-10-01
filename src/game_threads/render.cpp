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

            // HUD inferior persistente (etiquetas)
            mvprintw(local.bottom - 1, local.left + 2,
                "Flechas/A-D: Mover | SPACE: Lanzar | P: Pausa | R: Reiniciar | Q/ESC: Salir ");

            // Marca frameDrawn en la config real
            pthread_mutex_lock(&gMutex);
            cfg->frameDrawn = true;
            pthread_mutex_unlock(&gMutex);
        }

        werase(local.winPlay);

        // 2) HUD dinámico (score/vidas/paused)
        mvprintw(local.top + 1, local.left + 2, " Score: %d | Lives: %d | %s ",
                 local.score, local.lives, local.paused ? "PAUSED" : "PLAYING");

        // 3) Ladrillos sólo si gridDirty
        if (local.gridDirty) {
            std::vector<std::string> buf(local.h, std::string(local.w, ' '));

            // Limpia área de ladrillos (rellenar espacios sobre el bloque de ladrillos)
            int totalGaps = (local.cols - 1) * local.gapX;
            int usableW   = local.w - 2;
            int brickW    = (usableW - totalGaps) / local.cols;
            int remainder = (usableW - totalGaps) - (brickW * local.cols);
            int startY    = local.y0 + 2;

            // Borra zona de ladrillos (opcional: repinta espacios)
            for (int r = 0; r < local.rows; ++r) {
                int by = startY + r * (local.brickH + local.gapY);
                for (int c = local.x0 + 1; c < local.x1; ++c) {
                    mvaddch(by, c, ' ');
                }
            }

            // Dibuja ladrillos vivos
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

            // Se apaga gridDirty en config
            pthread_mutex_lock(&gMutex);
            cfg->brickBuffer.swap(buf);
            cfg->brickBufferReady = true;
            cfg->gridDirty = false;
            pthread_mutex_unlock(&gMutex);
        }

        if (local.brickBufferReady) {
            for (int y = 0; y < (int)local.brickBuffer.size(); ++y) {
                // 1 llamada por fila:
                mvwaddnstr(local.winPlay, y, 0,
                        local.brickBuffer[y].c_str(),
                        (int)local.brickBuffer[y].size());
            }
        }

        // 4) Paleta
        for (int i = 0; i < local.paddleW; ++i) {
            mvwaddch(local.winPlay,
                    local.paddleY - local.y0 - 1,
                    local.paddleX - local.x0 - 1 + i, '=');
        }

        // 5) Pelota
        mvwaddch(local.winPlay,
                (int)std::round(local.ballY) - local.y0 - 1,
                (int)std::round(local.ballX) - local.x0 - 1, 'o');

        // 6) Mensajes
        if (!local.ballLaunched) {
            mvprintw(local.y0 + local.h/2, local.x0 + 2, "Presiona ESPACIO para lanzar la bola");
        }
        if (local.won) {
            mvprintw(local.y0 + local.h/2, local.x0 + local.w/2 - 10, "¡GANASTE! Presiona Q");
        }
        if (local.lost) {
            mvprintw(local.y0 + local.h/2, local.x0 + local.w/2 - 10, "PERDISTE - Presiona Q");
        }

        refresh();

        // Cierra el paso
        pthread_mutex_lock(&gMutex);
        if (cfg->running) {
            // nada; tick pondrá step = 0 al siguiente frame
        }
        pthread_mutex_unlock(&gMutex);
    }

    return nullptr;
}