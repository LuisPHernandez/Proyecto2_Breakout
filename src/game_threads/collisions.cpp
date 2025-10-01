#include "../game.h"
#include <pthread.h>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cmath>

// Función implementada más abajo
static void getAngle(float& vx, float& vy);

void* collisionsThread(void* arg) {
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
                getAngle(cfg->ballVX, cfg->ballVY);
            }
            if (cfg->ballX >= cfg->x1 - 1) { // Pared derecha
                cfg->ballX = cfg->x1 - 2;   
                cfg->ballVX = -cfg->ballVX;  // Cambio en dirección de velocidad en x (rebote)
                getAngle(cfg->ballVX, cfg->ballVY);
            }

            // 2) Techo y piso
            if (cfg->ballY <= cfg->y0 + 1) { // Techo
                cfg->ballY = cfg->y0 + 2;
                cfg->ballVY = -cfg->ballVY;  // Cambio en dirección de velocidad en y (rebote)
                getAngle(cfg->ballVX, cfg->ballVY);
            }
            if (cfg->ballY > cfg->y1 - 1) { // Piso
                // Si toca el piso, el usuario pierde
                cfg->lives--;
                cfg->ballLaunched = false;
                cfg->ballVX = 0.0f; cfg->ballVY = 0.0f;
                cfg->ballX = cfg->paddleX + cfg->paddleW / 2.0f;
                cfg->ballY = cfg->paddleY - 1.0f;

                if (cfg->lives <= 0) {
                    cfg->lost = true;
                    cfg->running = false;
                }
            }

            // 3) Paleta
            if (!cfg->paused && cfg->running) {
                int half = cfg->paddleW / 2; // Mitad del ancho de la paleta
                if (half <= 0) half = 1;
                int py = cfg->paddleY;
                
                if ((int)cfg->ballY == py - 1) { // Impacto una fila arriba de la paleta
                    int bx = (int)cfg->ballX;
                    if (bx >= cfg->paddleX && bx <= (cfg->paddleX + (cfg->paddleW - 1))) { // Impacto dentro del ancho de la paleta
                        cfg->ballY = py - 2;
                        cfg->ballVY = -std::abs(cfg->ballVY > 0 ? cfg->ballVY : 1.0f);

                        // Ángulo según punto de impacto relativo respecto al centro
                        float center = cfg->paddleX + cfg->paddleW / 2.0f;
                        float half = std::max(1.0f, cfg->paddleW / 2.0f);
                        float rel = (cfg->ballX - center) / half;   // en [-1..+1]
                        cfg->ballVX = rel * 1.2f;

                        getAngle(cfg->ballVX, cfg->ballVY);
                    }
                }
            }

            // 4) Ladrillos
            int totalGaps = (cfg->cols - 1) * cfg->gapX; // Suma total de espacios en blanco entre ladrillos
            int usableW   = cfg->w - 2;                  // Margen interno
            if (usableW < 1) usableW = 1;

            int cols = (cfg->cols > 0 ? cfg->cols : 1); 

            if (totalGaps >= usableW) totalGaps = 0;

            int brickW = (usableW - totalGaps) / cols;
            if (brickW < 1) brickW = 1;

            int remainder = (usableW - totalGaps) - (brickW * cols);
            if (remainder < 0) remainder = 0;
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
                        if (b.hp > 0) { // Solo hay colisión si el ladrillo no ha sido destruido
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
                            b.hp--;
                            if (b.hp <= 0) {
                                cfg->score += b.points;
                            }

                            // Se calcula el angulo de salida
                            getAngle(cfg->ballVX, cfg->ballVY);
                        }
                    }
                }
            }

            // 5) Chequeo de victoria
            bool anyAlive = false;
            for (auto &row : cfg->grid) { // Se revisa si hay ladrillos vivos
                for (auto &b : row) {
                    if (b.hp > 0) {
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
    }
    return nullptr;
}

// Asegura que el ángulo de rebote no sea ni muy horizontal ni muy vertical para mejorar la jugabilidad
static void getAngle(float& vx, float& vy) {
    const float MIN_X = 0.2f; // velocidad mínima en X
    const float MIN_Y = 0.4f; // velocidad mínima en Y

    // Si está demasiado vertical (vx casi 0), se aumenta la velocidad en x
    if (std::fabs(vx) < MIN_X) {
        vx = (vx >= 0 ? MIN_X : -MIN_X);
    }

    // Si está demasiado horizontal (vy casi 0), se aumenta la velocidad en y
    if (std::fabs(vy) < MIN_Y) {
        vy = (vy >= 0 ? MIN_Y : -MIN_Y);
    }
}