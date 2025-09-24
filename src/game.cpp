#include <ncurses.h>
#include <string>

// Estructura del estado actual del juego
struct GameState {
    // Geometría
    int width, height;
    float ball_x, ball_y, ball_vx, ball_vy;
    int paddle_x, paddle_y, paddle_w;

    // Puntuación/vidas
    int score = 0, lives = 3, level = 1;
    bool running = true, paused = false, gameover = false, need_redraw = true;

    // Sincronización
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  cv_tick = PTHREAD_COND_INITIALIZER;   // marca de avance de frame
    pthread_barrier_t frame_barrier;                      // sync fin de frame
    sem_t event_sem;                                      // eventos (audio/log)
    InputState input;
};