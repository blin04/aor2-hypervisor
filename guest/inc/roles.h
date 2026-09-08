#ifndef ROLE_H
#define ROLE_H

enum guest_role {
	ROLE_NONE = -1,
	ROLE_READ,
	ROLE_WRITE
};

void role_init(enum guest_role role);

void reader_step();

void writer_step();

#endif