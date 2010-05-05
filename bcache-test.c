#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#define Pread(fd, buf, size, offset) do {				\
	int _read = 0, _r;						\
	while (_read < size) {						\
		_r = pread(fd, buf, (size) - _read, (offset) + _read);	\
		if (_r <= 0)						\
			goto err;					\
		_read += _r;						\
	}								\
} while (0)

/* Marsaglia polar method
 */
double normal()
{
	double x, y, s;
	static double n = 0 / (double) 0;

	if (n == n) {
		x = n;
		n = 0 / (double) 0;
		return x;
	}
	
	do {
		x = random() / (double) (RAND_MAX / 2) - 1;
		y = random() / (double) (RAND_MAX / 2) - 1;

		s = x * x + y * y;
	} while (s >= 1);

	s = sqrt(-2 * log(s) / s);
	n =	y * s;
	return  x * s;
}

long getblocks(int fd)
{
	long ret;
	struct stat statbuf;
	if (fstat(fd, &statbuf)) {
		perror("stat error\n");
		exit(EXIT_FAILURE);
	}
	ret = statbuf.st_blocks;
	if (S_ISBLK(statbuf.st_mode))
		if (ioctl(fd, BLKGETSIZE, &ret)) {
			perror("ioctl error");
			exit(EXIT_FAILURE);
		}
	return ret;
}

int main(int argc, char **argv)
{
	bool walk = false, randsize = false, verbose = false;
	int fd1, fd2, direct = 0;
	long size, i;

	if (argc < 3) {
		printf("Please enter a cache device and raw device\n");
		exit(EXIT_FAILURE);
	}

	for (i = 3; i < argc; i++) {
		if (strcmp(argv[i], "direct") == 0)
			direct = O_DIRECT;
		if (strcmp(argv[i], "walk") == 0)
			walk = true;
		if (strcmp(argv[i], "verbose") == 0)
			verbose = true;
		if (strcmp(argv[i], "size") == 0)
			randsize = true;
	}

	fd1 = open(argv[1], O_RDONLY|direct);
	fd2 = open(argv[2], O_RDONLY|direct);
	if (fd1 == -1 || fd2 == -1) {
		perror("Error opening device");
		exit(EXIT_FAILURE);
	}

	size = MIN(getblocks(fd1), getblocks(fd2)) / 8;
	printf("size %li\n", size);

	for (i = 0;; i++) {
		char buf1[4096 * 16], buf2[4096 * 16];
		long offset;
		int pages = randsize ? MAX(MIN(abs(normal()) * 4, 16), 1) : 1;

		offset = walk ? offset * normal() * 2 : random();
		offset %= size;

		if (verbose)
			printf("Loop %li offset %li\n", i, offset);
		else if (!(i % 100))
			printf("Loop %li\n", i);

		Pread(fd1, buf1, 4096 * pages, offset << 12);
		Pread(fd2, buf2, 4096 * pages, offset << 12);

		if (memcmp(buf1, buf2, 4096 * pages)) {
			printf("Bad read! offset %li", offset << 12);
			exit(EXIT_FAILURE);
		}
	}
err:
	perror("Read error");
	exit(EXIT_FAILURE);
}
