#include "../game.h"
#include <ncurses.h>
#include <pthread.h>
#include <atomic>
#include <unistd.h>

void* paddleThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    unsigned long lastFrame = 0;
    
    while (!gStopAll.load()) {
        // Esperar siguiente frame
        lastFrame = waitNextFrame(cfg, lastFrame);
        
        if (gStopAll.load()) break;
        
        pthread_mutex_lock(&gMutex);
        
        // Solo mover si el juego está corriendo y no esta pausado
        if (cfg->running && !cfg->paused) {
            
            // Velocidad de movimiento de la paleta 
            const int PADDLE_SPEED = 2;
            
            // Calcular nueva posición segun la direccion que el user quiera 
            int newX = cfg->paddleX + (cfg->desiredDir * PADDLE_SPEED);
            
            // Limitar a los bordes para que no se salga del area de juego
            // La paleta debe quedar adentro del area de juego que se limito
            int minX = cfg->x0;
            int maxX = cfg->x1 - cfg->paddleW + 1;
            
            // Aplicar límites
            if (newX < minX) {
                newX = minX;
            } else if (newX > maxX) {
                newX = maxX;
            }
            
            cfg->paddleX = newX;
            
            // Si la bola no ha sido lanzada, entonces se va a pegar  a la paleta
            if (!cfg->ballLaunched) {
                // Centrar la bola en la paleta
                cfg->ballX = cfg->paddleX + (cfg->paddleW / 2.0f);
                cfg->ballY = cfg->paddleY - 1.0f;  // Justo encima de la paleta
            }
            
           
        }
        
        pthread_mutex_unlock(&gMutex);
    }
    
    return nullptr;
}