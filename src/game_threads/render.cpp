#include "../game.h"
#include <pthread.h>    
#include <atomic>     
#include <ncurses.h>    
#include <cstddef>
#include <cstring>

void* renderThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;
    
    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        
        pthread_mutex_lock(&gMutex);

        clear();
        
        // Dibujar marco del juego
        for (int x = cfg->left; x <= cfg->right; ++x) {
            mvaddch(cfg->top, x, '=');
            mvaddch(cfg->bottom, x, '=');
        }
        for (int y = cfg->top; y <= cfg->bottom; ++y) {
            mvaddch(y, cfg->left, '|');
            mvaddch(y, cfg->right, '|');
        }
        mvaddch(cfg->top, cfg->left, '+');
        mvaddch(cfg->top, cfg->right, '+');
        mvaddch(cfg->bottom, cfg->left, '+');
        mvaddch(cfg->bottom, cfg->right, '+');
        
        // Título
        const char* title = "BREAKOUT";
        int titleLen = (int)std::strlen(title);;
        mvprintw(cfg->top, cfg->left + (cfg->w - titleLen) / 2, "%s", title);
        
        // Información de estado
        mvprintw(cfg->top + 1, cfg->left + 2, " Score: %d | Lives: %d | %s ", 
                 cfg->score, cfg->lives, cfg->paused ? "PAUSED" : "PLAYING");
        
        // Instrucciones
        mvprintw(cfg->bottom - 1, cfg->left + 2, 
                 "Flechas o A/D: Mover | SPACE: Lanzar | P: Pausa | R: Reiniciar | Q/ESC: Salir ");
        
        // Dibujar ladrillos
        int totalGaps = (cfg->cols - 1) * cfg->gapX;
        int usableW = cfg->w - 2;  // Ancho disponible dentro del marco
        int brickW = (usableW - totalGaps) / cfg->cols;
        int remainder = (usableW - totalGaps) - (brickW * cfg->cols);

        int startY = cfg->y0 + 2;

        for (int r = 0; r < cfg->rows; ++r) {
            int by = startY + r * (cfg->brickH + cfg->gapY);

            int x = cfg->x0 + 1;
            for (int c = 0; c < cfg->cols; ++c) {
                if (cfg->grid[r][c].hp > 0) {
                    int thisW = brickW + (c < remainder ? 1 : 0);

                    for (int k = 0; k < thisW; ++k) {
                        for (int h = 0; h < cfg->brickH; ++h) {
                            mvaddch(by + h, x + k, cfg->grid[r][c].ch);
                        }
                    }
                }
                x += brickW + (c < remainder ? 1 : 0);
                if (c < cfg->cols - 1) x += cfg->gapX;
            }
        }

        
        // Dibujar paleta
        for (int i = 0; i < cfg->paddleW; ++i) {
            mvaddch(cfg->paddleY, cfg->paddleX + i, '=');
        }
        
        // Dibujar pelota
        mvaddch((int)cfg->ballY, (int)cfg->ballX, 'o');
        
        // Indicador de bola no lanzada
        if (!cfg->ballLaunched) {
            mvprintw(cfg->y0 + cfg->h / 2, cfg->x0 + 2, "Presiona ESPACIO para lanzar la bola");
        } 
        
        // Mensajes de fin
        if (cfg->won) {
            mvprintw(cfg->y0 + cfg->h / 2, cfg->x0 + cfg->w / 2 - 10, 
                     "¡GANASTE! Presiona Q");
        }
        if (cfg->lost) {
            mvprintw(cfg->y0 + cfg->h / 2, cfg->x0 + cfg->w / 2 - 10, 
                     "PERDISTE - Presiona Q");
        }
        
        refresh();
        
        pthread_mutex_unlock(&gMutex);
    }
    
    return nullptr;
}