#include "io.h"

void print(const char *s)
{
	for (; *s; ++s)
		outb(0xE9, *s);
}

void print_int(int value)
{
	char buf[12]; 
	int i = 0;
	unsigned int mag;

	if (value < 0) {
		outb(0xE9, '-');
		mag = -(unsigned int)value;
	} else {
		mag = (unsigned int)value;
	}

	do {
		buf[i++] = (char)('0' + mag % 10);
		mag /= 10;
	} while (mag);

	while (i-- > 0)
		outb(0xE9, buf[i]);
}