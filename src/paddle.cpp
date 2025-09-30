// paddle.cpp- Implementación de 
// Control de paleta y entrada del teclado

#include <ncurses.h>
#include <pthread.h>
#include <atomic>
#include <unistd.h>
#include "game.cpp"  // Incluye la definición de GameConfig

// Variables externas del game.cpp
extern pthread_mutex_t gMutex;
extern pthread_cond_t gTickCV;
extern std::atomic<bool> gStopAll;

// INPUT THREAD - lo que va a ser es que va a leer las  teclas y actualiza la intención del jugador

void* inputThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    
    // Configurar getch() en modo no bloqueante
    nodelay(stdscr, TRUE);
    
    while (!gStopAll.load()) {
        int ch = getch();
        
        if (ch != ERR) {
            pthread_mutex_lock(&gMutex);
            
            switch (ch) {
                // MOVIMIENTO IZQUIERDA
                case KEY_LEFT:
                case 'a':
                case 'A':
                    cfg->desiredDir = -1;
                    break;
                
                // MOVIMIENTO DERECHA
                case KEY_RIGHT:
                case 'd':
                case 'D':
                    cfg->desiredDir = 1;
                    break;
                
                // DETENER MOVIMIENTO 
                case KEY_DOWN:
                case 's':
                case 'S':
                    cfg->desiredDir = 0;
                    break;
                
                // PAUSA
                case 'p':
                case 'P':
                    cfg->paused = !cfg->paused;
                    if (!cfg->paused) {
                        // Reanudar todos los hilos que esperan
                        pthread_cond_broadcast(&gTickCV);
                    }
                    break;
                
                // LANZAR BOLA
                case ' ':  // Espacio
                    if (!cfg->ballLaunched && cfg->running) {
                        cfg->ballLaunched = true;
                        // Velocidad inicial de la bola (pa arriba)
                        cfg->ballVX = 0.5f;  // tiene un poco de  inclinación
                        cfg->ballVY = -1.0f; // Hacia arriba
                    }
                    break;
                
                // REINICIAR NIVEL
                case 'r':
                case 'R':
                    if (cfg->running) {
                        cfg->restartRequested = true;
                        pthread_cond_broadcast(&gTickCV);
                    }
                    break;
                
                // SALIR
                case 'q':
                case 'Q':
                case 27:  // con "ESC"
                    cfg->running = false;
                    gStopAll.store(true);
                    pthread_cond_broadcast(&gTickCV);
                    break;
            }
            
            pthread_mutex_unlock(&gMutex);
        }
        
        //pausa para no presionar mucho al CPU
        usleep(10000);  // 10ms
    }
    
    return nullptr;
}


// PADDLE THREAD - Mueve la paleta según la dirección que el user quiera 

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


// FUNCIÓN AUXILIAR - Espera al siguiente frame

unsigned long waitNextFrame(GameConfig* cfg, unsigned long lastFrame) {
    pthread_mutex_lock(&gMutex);
    
    // Esperar hasta que:
    // 1. No esté detenido
    // 2. No esté pausado O haya un nuevo frame O el juego si este  corriendo
    while (!gStopAll.load() &&
           (cfg->paused || cfg->frameCounter == lastFrame || !cfg->running)) {
        pthread_cond_wait(&gTickCV, &gMutex);
    }
    
    unsigned long f = cfg->frameCounter;
    pthread_mutex_unlock(&gMutex);
    
    return f;
}