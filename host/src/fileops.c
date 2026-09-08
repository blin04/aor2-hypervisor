#include "fileops.h"
#include "vm.h"

#include <ctype.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>

static char **shared_paths;

static int n_shared_paths;

static void copy_file(const char *src, const char *dst);

static void resolve_host_path(struct vm *v, const char *guest_path, int shared,
                              char *resolved_path, size_t resolved_path_sz);

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
        

    int shared = is_file_shared(path);

    // shade private files into the VM's namespace; shared files stay unshaded
    char host_path[FOP_MAX_PATH + 16];
    resolve_host_path(v, path, shared, host_path, sizeof(host_path));

    int fd = open(host_path, real_flags, 0644);
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
            v->files[free_fd].is_shared = shared;
            v->files[free_fd].copied = 0;
            v->files[free_fd].flags = flags;
            // store guest path, the on the host is always derived from it
            strncpy(v->files[free_fd].name, path, FOP_MAX_PATH - 1);
        }
    }
    else free_fd = fd;
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
    int ret = read(host_fd, buf, count);
    return ret;
}

static int host_write(struct vm *v, int fd, const char *buf, uint32_t count) {
    if (fd < 0 || fd >= FOP_MAX_FILES)
        return -1;

    if (v->files[fd].is_shared && !v->files[fd].copied) {
        // get current file position;
        off_t pos = lseek(v->files[fd].host_fd, 0, SEEK_CUR);
        close(v->files[fd].host_fd);

        char copy_path[FOP_MAX_PATH + 16];
        snprintf(copy_path, sizeof(copy_path), "vm-%d-%s", v->id, v->files[fd].name);

        copy_file(v->files[fd].name, copy_path);

        int new_fd = open(copy_path, O_RDWR);
        v->files[fd].host_fd = new_fd;
        v->files[fd].copied = 1;
        if (pos >= 0)   // restore file position
            lseek(new_fd, pos, SEEK_SET);
    }
    int host_fd = v->files[fd].host_fd;
    int ret = write(host_fd, buf, count);
    return ret;
}

static int host_lseek(struct vm *v, int fd, int offset, uint8_t off_flag) {
    if (fd < 0 || fd >= FOP_MAX_FILES)
        return -1;

    int host_fd = v->files[fd].host_fd;
    int whence = (off_flag == FOP_SEEK_END) ? SEEK_END : SEEK_SET;
    int ret = lseek(host_fd, (off_flag == FOP_SEEK_END) ? 0 : offset, whence);
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

void init_shared_files(char **paths, int n) { 
    shared_paths = paths; 
    n_shared_paths = n; 
}

int is_file_shared(const char *path) {
    for (int i = 0; i < n_shared_paths; i++)
        if (strcmp(path, shared_paths[i]) == 0)
            return 1;
    return 0;
}

static void copy_file(const char *src , const char *dst) {
    int s_fd = open(src, O_RDONLY, 0644);
    int d_fd = open(dst, O_CREAT | O_WRONLY, 0644);

    char c;
    while (read(s_fd, &c, 1))
        write(d_fd, &c, 1);

    close(s_fd);
    close(d_fd);
}

// map guest's filename to the actual file in the filesystem
//
// local guest files are shaded with `vm-<id>-` prefix in 
// order to provide isolation among guests
//
// shared files are unshaded
// when cow is peformed the local copy gets shaded accordingly
static void resolve_host_path(struct vm *v, const char *guest_path, int shared,
                              char *resolved_path, size_t resolved_path_sz) {
    if (shared)
        snprintf(resolved_path, resolved_path_sz, "%s", guest_path);
    else
        snprintf(resolved_path, resolved_path_sz, "vm-%d-%s", v->id, guest_path);
}