#ifndef FILEOPS_H
#define FILEOPS_H

#include <stdint.h>

#define FOP_MAX_PATH 256
#define FOP_MAX_FILES 16

#define FOP_O_RD     1u
#define FOP_O_WR     2u
#define FOP_O_RDWR   4u
#define FOP_O_CREATE 8u

/* wire-protocol lseek flags (must match guest/inc/fileio.h); differ from libc's
 * SEEK_SET (0) / SEEK_END (2), so translate before calling the real lseek(). */
#define FOP_SEEK_SET 1u
#define FOP_SEEK_END 2u

enum {
	FOP_OPEN  = 1,
	FOP_CLOSE = 2,
	FOP_READ  = 3,
	FOP_WRITE = 4,
	FOP_LSEEK = 5,
};

enum file_operation_state {
	FOP_WAIT_OP = 0,

	FOP_OPEN_WAIT_PATHLEN, FOP_OPEN_WAIT_PATH, FOP_OPEN_WAIT_FLAGS, FOP_OPEN_SEND_FD,

	FOP_CLOSE_WAIT_FD, FOP_CLOSE_SEND_STATUS,

	FOP_READ_WAIT_FD, FOP_READ_WAIT_COUNT, FOP_READ_SEND_N, FOP_READ_SEND_DATA,

	FOP_WRITE_WAIT_FD, FOP_WRITE_WAIT_COUNT, FOP_WRITE_WAIT_DATA, FOP_WRITE_SEND_N,

	FOP_LSEEK_WAIT_FD, FOP_LSEEK_WAIT_OFFSET, FOP_LSEEK_WAIT_FLAG, FOP_LSEEK_SEND_OFFSET,
};

struct file_operation  {
    int code;
    enum file_operation_state state;
    int      fd;
    uint32_t have;      // counter for loops
    int32_t  result;    

    // open related
    char     path[FOP_MAX_PATH];
    uint32_t path_len;
    uint8_t  flags;

    // read / write related 
    uint32_t count;
    char    *payload;

    // lseek related
    int      offset;
    uint8_t  off_flag;

};

struct file_struct {
	int host_fd;
	int in_use;
    int is_shared;
    int copied;
    char name[FOP_MAX_PATH];
    uint8_t flags;
};

struct vm;

void init_shared_files(char **paths, int n_paths);

int  is_file_shared(const char *path);

uint32_t file_operation_handler(struct vm *v, struct file_operation* file_op, uint32_t data);

#endif