#include "fileops.h"
#include "vm.h"

#include <linux/kvm.h>

static void handle_open(struct file_operation* file_op, uint32_t data) {
    switch (file_op->state) {
        case FOP_OPEN_WAIT_PATHLEN:
            file_op->path_len = data;
            file_op->state = FOP_OPEN_WAIT_PATH;
            break;
        case FOP_OPEN_WAIT_PATH:
            file_op->path[file_op->have++] = (char)data;
            if (file_op->have == file_op->path_len)
                file_op->state = FOP_OPEN_WAIT_FLAGS;
            break;
        case FOP_OPEN_WAIT_FLAGS:
            file_op->flags = data;
            file_op->state = FOP_OPEN_SEND_FD;
            break;
        case FOP_OPEN_SEND_FD:
            break;
        default:
            break;
    }
}

static void handle_close(struct file_operation* file_op, uint32_t data) {
    switch (file_op->state) {
        case FOP_CLOSE_WAIT_FD:
            file_op->fd = data;
            file_op->state = FOP_CLOSE_SEND_STATUS;
            break;
        case FOP_CLOSE_SEND_STATUS:
            break;
        default:
            break;
    }
}

static void handle_read(struct file_operation* file_op, uint32_t data) {
    switch (file_op->state) {
        case FOP_READ_WAIT_FD:
            break;
        case FOP_READ_WAIT_COUNT:
            break;
        case FOP_READ_SEND_N:
            break;
        case FOP_READ_SEND_DATA:
            break;
        default:
            break;
    }
}

static void handle_write(struct file_operation* file_op, uint32_t data) {
    switch (file_op->state) {
        case FOP_WRITE_WAIT_FD:
            break;
        case FOP_WRITE_WAIT_COUNT:
            break;
        case FOP_WRITE_WAIT_DATA:
            break;
        case FOP_WRITE_SEND_N:
            break;
        default:
            break;
    }
}

static void handle_lseek(struct file_operation* file_op, uint32_t data) {
    switch (file_op->state) {
        case FOP_LSEEK_WAIT_FD:
            break;
        case FOP_LSEEK_WAIT_OFFSET:
            break;
        case FOP_LSEEK_WAIT_FLAG:
            break;
        case FOP_LSEEK_SEND_OFFSET:
            break;
        default:
            break;
    }
}

void file_operation_handle_out(struct file_operation* file_op, uint32_t data) {
    switch (file_op->code) {
        case FN_OPEN:
            handle_open(file_op, data);
            break;
        case FN_CLOSE:
            handle_close(file_op, data);
            break;
        case FN_READ:
            handle_read(file_op, data);
            break;
        case FN_WRITE:
            handle_write(file_op, data);
            break;
        case FN_LSEEK:
            handle_lseek(file_op, data);
            break;
        default:
            // no op started yet
            // the data handled is operation code
            file_op->code = data;
    }
}

uint32_t file_operation_handle_in(struct file_operation* file_op) {
    switch (file_op->state) {
        case FOP_OPEN_SEND_FD:
            file_op->state = FOP_WAIT_FN;
            return (uint32_t)file_op->result;
        case FOP_CLOSE_SEND_STATUS:
            file_op->state = FOP_WAIT_FN;
            return (uint32_t)file_op->result;
        default:
            return 0;
    }
}
