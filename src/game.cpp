#include <ncurses.h>
#include <pthread.h>
#include <atomic>
#include <vector>
#include <unistd.h>
#include <cmath>

// Estructura de un ladrillo
struct Brick {
    int hp;      // Puntos de vida
    char ch;     // Carácter visual
    int points;  // Puntos que otorga
};

// Estado general del juego
struct GameConfig {
    // Área jugable
    int top, left, bottom, right;
    int x0, y0, x1, y1, w, h;

    // Paleta
    int paddleW;
    int paddleX, paddleY;
    int desiredDir;

    // Bola
    float ballX, ballY;
    float ballVX, ballVY;
    bool ballLaunched;

    // Ladrillos
    int rows, cols, gapX, gapY, brickH;
    std::vector<std::vector<Brick>> grid;

    // Estado general
    int score;
    int lives;
    bool paused;
    bool running;
    bool restartRequested;
    bool won;
    bool lost;

    // Timing
    int tick_ms;
    unsigned long frameCounter;
};

pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gTickCV = PTHREAD_COND_INITIALIZER;
std::atomic<bool> gStopAll(false);

// Función auxiliar que espera hasta que toque la siguiente frame
static unsigned long waitNextFrame(GameConfig* cfg, unsigned long lastFrame) {
    pthread_mutex_lock(&gMutex);
    while (!gStopAll.load() &&
           (cfg->paused || cfg->frameCounter == lastFrame || !cfg->running)) {
        pthread_cond_wait(&gTickCV, &gMutex);
    }
    unsigned long f = cfg->frameCounter;
    pthread_mutex_unlock(&gMutex);
    return f;
}

// ============================================================================
// PERSONA 1 - LUPITA (Bola y Colisiones)
// ============================================================================
void* ballThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;
    
    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        
        pthread_mutex_lock(&gMutex);
        
        // TODO: LUPA - Implementar física y colisiones de la bola
        // Por ahora, solo movimiento básico para testing
        if (cfg->running && !cfg->paused && cfg->ballLaunched) {
            cfg->ballX += cfg->ballVX;
            cfg->ballY += cfg->ballVY;
            
            // Colisiones temporales con paredes
            if (cfg->ballX <= cfg->x0) {
                cfg->ballX = cfg->x0;
                cfg->ballVX = fabs(cfg->ballVX);
            }
            if (cfg->ballX >= cfg->x1) {
                cfg->ballX = cfg->x1;
                cfg->ballVX = -fabs(cfg->ballVX);
            }
            if (cfg->ballY <= cfg->y0) {
                cfg->ballY = cfg->y0;
                cfg->ballVY = fabs(cfg->ballVY);
            }
            
            // Colisión temporal con paleta
            if ((int)cfg->ballY == cfg->paddleY && 
                (int)cfg->ballX >= cfg->paddleX && 
                (int)cfg->ballX < cfg->paddleX + cfg->paddleW) {
                cfg->ballVY = -fabs(cfg->ballVY);
            }
            
            // Perder vida si cae al fondo
            if (cfg->ballY >= cfg->y1) {
                cfg->lives--;
                cfg->ballLaunched = false;
                cfg->ballX = cfg->paddleX + cfg->paddleW / 2.0f;
                cfg->ballY = cfg->paddleY - 1.0f;
                cfg->ballVX = 0.0f;
                cfg->ballVY = 0.0f;
                
                if (cfg->lives <= 0) {
                    cfg->lost = true;
                    cfg->running = false;
                }
            }
        }
        
        pthread_mutex_unlock(&gMutex);
    }
    
    return nullptr;
}

// ============================================================================
// PERSONA 2 - JOEL (Paleta & Entrada) 
// ============================================================================

// INPUT THREAD - Lee teclas y actualiza intenciones
void* inputThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    
    // Configurar getch() en modo no bloqueante
    nodelay(stdscr, TRUE);
    
    while (!gStopAll.load()) {
        int ch = getch();
        
        if (ch != ERR) {
            pthread_mutex_lock(&gMutex);
            
            switch (ch) {
                // ========== MOVIMIENTO PALETA ==========
                case KEY_LEFT:
                case 'a':
                case 'A':
                    cfg->desiredDir = -1;
                    break;
                
                case KEY_RIGHT:
                case 'd':
                case 'D':
                    cfg->desiredDir = 1;
                    break;
                
                // Detener movimiento (opcional)
                case KEY_DOWN:
                case 's':
                case 'S':
                    cfg->desiredDir = 0;
                    break;
                
                // ========== PAUSA ==========
                case 'p':
                case 'P':
                    cfg->paused = !cfg->paused;
                    if (!cfg->paused) {
                        pthread_cond_broadcast(&gTickCV);
                    }
                    break;
                
                // ========== LANZAR BOLA ==========
                case ' ':  // Espacio
                    if (!cfg->ballLaunched && cfg->running) {
                        cfg->ballLaunched = true;
                        // Velocidad inicial
                        cfg->ballVX = 0.5f;
                        cfg->ballVY = -1.0f;
                    }
                    break;
                
                // ========== REINICIAR NIVEL ==========
                case 'r':
                case 'R':
                    if (cfg->running) {
                        cfg->restartRequested = true;
                        pthread_cond_broadcast(&gTickCV);
                    }
                    break;
                
                // ========== SALIR ==========
                case 'q':
                case 'Q':
                case 27:  // ESC
                    cfg->running = false;
                    gStopAll.store(true);
                    pthread_cond_broadcast(&gTickCV);
                    break;
            }
            
            pthread_mutex_unlock(&gMutex);
        }
        
        usleep(10000);  // 10ms - evitar saturar CPU
    }
    
    return nullptr;
}

// PADDLE THREAD - Mueve la paleta
void* paddleThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;
    
    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);
        
        if (gStopAll.load()) break;
        
        pthread_mutex_lock(&gMutex);
        
        if (cfg->running && !cfg->paused) {
            // Velocidad de movimiento
            const int PADDLE_SPEED = 2;
            
            // Calcular nueva posición
            int newX = cfg->paddleX + (cfg->desiredDir * PADDLE_SPEED);
            
            // Limitar a bordes
            int minX = cfg->x0;
            int maxX = cfg->x1 - cfg->paddleW + 1;
            
            if (newX < minX) {
                newX = minX;
            } else if (newX > maxX) {
                newX = maxX;
            }
            
            cfg->paddleX = newX;
            
            // ========== PEGAR BOLA A PALETA ==========
            // Si la bola no está lanzada, mantenerla en la paleta
            if (!cfg->ballLaunched) {
                cfg->ballX = cfg->paddleX + (cfg->paddleW / 2.0f);
                cfg->ballY = cfg->paddleY - 1.0f;
            }
        }
        
        pthread_mutex_unlock(&gMutex);
    }
    
    return nullptr;
}

// ============================================================================
// PERSONA 3 - MIGUEL (Render)
// ============================================================================
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

// ============================================================================
// TICK THREAD - Coordinador de tiempo
// ============================================================================
void* tickThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    
    while (!gStopAll.load()) {
        usleep(cfg->tick_ms * 1000);
        
        pthread_mutex_lock(&gMutex);
        
        if (cfg->running && !cfg->paused) {
            cfg->frameCounter++;
        }
        
        pthread_cond_broadcast(&gTickCV);
        
        pthread_mutex_unlock(&gMutex);
    }
    
    return nullptr;
}

// ============================================================================
// INICIALIZACIÓN DEL JUEGO
// ============================================================================
void initGameConfig(GameConfig* cfg, int rows, int cols) {
    // Área de juego
    cfg->top = rows/2 - 12;
    cfg->left = cols/2 - 40;
    cfg->bottom = rows/2 + 12;
    cfg->right = cols/2 + 40;
    
    cfg->x0 = cfg->left + 1;
    cfg->y0 = cfg->top + 2;
    cfg->x1 = cfg->right - 1;
    cfg->y1 = cfg->bottom - 2;
    cfg->w = cfg->x1 - cfg->x0 + 1;
    cfg->h = cfg->y1 - cfg->y0 + 1;
    
    // Paleta
    cfg->paddleW = 9;
    cfg->paddleY = cfg->y1 - 1;
    cfg->paddleX = cfg->x0 + (cfg->w - cfg->paddleW) / 2;
    cfg->desiredDir = 0;
    
    // Bola (pegada a la paleta)
    cfg->ballX = cfg->paddleX + cfg->paddleW / 2.0f;
    cfg->ballY = cfg->paddleY - 1.0f;
    cfg->ballVX = 0.0f;
    cfg->ballVY = 0.0f;
    cfg->ballLaunched = false;
    
    // Ladrillos
    cfg->rows = 4;
    cfg->cols = 10;
    cfg->gapX = 1;
    cfg->gapY = 1;
    cfg->brickH = 1;
    
    cfg->grid.resize(cfg->rows);
    for (int r = 0; r < cfg->rows; ++r) {
        cfg->grid[r].resize(cfg->cols);
        for (int c = 0; c < cfg->cols; ++c) {
            cfg->grid[r][c].hp = 1;
            cfg->grid[r][c].ch = '#';
            cfg->grid[r][c].points = 10;
        }
    }
    
    // Estado
    cfg->score = 0;
    cfg->lives = 3;
    cfg->paused = false;
    cfg->running = true;
    cfg->restartRequested = false;
    cfg->won = false;
    cfg->lost = false;
    
    // Timing (30 FPS)
    cfg->tick_ms = 33;
    cfg->frameCounter = 0;
}

// ============================================================================
// FUNCIÓN PRINCIPAL DEL JUEGO
// ============================================================================
int runGame() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    GameConfig cfg;
    initGameConfig(&cfg, rows, cols);
    
    gStopAll.store(false);
    
    // Crear todos los hilos
    pthread_t threads[5];
    pthread_create(&threads[0], nullptr, tickThread, &cfg);
    pthread_create(&threads[1], nullptr, inputThread, &cfg);    //  Persona 2
    pthread_create(&threads[2], nullptr, paddleThread, &cfg);   // Persona 2
    pthread_create(&threads[3], nullptr, ballThread, &cfg);     // Persona 1 
    pthread_create(&threads[4], nullptr, renderThread, &cfg);   // Persona 3
    
    // Esperar a que terminen
    for (int i = 0; i < 5; ++i) {
        pthread_join(threads[i], nullptr);
    }
    
    endwin();
    return 0;
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    runGame();
    return 0;
}