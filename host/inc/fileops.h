#ifndef FILEOPS_H
#define FILEOPS_H

#include <stdint.h>

#define FOP_MAX_PATH 256
#define FOP_MAX_FILES 16

/*
 * wire-protocol flag bits (must match guest/inc/fileio.h). Kept distinct from
 * <fcntl.h>'s O_RDWR/O_CREAT etc. since the numeric values don't match -
 * FOP_O_RDWR is 4 here but O_RDWR is 2 in libc, for example.
 */
#define FOP_O_RD     1u
#define FOP_O_WR     2u
#define FOP_O_RDWR   4u
#define FOP_O_CREATE 8u

enum {
	FOP_OPEN  = 1,
	FOP_CLOSE = 2,
	FOP_READ  = 3,
	FOP_WRITE = 4,
	FOP_LSEEK = 5,
};

enum file_operation_state {
	FOP_WAIT_FN = 0,

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
    uint32_t have;      /* progress counter for whichever byte-loop is active */
    int32_t  result;    /* fd / status / n / new_offset, once computed */

    /* OPEN-specific */
    char     path[FOP_MAX_PATH];
    uint32_t path_len;
    uint8_t  flags;

    /* READ/WRITE-specific */
    uint32_t count;
    char    *payload;

    /* LSEEK-specific */
    int      offset;
    uint8_t  off_flag;

};

/* per-VM open-file table entry; guest-visible fd is the index into struct vm::files */
struct file_struct {
	int host_fd;
	int in_use;
};

struct vm; /* defined in vm.h, which includes this header for struct fileio_file */

void file_operation_handle_out(struct file_operation* file_op, uint32_t data);
uint32_t file_operation_handle_in(struct vm *v, struct file_operation* file_op);

#endif