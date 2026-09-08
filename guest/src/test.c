#include "test.h"
#include "fileio.h"
#include "io.h"
#include "utils.h"

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

void copy_on_write_test()
{
	print("\n------ copy-on-write test ------\n");
	int fd, ret;
	char buf[128];

	fd = open("shared.txt", O_RDWR);
	if (fd < 0) {
		print("cow test failed: can't open shared file\n");
		return;
	}

	int orig_len = read(fd, buf, sizeof(buf) - 1);
	if (orig_len < 0) {
		print("cow test failed: can't read\n");
		close(fd);
		return;
	}
	buf[orig_len] = '\0';
	print("original contents: ");
	print(buf);
	print("\n");

	char* str = "\novo je nova linija\n";
	int len = strlen(str);
	ret = write(fd, str, len);
	if (ret != len) {
		print("cow test failed: can't write\n");
		close(fd);
		return;
	}

	lseek(fd, orig_len, SEEK_SET);
	char verify[32];
	int vn = read(fd, verify, len);
	verify[vn > 0 ? vn : 0] = '\0';
	print("read back: ");
	print(verify);
	print("\n");

	int ok = (vn == len);
	for (int i = 0; ok && i < vn; i++)
		if (verify[i] != str[i])
			ok = 0;
	print(ok ? "cow verify: PASS\n" : "cow verify: FAIL\n");

	close(fd);
}
