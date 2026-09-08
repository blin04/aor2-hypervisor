#include "buffer.h"

#include <pthread.h>
#include <string.h>

struct buffer_state {
    char data[BUFFER_SIZE];
    int len;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int round;

    int readers_total;  /* number of reader VMs */
    int readers_left;   /* readers still to consume the current round */
};

static struct buffer_state state;

void buffer_init(int readers_total)
{
    memset(state.data, 0, sizeof(state.data));
    state.len = 0;
    state.round = 0;
    state.readers_total = readers_total;
    state.readers_left = 0;
    pthread_mutex_init(&state.lock, NULL);
    pthread_cond_init(&state.cond, NULL);
}

int buffer_write(const char *src, int count)
{
    if (count > BUFFER_SIZE)
        count = BUFFER_SIZE;

    pthread_mutex_lock(&state.lock);

    // wait for all readers to finish reading
    while (state.readers_left > 0)
        pthread_cond_wait(&state.cond, &state.lock);

    memcpy(state.data, src, count);
    state.len = count;
    state.round++;
    state.readers_left = state.readers_total;

    pthread_cond_broadcast(&state.cond);
    pthread_mutex_unlock(&state.lock);

    return count;
}

int buffer_read(char *dst, int *last_round)
{
    pthread_mutex_lock(&state.lock);

    // wait for a new round
    while (state.round == *last_round)
        pthread_cond_wait(&state.cond, &state.lock);

    *last_round = state.round;
    int n = state.len;
    memcpy(dst, state.data, n);

    pthread_mutex_unlock(&state.lock);
    return n;
}

void buffer_read_done(void)
{
    pthread_mutex_lock(&state.lock);
    if (--state.readers_left == 0)
        pthread_cond_broadcast(&state.cond);  /* wake the writer */
    pthread_mutex_unlock(&state.lock);
}
