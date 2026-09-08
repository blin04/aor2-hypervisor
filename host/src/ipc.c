#include "ipc.h"
#include "vm.h"
#include "buffer.h"

#include <pthread.h>
#include <stdio.h>

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

static uint32_t writer_handler(struct ipc_operation *op, uint32_t data)
{
    switch (op->state) {
    case IPC_W_WAIT_COUNT:
        op->count = data;
        op->have  = 0;
        if (op->count == 0) {
            buffer_write(op->buf, 0);
            ipc_finish();
            op->accepted = 0;
            op->state = IPC_W_SEND_ACK;
        } 
        else
            op->state = IPC_W_WAIT_DATA;
        return 0;
    case IPC_W_WAIT_DATA:
        if (op->have < BUFFER_SIZE)
            op->buf[op->have++] = (char)data;

        if (op->have == op->count) {
            // blocks until all readers finished the previous round 
            op->accepted = (uint32_t)buffer_write(op->buf, (int)op->count);
            op->state = IPC_W_SEND_ACK;
        }
        return 0;
    case IPC_W_SEND_ACK:
        op->state = IPC_W_WAIT_COUNT;
        return op->accepted;
    default:
        return 0;
    }
}

static uint32_t reader_handler(struct vm *v, struct ipc_operation *op, uint32_t data)
{
    switch (op->state) {
    case IPC_R_SEND_COUNT:
        // blocks until the writer adds a new round
        op->count = (uint32_t)buffer_read(op->buf, &op->last_round);
        op->have  = 0;
        op->state = (op->count == 0) ? IPC_R_WAIT_ACK : IPC_R_SEND_DATA;
        return op->count;
    case IPC_R_SEND_DATA: {
        uint32_t b = (uint32_t)(unsigned char)op->buf[op->have++];
        if (op->have == op->count)
            op->state = IPC_R_WAIT_ACK;
        return b;
    }
    case IPC_R_WAIT_ACK:
        buffer_read_done(); 
        if (data != op->count) {
            printf("VM %d: read %u of %u bytes, stopping\n",
                   v->id, data, op->count);
            v->stop = 1;
        }
        op->state = IPC_R_SEND_COUNT;
        return 0;
    default:
        return 0;
    }
}

uint32_t ipc_handler(struct vm *v, struct ipc_operation *op, uint32_t data)
{
    if (op->state == IPC_SEND_ROLE) {
        // first interrupt handling, send the role
        op->state = (v->guest_role == GUEST_ROLE_WRITE) ? IPC_W_WAIT_COUNT : IPC_R_SEND_COUNT;
        return (uint32_t)v->guest_role;
    }

    return (v->guest_role == GUEST_ROLE_WRITE) ? writer_handler(op, data)
                                               : reader_handler(v, op, data);
}
