#include "../game.h"
#include <ncurses.h>
#include <pthread.h>
#include <atomic>
#include <unistd.h>
#include <chrono>
#include <cstdlib>

void* inputThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    using clock = std::chrono::steady_clock;
    auto lastInput = clock::now();

    while (!gStopAll.load()) {
        int ch = getch();
        

        if (ch != ERR) {
            pthread_mutex_lock(&gMutex);

            switch (ch) {
                case 'a':
                case 'A':
                    cfg->desiredDir = -1;             // mover paleta 1 a la izquierda
                    lastInput = clock::now();
                    break;

                case 'd':
                case 'D':
                    cfg->desiredDir = 1;              // mover paleta 1 a la derecha
                    lastInput = clock::now();
                    break;

                // --- Movimiento P2 con flechas ---
                case KEY_LEFT:
                    if (cfg->twoPlayers) {
                        cfg->desiredDir2 = -1;        // mover paleta 2 a la izquierda
                        lastInput = clock::now();
                    } else {
                        cfg->desiredDir = -1;         // en single player, flechas también mueven P1
                        lastInput = clock::now();
                    }
                    break;

                case KEY_RIGHT:
                    if (cfg->twoPlayers) {
                        cfg->desiredDir2 = 1;         // mover paleta 2 a la derecha
                        lastInput = clock::now();
                    } else {
                        cfg->desiredDir = 1;          // en single player, flechas también mueven P1
                        lastInput = clock::now();
                    }
                    break;

                case 'p': case 'P':
                    cfg->paused = !cfg->paused;
                    pthread_cond_broadcast(&gTickCV);
                    lastInput = clock::now();
                    break;

                case ' ':
                    if (!cfg->ballLaunched && cfg->running) {
                        cfg->ballLaunched = true;
                        cfg->ballJustReset = false;
                        cfg->ballVX = (std::rand() % 2 == 0 ? -0.25f : 0.25f);
                        cfg->ballVY = -0.5f;
                    }
                    lastInput = clock::now();
                    break;

                case 'r': case 'R':
                    if (cfg->running || cfg->won || cfg->lost) {
                        cfg->restartRequested = true;
                        cfg->running = true; // Reactivar si estaba terminado
                        pthread_cond_signal(&gCtrlCV); // Notificar al control
                        pthread_cond_broadcast(&gTickCV);
                    }
                    lastInput = clock::now();
                    break;

                case 'q': case 'Q': case 27: // ESC
                    cfg->running = false;
                    gStopAll.store(true);
                    pthread_cond_signal(&gCtrlCV);
                    pthread_cond_broadcast(&gTickCV);
                    lastInput = clock::now();
                    break;
            }

            pthread_mutex_unlock(&gMutex);
        } else {
            // No hay tecla presionada
            auto now = clock::now();
            if (now - lastInput > std::chrono::milliseconds(50)) {
                pthread_mutex_lock(&gMutex);
                cfg->desiredDir = 0;
                if (cfg->twoPlayers) cfg->desiredDir2 = 0;
                pthread_mutex_unlock(&gMutex);
            }
            usleep(50000); // 3ms
        }
    }

    return nullptr;
}