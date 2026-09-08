#ifndef BUFFER_H
#define BUFFER_H

#define BUFFER_SIZE 1024

void buffer_init(int readers_total);

int buffer_write(const char *src, int count);

int buffer_read(char *dst, int *last_round);

void buffer_read_done(void);

#endif
