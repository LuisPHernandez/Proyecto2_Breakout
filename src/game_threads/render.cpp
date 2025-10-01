#include "../game.h"
#include <pthread.h>    
#include <atomic>     
#include <ncurses.h>    
#include <cstddef>
#include <cstring>
#include <cmath>

void* renderThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;
    
    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        GameConfig local;

        pthread_mutex_lock(&gMutex);
        local = *cfg;
        pthread_mutex_unlock(&gMutex);

        clear();
        
        // Dibujar marco del juego
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
        int titleLen = (int)std::strlen(title);;
        mvprintw(local.top, local.left + (local.w - titleLen) / 2, "%s", title);
        
        // Información de estado
        mvprintw(local.top + 1, local.left + 2, " Score: %d | Lives: %d | %s ", 
                 local.score, local.lives, local.paused ? "PAUSED" : "PLAYING");
        
        // Instrucciones
        mvprintw(local.bottom - 1, local.left + 2, 
                 "Flechas o A/D: Mover | SPACE: Lanzar | P: Pausa | R: Reiniciar | Q/ESC: Salir ");
        
        // Dibujar ladrillos
        int totalGaps = (local.cols - 1) * local.gapX;
        int usableW = local.w - 2;  // Ancho disponible dentro del marco
        int brickW = (usableW - totalGaps) / local.cols;
        int remainder = (usableW - totalGaps) - (brickW * local.cols);

        int startY = local.y0 + 2;

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

        
        // Dibujar paleta
        for (int i = 0; i < local.paddleW; ++i) {
            mvaddch(local.paddleY, local.paddleX + i, '=');
        }
        
        // Dibujar pelota
        mvaddch((int)roundf(local.ballY), (int)roundf(local.ballX), 'o');
        
        // Indicador de bola no lanzada
        if (!local.ballLaunched) {
            mvprintw(local.y0 + local.h / 2, local.x0 + 2, "Presiona ESPACIO para lanzar la bola");
        } 
        
        // Mensajes de fin
        if (local.won) {
            mvprintw(local.y0 + local.h / 2, local.x0 + local.w / 2 - 10, 
                     "¡GANASTE! Presiona Q");
        }
        if (local.lost) {
            mvprintw(local.y0 + local.h / 2, local.x0 + local.w / 2 - 10, 
                     "PERDISTE - Presiona Q");
        }
        
        refresh();
    }
    
    return nullptr;
}