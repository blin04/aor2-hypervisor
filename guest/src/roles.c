#include "fileio.h"
#include "roles.h"
#include "io.h"
#include "ipc.h"

static int fd;

void role_init(enum guest_role role)
{
    if (role == ROLE_READ)
        fd = open(IPC_OUT_PATH, O_CREATE | O_WR);
    else
        fd = open(IPC_FILE_PATH, O_RD);

    if (fd < 0) {
        print("error: failed to initialize ");
        if (role == ROLE_READ) print("reader\n");
        else print ("writer\n");
    }
}

void writer_step()
{
    static int done = 0;
    if (done)
        return;

    char buf[IPC_CHUNK_SIZE];
    int n = read(fd, buf, IPC_CHUNK_SIZE);
    if (n <= 0) {                 /* end of input: publish a terminal round */
        ipc_write(buf, 0);
        close(fd);
        done = 1;
        return;
    }

    ipc_write(buf, (uint32_t)n);
}

void reader_step()
{
    static int done = 0;
    if (done)
        return;

    char buf[IPC_CHUNK_SIZE];
    uint32_t n = ipc_read(buf, IPC_CHUNK_SIZE);
    if (n == 0) {
        close(fd);
        done = 1;
        return;
    }

    write(fd, buf, (int)n);
}
