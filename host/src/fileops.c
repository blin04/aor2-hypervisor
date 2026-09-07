#include "fileops.h"
#include "vm.h"

#include <ctype.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdlib.h>
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
    int free_fd;
    if (fd != -1) {
        for (free_fd = 0; free_fd < FOP_MAX_FILES; free_fd++)
            if (!v->files[free_fd].in_use)
                break;
        if (free_fd == FOP_MAX_FILES) {
            // error: no free slot found
            close(fd);
            return -1;
        }
        else {
            v->files[free_fd].host_fd = fd;
            v->files[free_fd].in_use = 1;
        }
    }
    else free_fd = fd;

    printf("[dbg] host_open path=%s flags=%u -> guest_fd=%d host_fd=%d\n", path, flags, free_fd, fd);
    return free_fd;
}

static int host_close(struct vm *v, int fd) {
    if (fd < 0 || fd >= FOP_MAX_FILES)
        return -1;
    int host_fd = v->files[fd].host_fd;
    int ret = close(host_fd);
    v->files[fd].in_use = 0;
    return ret;
}

static int host_read(struct vm *v, int fd, char *buf, uint32_t count) {
    if (fd < 0 || fd >= FOP_MAX_FILES)
        return -1;
    int host_fd = v->files[fd].host_fd;
    printf("[debug] reading from fd=%d\n", host_fd);
    int ret = read(host_fd, buf, count);
    return ret;
}

static int host_write(struct vm *v, int fd, const char *buf, uint32_t count) {
    if (fd < 0 || fd >= FOP_MAX_FILES)
        return -1;
    int host_fd = v->files[fd].host_fd;
    int ret = write(host_fd, buf, count);
    return ret;
}

static int host_lseek(struct vm *v, int fd, int offset, uint8_t off_flag) {
    if (fd < 0 || fd >= FOP_MAX_FILES)
        return -1;
    int host_fd = v->files[fd].host_fd;

    // translate wire seek flag into POSIX whence; offset is ignored for SEEK_END
    int whence = (off_flag == FOP_SEEK_END) ? SEEK_END : SEEK_SET;
    int ret = lseek(host_fd, (off_flag == FOP_SEEK_END) ? 0 : offset, whence);
    printf("[dbg] host_lseek guest_fd=%d host_fd=%d offset=%d off_flag=%u -> %d\n",
           fd, host_fd, offset, off_flag, ret);
    return ret;
}

static void handle_open(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_OP:
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
            file_op->state = FOP_WAIT_OP;
        default:
            break;
    }
}

static void handle_close(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_OP:
            file_op->state = FOP_CLOSE_WAIT_FD;
            break;
        case FOP_CLOSE_WAIT_FD:
            file_op->fd = data;
            file_op->state = FOP_CLOSE_SEND_STATUS;
            break;
        case FOP_CLOSE_SEND_STATUS:
            file_op->result = host_close(v, file_op->fd);
            file_op->state = FOP_WAIT_OP;
        default:
            break;
    }
}

static void handle_read(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_OP:
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
            file_op->payload = malloc(file_op->count * sizeof(char));
            file_op->result = host_read(v, file_op->fd, file_op->payload, file_op->count);

            if (file_op->result > 0) {
                file_op->count = file_op->result;
                file_op->state = FOP_READ_SEND_DATA;
            }
            else {
                free(file_op->payload);
                file_op->state = FOP_WAIT_OP;
            }

            break;
        case FOP_READ_SEND_DATA:
            file_op->result = file_op->payload[file_op->have++];
            if (file_op->have == file_op->count) {
                file_op->have = 0;
                free(file_op->payload);
                file_op->state = FOP_WAIT_OP;
            }
        default:
            break;
    }
}

static void handle_write(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_OP:
            file_op->state = FOP_WRITE_WAIT_FD;
            break;
        case FOP_WRITE_WAIT_FD:
            file_op->fd = data;
            file_op->state = FOP_WRITE_WAIT_COUNT;
            break;
        case FOP_WRITE_WAIT_COUNT:
            file_op->count = data;
            file_op->state = FOP_WRITE_WAIT_DATA;
            file_op->payload = malloc(data * sizeof(char));
            break;
        case FOP_WRITE_WAIT_DATA:
            file_op->payload[file_op->have++] = data;
            if (file_op->have == file_op->count) {
                file_op->state = FOP_WRITE_SEND_N;
                file_op->have = 0;
                file_op->result = host_write(v, file_op->fd, file_op->payload, file_op->count);
            }
            break;
        case FOP_WRITE_SEND_N:
            free(file_op->payload);
            file_op->state = FOP_WAIT_OP; 
            break;
        default:
            break;
    }
}

static void handle_lseek(struct file_operation* file_op, uint32_t data, struct vm* v) {
    switch (file_op->state) {
        case FOP_WAIT_OP:
            file_op->state = FOP_LSEEK_WAIT_FD;
            break;
        case FOP_LSEEK_WAIT_FD:
            file_op->fd = data;
            file_op->state = FOP_LSEEK_WAIT_OFFSET;
            break;
        case FOP_LSEEK_WAIT_OFFSET:
            file_op->offset = data;
            file_op->state = FOP_LSEEK_WAIT_FLAG;
            break;
        case FOP_LSEEK_WAIT_FLAG:
            file_op->off_flag = data;
            file_op->state = FOP_LSEEK_SEND_OFFSET;
            break;
        case FOP_LSEEK_SEND_OFFSET:
            file_op->result = host_lseek(v, file_op->fd, file_op->offset, file_op->off_flag);
            file_op->state = FOP_WAIT_OP;
        default:
            break;
    }
}

uint32_t file_operation_handler(struct vm* v, struct file_operation* file_op, uint32_t data) {
    if (file_op->state == FOP_WAIT_OP) {
        // no op started yet: the data byte is the operation code
        file_op->code = data;
    }
    switch (file_op->code) {
        case FOP_OPEN:
            handle_open(file_op, data, v);
            break;
        case FOP_CLOSE:
            handle_close(file_op, data, v);
            break;
        case FOP_READ:
            handle_read(file_op, data, v);
            break;
        case FOP_WRITE:
            handle_write(file_op, data, v);
            break;
        case FOP_LSEEK:
            handle_lseek(file_op, data, v);
            break;
        default:
            break;
    }
    return (uint32_t)file_op->result;
}
