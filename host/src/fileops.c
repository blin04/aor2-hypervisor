#include "fileops.h"
#include "vm.h"

#include <ctype.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <unistd.h>

static int host_open(struct vm *v, const char *path, uint8_t flags) {
    if (!(('A' <= path[0] && path[0] <= 'Z')
        || ('a' <= path[0] && path[0] <= 'z')))
        return -1;

    for (int i = 0; path[i] != '\0'; i++) 
        if (!isalnum(path[i]) && !(path[i] == '.'))
            return -1;
        

    // translate flags into POSIX
    // compatible values
    int real_flags;
    if (flags & FOP_O_RDWR)
        real_flags = O_RDWR;
    else if (flags & FOP_O_WR)
        real_flags = O_WRONLY;
    else
        real_flags = O_RDONLY;
    if (flags & FOP_O_CREATE)
        real_flags |= O_CREAT;


    int fd = open(path, real_flags, 0644);
    if (fd != -1) {
        int free_fd = 0;
        for (; free_fd < FOP_MAX_FILES; free_fd++)
            if (!v->files[free_fd].in_use)
                break;
        if (free_fd == FOP_MAX_FILES) {
            // error: no free slot found
            close(fd);
            return -1;
        }
        else {
            v->files[free_fd].host_fd = fd;
            v->files[fd].in_use = 1;
        }
    }
    return fd;
}

static int host_close(struct vm *v, int fd) {
    if (fd < 0 || fd >= FOP_MAX_FILES)
        return -1;
    int host_fd = v->files[fd].host_fd;
    int ret = close(host_fd);
    return ret;
}

static int host_read(struct vm *v, int fd, char *buf, uint32_t count) {
    int host_fd = v->files[fd].host_fd;
    int ret = read(host_fd, buf, count);
    return ret;
}

static int host_write(struct vm *v, int fd, const char *buf, uint32_t count) {
    int host_fd = v->files[fd].host_fd;
    int ret = write(host_fd, buf, count);
    return ret;
}

static int host_lseek(struct vm *v, int fd, int offset, uint8_t off_flag) {
    (void)v;
    (void)fd;
    (void)offset;
    (void)off_flag;
    return -1;
}

static void handle_open(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_FN:
            file_op->state = FOP_OPEN_WAIT_PATHLEN;
            break;
        case FOP_OPEN_WAIT_PATHLEN:
            file_op->path_len = data;
            file_op->state = FOP_OPEN_WAIT_PATH;
            break;
        case FOP_OPEN_WAIT_PATH:
            file_op->path[file_op->have++] = (char)data;
            if (file_op->have == file_op->path_len) {
                file_op->state = FOP_OPEN_WAIT_FLAGS;
                file_op->have = 0;
            }
            break;
        case FOP_OPEN_WAIT_FLAGS:
            file_op->flags = data;
            file_op->state = FOP_OPEN_SEND_FD;
            break;
        case FOP_OPEN_SEND_FD:
            file_op->result = host_open(v, file_op->path, file_op->flags); 
        default:
            break;
    }
}

static void handle_close(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_FN:
            file_op->state = FOP_CLOSE_WAIT_FD;
            break;
        case FOP_CLOSE_WAIT_FD:
            file_op->fd = data;
            file_op->state = FOP_CLOSE_SEND_STATUS;
            break;
        case FOP_CLOSE_SEND_STATUS:
            file_op->result = host_close(v, file_op->fd);
        default:
            break;
    }
}

static void handle_read(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_FN:
            file_op->state = FOP_READ_WAIT_FD;
            break;
        case FOP_READ_WAIT_FD:
            file_op->fd = data;
            file_op->state = FOP_READ_WAIT_COUNT;
            break;
        case FOP_READ_WAIT_COUNT:
            file_op->count = data;
            file_op->state = FOP_READ_SEND_N;
            break;
        case FOP_READ_SEND_N:
            break;
        case FOP_READ_SEND_DATA:
            break;
        default:
            break;
    }
}

static void handle_write(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_FN:
            break;
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

static void handle_lseek(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_FN:
            break;
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
    if (file_op->state == FOP_WAIT_FN) {
        // no op started yet
        // the data handled is operation code
        file_op->code = data;
    }
    switch (file_op->code) {
        case FOP_OPEN:
            handle_open(file_op, data, NULL);
            break;
        case FOP_CLOSE:
            handle_close(file_op, data, NULL);
            break;
        case FOP_READ:
            handle_read(file_op, data, NULL);
            break;
        case FOP_WRITE:
            handle_write(file_op, data, NULL);
            break;
        case FOP_LSEEK:
            handle_lseek(file_op, data, NULL);
            break;
        default:

    }
}

uint32_t file_operation_handle_in(struct vm *v, struct file_operation* file_op) {
    switch (file_op->code) {
        case FOP_OPEN:
            handle_open(file_op, 0, v);
            break;
        case FOP_CLOSE:
            handle_close(file_op, 0, v);
            break;
        default:
            return 0;
    }

    // reset file op state
    file_op->state = FOP_WAIT_FN;
    return (uint32_t)file_op->result;
}
