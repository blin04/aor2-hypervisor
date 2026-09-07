#include "fileio.h"
#include "io.h"

#define FILEIO_PORT 0x278

enum {
	FN_OPEN  = 1,
	FN_CLOSE = 2,
	FN_READ  = 3,
	FN_WRITE = 4,
	FN_LSEEK = 5,
};

int open(const char *path, int flags)
{
	uint32_t len = 0;
	while (path[len])
		++len;

	outb(FILEIO_PORT, FN_OPEN);
	out_u32(FILEIO_PORT, len);
	out_bytes(FILEIO_PORT, path, len);
	outb(FILEIO_PORT, (uint8_t)flags);

	return (int)in_u32(FILEIO_PORT);
}

int close(int fd)
{
	outb(FILEIO_PORT, FN_CLOSE);
	out_u32(FILEIO_PORT, (uint32_t)fd);

	return (int)in_u32(FILEIO_PORT);
}

int read(int fd, char *buf, int count)
{
	int n;

	outb(FILEIO_PORT, FN_READ);
	out_u32(FILEIO_PORT, (uint32_t)fd);
	out_u32(FILEIO_PORT, (uint32_t)count);

	n = (int)in_u32(FILEIO_PORT);
	if (n > 0)
		in_bytes(FILEIO_PORT, buf, (uint32_t)n);

	return n;
}

int write(int fd, const char *buf, int count)
{
	outb(FILEIO_PORT, FN_WRITE);
	out_u32(FILEIO_PORT, (uint32_t)fd);
	out_u32(FILEIO_PORT, (uint32_t)count);
	out_bytes(FILEIO_PORT, buf, (uint32_t)count);

	return (int)in_u32(FILEIO_PORT);
}

int lseek(int fd, const int offset, int off_flag)
{
	outb(FILEIO_PORT, FN_LSEEK);
	out_u32(FILEIO_PORT, (uint32_t)fd);
	out_u32(FILEIO_PORT, (uint32_t)offset);
	outb(FILEIO_PORT, (uint8_t)off_flag);

	return (int)in_u32(FILEIO_PORT);
}
