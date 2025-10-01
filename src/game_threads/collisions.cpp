#include "game.h"

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