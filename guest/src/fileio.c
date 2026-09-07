#include "fileio.h"
#include "io.h"
#include "utils.h"

enum {
	FOP_OPEN  = 1,
	FN_CLOSE = 2,
	FN_READ  = 3,
	FN_WRITE = 4,
	FN_LSEEK = 5,
};

int open(const char *path, int flags)
{

	int len = strlen(path);

	outb(FILE_PORT, FOP_OPEN);
	out_u32(FILE_PORT, len);
	out_bytes(FILE_PORT, path, len);
	outb(FILE_PORT, (uint8_t)flags);

	return (int)in_u32(FILE_PORT);
}

int close(int fd)
{
	outb(FILE_PORT, FN_CLOSE);
	out_u32(FILE_PORT, (uint32_t)fd);

	return (int)in_u32(FILE_PORT);
}

int read(int fd, char *buf, int count)
{
	int n;

	outb(FILE_PORT, FN_READ);
	out_u32(FILE_PORT, (uint32_t)fd);
	out_u32(FILE_PORT, (uint32_t)count);

	n = (int)in_u32(FILE_PORT);
	if (n > 0)
		in_bytes(FILE_PORT, buf, (uint32_t)n);

	return n;
}

int write(int fd, const char *buf, int count)
{
	outb(OUT_PORT, 69);
	outb(FILE_PORT, FN_WRITE);
	out_u32(FILE_PORT, (uint32_t)fd);
	out_u32(FILE_PORT, (uint32_t)count);
	out_bytes(FILE_PORT, buf, (uint32_t)count);

	return (int)in_u32(FILE_PORT);
}

int lseek(int fd, const int offset, int off_flag)
{
	outb(FILE_PORT, FN_LSEEK);
	out_u32(FILE_PORT, (uint32_t)fd);
	out_u32(FILE_PORT, (uint32_t)offset);
	outb(FILE_PORT, (uint8_t)off_flag);

	return (int)in_u32(FILE_PORT);
}
