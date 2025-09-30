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
    GameConfig* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);

        pthread_mutex_lock(&gMutex);
        if (cfg->running && !cfg->paused && cfg->ballLaunched) {
            // Integración “por frame” (puedes subir 2x si quieres más suave)
            cfg->ballX += cfg->ballVX;
            cfg->ballY += cfg->ballVY;
        }
        pthread_mutex_unlock(&gMutex);

        // Fin de fase de actualización: sincroniza con el resto
        pthread_barrier_wait(&gFrameBarrier);
    }
    return nullptr;
}

void* collisionThread(void* arg) {
    GameConfig* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);

        pthread_mutex_lock(&gMutex);
        if (cfg->running && !cfg->paused && cfg->ballLaunched) {

            // 1) Paredes laterales
            if (cfg->ballX <= cfg->x0 + 1) { // Pared izquierda
                cfg->ballX = cfg->x0 + 2;
                cfg->ballVX = -cfg->ballVX;  // Cambio en dirección de velocidad en x (rebote)
                ensureInterestingAngle(cfg->ballVX, cfg->ballVY);
            }
            if (cfg->ballX >= cfg->x1 - 1) { // Pared derecha
                cfg->ballX = cfg->x1 - 2;   
                cfg->ballVX = -cfg->ballVX;  // Cambio en dirección de velocidad en x (rebote)
                ensureInterestingAngle(cfg->ballVX, cfg->ballVY);
            }

            // 2) Techo y piso
            if (cfg->ballY <= cfg->y0 + 1) { // Techo
                cfg->ballY = cfg->y0 + 2;
                cfg->ballVY = -cfg->ballVY;  // Cambio en dirección de velocidad en y (rebote)
                ensureInterestingAngle(cfg->ballVX, cfg->ballVY);
            }
            if (cfg->ballY >= cfg->y1 - 1) { // Piso
                // Si toca el piso, el usuario pierde
                cfg->lives--;
                cfg->ballLaunched = false;
                cfg->ballVX = 0.0f; cfg->ballVY = 0.0f;
                cfg->ballX = cfg->paddleX;
                cfg->ballY = cfg->paddleY - 1;

                if (cfg->lives <= 0) {
                    cfg->lost = true;
                    cfg->running = false;
                }
            }

            // 3) Paleta
            if (!cfg->paused && cfg->running) {
                int half = cfg->paddleW / 2; // Mitad del ancho de la paleta
                int py = cfg->paddleY;
                
                if ((int)cfg->ballY == py - 1) { // Impacto con la paleta (pelota una fila arriba)
                    if ((int)cfg->ballX >= cfg->paddleX - half && // Pelota dentro del ancho de la paleta
                        (int)cfg->ballX <= cfg->paddleX + half) {

                        // La pelota sube, cambio en dirección de velocidad
                        cfg->ballY = py - 2;
                        cfg->ballVY = -std::abs(cfg->ballVY > 0 ? cfg->ballVY : 1.0f);

                        // Ángulo según punto de impacto relativo
                        int rel = (int)cfg->ballX - cfg->paddleX;    // Distancia de punto de impacto a centro de la paleta
                        float normalized = (float)rel / (float)half; // Normaliza a un valor entre -1 y 1
                        cfg->ballVX = normalized * 1.2f;             // Multiplica la distancia normalizada por 1.2 (máxima)

                        // Se calcula el ángulo de salida
                        ensureInterestingAngle(cfg->ballVX, cfg->ballVY);
                    }
                }
            }

            // 4) Ladrillos
            int totalGaps = (cfg->cols - 1) * cfg->gapX; // Suma total de espacios en blanco entre ladrillos
            int usableW   = cfg->w - 2;                  // Margen interno
            int brickW    = (usableW - totalGaps) / cfg->cols;
            int remainder = (usableW - totalGaps) - (brickW * cfg->cols);
            int startY    = cfg->y0 + 2;

            // Detectamos fila por Y
            int yInt = (int)cfg->ballY; // Posición en y de la bola (en entero)

            // Se revisa si la pelota está en el bloque de ladrillos
            if (yInt >= startY && yInt < startY + cfg->rows * (cfg->brickH + cfg->gapY)) { 
                // Encontrar fila de ladrillos exacta donde colisiona la pelota
                int r = -1;
                for (int i = 0; i < cfg->rows; ++i) {
                    int by = startY + i * (cfg->brickH + cfg->gapY);
                    if (yInt == by) { r = i; break; }
                }

                // Si hay una fila candidata a colisión, se revisa la columna
                if (r >= 0) {
                    int x = cfg->x0 + 1;
                    int cHit = -1, hitW = 0, startX = x; // cHit = # de ladrillo, hitW = ancho del ladrillo, startX = x inicial del ladrillo
                    for (int c = 0; c < cfg->cols; ++c) {
                        int thisW = brickW + (c < remainder ? 1 : 0); // Se calcula el ancho, sumando o no 1 de remainder

                        // Se revisa si la pelota esta dentro del ancho del ladrillo
                        if ((int)cfg->ballX >= x && (int)cfg->ballX < x + thisW) { 
                            cHit = c; hitW = thisW; startX = x; break;
                        }
                        x += thisW; // Se suma el ancho a x para pasar al inicio del próximo ladrillo
                        if (c < cfg->cols - 1) { 
                            x += cfg->gapX; 
                        }
                    }

                    if (cHit >= 0) { // Si se encuentra un ladrillo
                        Brick &b = cfg->grid[r][cHit]; // Se toma una referencia al objeto Brick correspondiente a ese ladrillo
                        if (b.alive) { // Solo hay colisión si el ladrillo no ha sido destruido
                            // Decide rebote vertical u horizontal aproximado:
                            // Si impacta cerca de los bordes izquierdo/derecho del ladrillo → invierte X,
                            // si no → invierte Y.
                            int localX = (int)cfg->ballX - startX;
                            if (localX <= 0 || localX >= hitW - 1) {
                                cfg->ballVX = -cfg->ballVX;
                            }
                            else{
                                cfg->ballVY = -cfg->ballVY;
                            }

                            // Daño al ladrillo
                            b.lives--;
                            cfg->score += 10;
                            if (b.lives <= 0) {
                                b.alive = false;
                            }

                            // Se calcula el angulo de salida
                            ensureInterestingAngle(cfg->ballVX, cfg->ballVY);
                        }
                    }
                }
            }

            // 5) Chequeo de victoria
            bool anyAlive = false;
            for (auto &row : cfg->grid) { // Se revisa si hay ladrillos vivos
                for (auto &b : row) {
                    if (b.alive) {
                        anyAlive = true; 
                        break;
                    }
                }
                if (anyAlive) {
                    break;
                }
            }
            if (!anyAlive) { // Si no hay ningun ladrillo vivo, el usuario ganó
                cfg->won = true; cfg->running = false;
            }
        }
        pthread_mutex_unlock(&gMutex);

        // Fin de fase física
        pthread_barrier_wait(&gFrameBarrier);
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