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
	int fd1, fd2, direct = 0, nbytes = 4096, j;
	unsigned long size, i = 0, offset = 0;
	void *buf1 = NULL, *buf2 = NULL;

	if (argc < 3) {
		printf("Please enter a cache device and raw device\n");
		exit(EXIT_FAILURE);
	}

	for (i = 3; i < argc; i++) {
		if (strcmp(argv[i], "direct") == 0)
			direct = O_DIRECT;
		else if (strcmp(argv[i], "walk") == 0)
			walk = true;
		else if (strcmp(argv[i], "verbose") == 0)
			verbose = true;
		else if (strcmp(argv[i], "size") == 0)
			randsize = true;
		else {
			printf("Uknown argument %s\n", argv[i]);
			exit(EXIT_FAILURE);
		}
	}

	fd1 = open(argv[1], O_RDONLY|direct);
	fd2 = open(argv[2], O_RDONLY|direct);
	if (fd1 == -1 || fd2 == -1) {
		perror("Error opening device");
		exit(EXIT_FAILURE);
	}

	size = MIN(getblocks(fd1), getblocks(fd2)) / 8 - 16;
	printf("size %li\n", size);

	if (posix_memalign(&buf1, 4096, 4096 * 16) ||
	    posix_memalign(&buf2, 4096, 4096 * 16)) {
		printf("Could not allocate buffers\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		if (randsize)
			nbytes = 4096 * (int) (drand48() * 16 + 1);

		offset += walk ? normal() * 60 : random();
		offset %= size;
		assert(offset < size);

		do {
			if (verbose)
				printf("Loop %li offset %li sectors %i\n",
				       i, offset << 3, nbytes >> 9);
			else if (!(i % 100))
				printf("Loop %li\n", i);

			Pread(fd1, buf1, nbytes, offset << 12);
			Pread(fd2, buf2, nbytes, offset << 12);

			for (j = 0; j < nbytes; j += 512)
				if (memcmp(buf1 + j,
					   buf2 + j,
					   512)) {
					printf("Bad read! offset %li sectors %i, sector %i\n",
					       offset << 3, nbytes >> 9, j >> 9);
					exit(EXIT_FAILURE);
				}
		} while (!(i++ & 1));
	}
err:
	perror("Read error");
	exit(EXIT_FAILURE);
}
