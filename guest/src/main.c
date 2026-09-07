#include "descriptors.h"
#include "fileio.h"
#include "interrupts.h"
#include "io.h"

static struct gdt_entry gdt[3];

static void print(const char *s)
{
	for (; *s; ++s)
		outb(0xE9, *s);
}

static void print_int(int value)
{
	char buf[12];               /* -2147483648 has 10 digits; sign printed separately */
	int i = 0;
	unsigned int mag;

	if (value < 0) {
		outb(0xE9, '-');
		mag = -(unsigned int)value; /* two's-complement magnitude, safe for INT_MIN */
	} else {
		mag = (unsigned int)value;
	}

	do {                        /* extract digits least-significant first */
		buf[i++] = (char)('0' + mag % 10);
		mag /= 10;
	} while (mag);

	while (i-- > 0)             /* emit most-significant first */
		outb(0xE9, buf[i]);
}

void fileio_test()
{
	int fd, status;

	/* create + open for writing */
	fd = open("text.txt", O_CREATE | O_WR);
	print(fd >= 0 ? "open create: PASS\n" : "open create: FAIL\n");
 
	status = close(fd);
	print(status == 0 ? "close: PASS\n" : "close: FAIL\n");
 
	/* re-open the same file for reading, without O_CREATE */
	fd = open("test.txt", O_RD);
	print(fd >= 0 ? "reopen: PASS\n" : "reopen: FAIL\n");
 
	status = close(fd);
	print(status == 0 ? "close after reopen: PASS\n" : "close after reopen: FAIL\n");
 
	/* open another file */
	fd = open("aaa.txt", O_CREATE | O_WR);
	print(fd >= 0 ? "reopen: PASS\n" : "reopen: FAIL\n");
 
	status = close(fd);
	print(status == 0 ? "close after reopen: PASS\n" : "close after reopen: FAIL\n");
 
	/* invalid filename must be rejected on creation */
	fd = open("1bad.txt", O_CREATE | O_WR);
	print(fd < 0 ? "invalid name rejected: PASS\n" : "invalid name rejected: FAIL\n");
	
	print("received: ");
	print_int(fd);
	print("\n");

	/* closing an fd that was never opened must fail */
	status = close(69);
	print(status < 0 ? "close bad fd: PASS\n" : "close bad fd: FAIL\n");

	print("received: ");
	print_int(status);
	print("\n");
 
	close(6969);

	// print("fileio test completed, verdict: PASS\n");
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void)
{
	struct dt_ptr p;

	gdt[0] = (struct gdt_entry){ 0 };
	gdt[1] = (struct gdt_entry){  /* 64-bit code, selector 0x08: P=1, DPL=0, S=1, type=0xA, L=1, G=1 */
		.limit_low   = 0xFFFF,
		.access      = 0x9A,
		.flags_limit = 0xAF,
	};
	gdt[2] = (struct gdt_entry){  /* 64-bit data, selector 0x10: P=1, DPL=0, S=1, type=0x2, D/B=1, G=1 */
		.limit_low   = 0xFFFF,
		.access      = 0x92,
		.flags_limit = 0xCF,
	};

	p.limit = sizeof(gdt) - 1;
	p.base  = (uint64_t)(uintptr_t)gdt;
	asm volatile("lgdt %0" : : "m"(p) : "memory");

	/* Reload CS to 0x08 and continue at label 1 */
	asm volatile(
		"pushq $0x08\n\t"
		"lea 1f(%%rip), %%rax\n\t"
		"pushq %%rax\n\t"
		"lretq\n\t"
		"1:\n\t"
		::: "rax", "memory"
	);

	/* Reload data segment selectors to 0x10 */
	asm volatile(
		"movl $0x10, %%eax\n\t"
		"movw %%ax, %%ds\n\t"
		"movw %%ax, %%es\n\t"
		"movw %%ax, %%ss\n\t"
		::: "eax", "memory"
	);

	init_idt();

	asm volatile("sti");

	print("Hello, world!\n");

	fileio_test();

	for (;;)
		asm volatile("hlt");
}
