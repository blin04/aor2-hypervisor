#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t value;
	asm volatile("inb %1,%0" : "=a" (value) : "Nd" (port) : "memory");
	return value;
}

static inline void out_u32(uint16_t port, uint32_t value)
{
	asm("outl %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

static inline uint32_t in_u32(uint16_t port)
{
	uint32_t value;
	asm volatile("inl %1,%0" : "=a" (value) : "Nd" (port) : "memory");
	return value;
}

static inline void out_bytes(uint16_t port, const void *buf, uint32_t count)
{
	const uint8_t *p = (const uint8_t *)buf;
	for (uint32_t i = 0; i < count; ++i)
		outb(port, p[i]);
}

static inline void in_bytes(uint16_t port, void *buf, uint32_t count)
{
	uint8_t *p = (uint8_t *)buf;
	for (uint32_t i = 0; i < count; ++i)
		p[i] = inb(port);
}

#endif /* IO_H */
