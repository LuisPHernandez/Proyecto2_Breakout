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
                case KEY_LEFT: case 'a': case 'A':
                    cfg->desiredDir = -1; lastInput = clock::now(); break;
                case KEY_RIGHT: case 'd': case 'D':
                    cfg->desiredDir = 1; lastInput = clock::now(); break;

                case 'p': case 'P':
                    cfg->paused = !cfg->paused;
                    pthread_cond_broadcast(&gTickCV);
                    lastInput = clock::now();
                    break;

                case ' ':
                    if (!cfg->ballLaunched && cfg->running) {
                        cfg->ballLaunched = true;
                        cfg->ballJustReset = false;
                        cfg->ballVX = (std::rand() % 2 == 0 ? -0.5f : 0.5f);
                        cfg->ballVY = -1.0f;
                    }
                    lastInput = clock::now();
                    break;

                case 'r': case 'R':
                    if (cfg->running) {
                        cfg->restartRequested = true;
                        pthread_cond_broadcast(&gTickCV);
                    }
                    lastInput = clock::now();
                    break;

                case 'q': case 'Q': case 27:
                    cfg->running = false;
                    gStopAll.store(true);
                    pthread_cond_broadcast(&gTickCV);
                    lastInput = clock::now();
                    break;
            }

            pthread_mutex_unlock(&gMutex);
        } else {
            // No tecla; si pasaron >80ms sin input, para la paleta
            auto now = clock::now();
            if (now - lastInput > std::chrono::milliseconds(80)) {
                pthread_mutex_lock(&gMutex);
                cfg->desiredDir = 0;
                pthread_mutex_unlock(&gMutex);
            }
            // Pequeño descanso (no bloqueante)
            usleep(5'000);
        }
    }

    return nullptr;
}
