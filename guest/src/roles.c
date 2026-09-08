#include "fileio.h"
#include "roles.h"
#include "io.h"

static int fd;

void role_init(const char* path, enum guest_role role)
{
    int flags = (role == ROLE_READ) ? (O_CREATE | O_WR) : (O_RD);
    fd = open(path, flags);

    if (fd < 0) {
        print("error: failed to initialize role\n");
        return;
    }
}

void writer_step() 
{

}

void reader_step()
{

}