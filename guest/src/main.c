#include "descriptors.h"
#include "fileio.h"
#include "interrupts.h"
#include "io.h"
#include "utils.h"

static struct gdt_entry gdt[3];

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

void write_test()
{
	print("\n------ write test ------\n");
	int fd, ret;

	fd = open("write.txt", O_CREATE | O_WR);
	if (fd < 0) {
		print("write test failed: can't open file\n");
		return;
	}

	char* str = "ja volim aor\0";
	ret = write(fd, str, strlen(str));
	if (ret < 0) {
		print("write test failed: can't write\n");
		return;
	}

	print("write test: PASS\n");
}

void read_test()
{
	print("\n------ read test ------\n");
	int fd;

	fd = open("write.txt", O_RD);
	if (fd < 0) {
		print("read test failed: can't open file\n");
		return;
	}

	char buf[100];
	int ret = read(fd, buf, 100);
	if (ret < 0) {
		print("read test failed: can't read\n");
		return;
	}
	
	print("read test got: ");
	print(buf);
	print("\n");
}

void lseek_test()
{
	print("\n------ lseek test ------\n");
	int fd, off, ret;

	/* open read+write so we can write, reposition, then read back */
	fd = open("seek.txt", O_CREATE | O_RDWR);
	if (fd < 0) {
		print("lseek test failed: can't open file\n");
		return;
	}

	char* str = "ja volim aor";
	ret = write(fd, str, strlen(str));   /* 12 bytes written, cursor now at 12 */
	if (ret < 0) {
		print("lseek test failed: can't write\n");
		return;
	}

	/* rewind to offset 3 and read 4 bytes -> expect "voli" */
	off = lseek(fd, 3, SEEK_SET);
	print("SEEK_SET 3 -> ");   print_int(off);   print(" (expect 3)\n");

	char buf[8];
	ret = read(fd, buf, 4);
	buf[ret > 0 ? ret : 0] = '\0';
	print("read 4 @off3 -> ");   print(buf);   print(" (expect voli)\n");

	/* seek to end -> expect 12 (total bytes written) */
	off = lseek(fd, 0, SEEK_END);
	print("SEEK_END -> ");   print_int(off);   print(" (expect 12)\n");

	close(fd);
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

	write_test();

	read_test();

	lseek_test();

	for (;;)
		asm volatile("hlt");
}
