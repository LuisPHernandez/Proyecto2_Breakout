#include "../game.h"
#include <pthread.h>
#include <atomic>       
#include <unistd.h>     
#include <cstddef>       

void* tickThread(void* arg) {
    auto* cfg = (GameConfig*)arg;
    
    while (!gStopAll.load()) {
        usleep(cfg->tick_ms * 1000);
        
        pthread_mutex_lock(&gMutex);
        
        if (cfg->running && !cfg->paused) {
            cfg->frameCounter++;
            cfg->step = 0; 
        }
        
        pthread_cond_broadcast(&gTickCV);
        
        pthread_mutex_unlock(&gMutex);
    }
    
    return nullptr;
}
