#include "fileio.h"
#include "roles.h"
#include "io.h"

static int fd;

void role_init(const char* path, enum guest_role role)
{
    // int flags = (role == ROLE_READ) ? (O_CREATE | O_WR) : (O_RD);
    int flags = O_CREATE | O_WR;
    fd = open(path, flags);

    if (fd < 0) {
        print("error: failed to initialize role\n");
        return;
    }
}

void writer_step() 
{
    int ret = write(fd, "writer working\n", 15);
    if (ret < 0) {
        print("error: write failed\n");
        return;
    }
}

void reader_step()
{
    int ret = write(fd, "reader working\n", 15);
    if (ret < 0) {
        print("error: write failed\n");
        return;
    }
}