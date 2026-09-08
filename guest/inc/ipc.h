#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#define IPC_DATA_PORT	    0x510
#define IPC_RESPONSE_PORT	0x520
#define IPC_CHUNK_SIZE	    64		// bytes sent per round

#define IPC_FILE_PATH	"ipc.txt"
#define IPC_OUT_PATH	"out.txt"


// writes up to CHUNK_SIZE bytes from buf
uint32_t ipc_write(const char *buf, uint32_t count);

// writes up to CHUNK_SIZE bytes to buf
uint32_t ipc_read(char *buf);

#endif