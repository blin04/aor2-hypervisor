#include "ipc.h"
#include "io.h"

uint32_t ipc_write(const char *buf, uint32_t count)
{
	out_u32(IPC_DATA_PORT, count);
	out_bytes(IPC_DATA_PORT, buf, count);

	return in_u32(IPC_RESPONSE_PORT);
}

uint32_t ipc_read(char *buf, uint32_t cap)
{
	uint32_t avail = in_u32(IPC_DATA_PORT);
	uint32_t got   = (avail < cap) ? avail : cap;

	if (got)
		in_bytes(IPC_DATA_PORT, buf, got);

	out_u32(IPC_RESPONSE_PORT, got);
	return got;
}
