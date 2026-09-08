#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#include "buffer.h"

struct vm;

enum ipc_operation_state {
	IPC_SEND_ROLE,		// first handling: hand the guest its assigned role
	IPC_W_WAIT_COUNT,
	IPC_W_WAIT_DATA,
	IPC_W_SEND_ACK,
	IPC_R_SEND_COUNT,
	IPC_R_SEND_DATA,
	IPC_R_WAIT_ACK
};

struct ipc_operation {
	enum ipc_operation_state state;
	uint32_t count;
	uint32_t have;
	uint32_t accepted;
	int      last_round;
	char     buf[BUFFER_SIZE];
};

// handle one IPC port access from the guest
// if OUT, data holds what the guest sent
// if IN, data is 0
uint32_t ipc_handler(struct vm *v, struct ipc_operation *op, uint32_t data);

void ipc_finish(void);

int ipc_is_finished(void);

#endif
