#include "ipc.h"

#include <pthread.h>

struct ipc_state {
    pthread_mutex_t lock;
    int finished;
};

static struct ipc_state state = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .finished = 0,
};


void ipc_finish(void)
{
    pthread_mutex_lock(&state.lock);
    state.finished = 1;
    pthread_mutex_unlock(&state.lock);
}

int ipc_is_finished(void)
{
    pthread_mutex_lock(&state.lock);
    int f = state.finished;
    pthread_mutex_unlock(&state.lock);
    return f;
}
