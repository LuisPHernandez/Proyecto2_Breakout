void* renderThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;
    
    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        
        pthread_mutex_lock(&gMutex);
        
        // TODO: MIGUEL - Implementar dibujado completo con ladrillos
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
        const char* title = " BREAKOUT - PERSONA 2 DEMO ";
        int titleLen = 28;
        mvprintw(cfg->top, cfg->left + (cfg->w - titleLen) / 2, "%s", title);
        
        // Información de estado
        mvprintw(cfg->top + 1, cfg->left + 2, " Score: %d | Lives: %d | %s ", 
                 cfg->score, cfg->lives, cfg->paused ? "PAUSED" : "PLAYING");
        
        // Instrucciones
        mvprintw(cfg->bottom - 1, cfg->left + 2, 
                 " ←/→ o A/D: Mover | SPACE: Lanzar | P: Pausa | R: Reiniciar | Q/ESC: Salir ");
        
        // Dibujar ladrillos (placeholder simple)
        int startY = cfg->y0 + 2;
        for (int r = 0; r < cfg->rows; ++r) {
            int by = startY + r * 2;
            for (int c = 0; c < cfg->cols; ++c) {
                if (cfg->grid[r][c].hp > 0) {
                    int bx = cfg->x0 + c * 6;
                    for (int i = 0; i < 5; ++i) {
                        mvaddch(by, bx + i, cfg->grid[r][c].ch);
                    }
                }
            }
        }
        
        // ========== DIBUJAR PALETA ========== (Persona 2 controla esto)
        for (int i = 0; i < cfg->paddleW; ++i) {
            mvaddch(cfg->paddleY, cfg->paddleX + i, '=');
        }
        
        // ========== DIBUJAR BOLA ========== (Persona 2 mantiene posición si no lanzada)
        mvaddch((int)cfg->ballY, (int)cfg->ballX, 'o');
        
        // Indicador de bola no lanzada
        if (!cfg->ballLaunched) {
            mvprintw(cfg->y0 + cfg->h / 2, cfg->x0 + 2, 
                     "Presiona ESPACIO para lanzar la bola");
        }
        
        // Mensajes de fin
        if (cfg->won) {
            mvprintw(cfg->y0 + cfg->h / 2, cfg->x0 + cfg->w / 2 - 10, 
                     "¡GANASTE! Presiona Q");
        }
        if (cfg->lost) {
            mvprintw(cfg->y0 + cfg->h / 2, cfg->x0 + cfg->w / 2 - 10, 
                     "GAME OVER - Presiona Q");
        }
        
        refresh();
        
        pthread_mutex_unlock(&gMutex);
    }
    
    return nullptr;
}