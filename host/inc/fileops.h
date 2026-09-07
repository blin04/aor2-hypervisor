#ifndef FILEOPS_H
#define FILEOPS_H

#include <stdint.h>

#define FIO_MAX_PATH 256

enum {
	FN_OPEN  = 1,
	FN_CLOSE = 2,
	FN_READ  = 3,
	FN_WRITE = 4,
	FN_LSEEK = 5,
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
    char     path[FIO_MAX_PATH];
    uint32_t path_len;
    uint8_t  flags;

    /* READ/WRITE-specific */
    uint32_t count;
    char    *payload;

    /* LSEEK-specific */
    int      offset;
    uint8_t  off_flag;

};

void file_operation_handle_out(struct file_operation* file_op, uint32_t data);
uint32_t file_operation_handle_in(struct file_operation* file_op);

#endif