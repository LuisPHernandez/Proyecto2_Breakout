#include "../game.h"
#include <algorithm>    // std::min, std::max

extern pthread_mutex_t gMutex;
extern std::atomic<bool> gStopAll;

void* speedThread(void* arg) {
    auto* cfg = (GameConfig*)arg;

    unsigned long lastFrame = 0;
    int throttle = 0; // ajustamos cada N frames para evitar tocarlo en cada tick

    while (!gStopAll.load()) {
        lastFrame = waitNextFrame(cfg, lastFrame);

        // cada ~6 frames (~100ms si estás en ~60fps)
        if (++throttle < 6) continue;
        throttle = 0;

        pthread_mutex_lock(&gMutex);
        // Limita y suaviza hacia un objetivo (si alguien cambió brusco con teclas)
        cfg->ballSpeed = std::max(0.5f, std::min(2.0f, cfg->ballSpeed));

        // (Opcional) pequeña auto-aceleración por score; quita esto si no lo quieres
        float target = cfg->ballSpeed;
        if      (cfg->score >= 400) target = std::max(target, 1.6f);
        else if (cfg->score >= 200) target = std::max(target, 1.4f);
        else if (cfg->score >= 100) target = std::max(target, 1.2f);

        // Lerp suave
        cfg->ballSpeed += 0.10f * (target - cfg->ballSpeed);
        pthread_mutex_unlock(&gMutex);
    }
    return nullptr;
}