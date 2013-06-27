/*
 * Author: Kent Overstreet <kmo@daterainc.com>
 *
 * GPLv2
 */

#define _FILE_OFFSET_BITS	64
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <openssl/rc4.h>
#include <openssl/md4.h>

static const unsigned char bcache_magic[] = {
	0xc6, 0x85, 0x73, 0xf6, 0x4e, 0x1a, 0x45, 0xca,
	0x82, 0x65, 0xf5, 0x7f, 0x48, 0xba, 0x6d, 0x81 };

unsigned char zero[4096];

bool klog = false;

#define Pread(fd, buf, size, offset) do {				\
	int _read = 0, _r;						\
	while (_read < size) {						\
		_r = pread(fd, buf, (size) - _read, (offset) + _read);	\
		if (_r <= 0)						\
			goto err;					\
		_read += _r;						\
	}								\
} while (0)

#define Pwrite(fd, buf, size, offset) do {				\
	int _write = 0, _r;						\
	while (_write < size) {						\
		_r = pwrite(fd, buf, (size) - _write, offset + _write);	\
		if (_r < 0)						\
			goto err;					\
		_write += _r;						\
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
		perror("stat error");
		exit(EXIT_FAILURE);
	}
	ret = statbuf.st_size / 512;
	if (S_ISBLK(statbuf.st_mode))
		if (ioctl(fd, BLKGETSIZE, &ret)) {
			perror("ioctl error");
			exit(EXIT_FAILURE);
		}
	return ret;
}

struct pagestuff {
	unsigned char csum[16];
	unsigned char oldcsum[16];
	int readcount;
	int writecount;
};

void flushlog(void)
{
	char logbuf[1 << 21];
	int w = 0, len;
	static int fd;

	if (!klog)
		return;

	if (!fd) {
		klogctl(8, 0, 6);

		sprintf(logbuf, "log.%i", abs(random()) % 1000);
		fd = open(logbuf, O_WRONLY|O_CREAT|O_TRUNC, 0644);

		if (fd == -1) {
			perror("Error opening log file");
			exit(EXIT_FAILURE);
		}
	}

	len = klogctl(4, logbuf, 1 << 21);

	if (len == -1) {
		perror("Error reading kernel log");
		exit(EXIT_FAILURE);
	}

	while (w < len) {
		int r = write(fd, logbuf + w, len - w);
		if (r == -1) {
			perror("Error writing log");
			exit(EXIT_FAILURE);
		}
		w += r;
	}
}

void aio_loop(int nr)
{

}

void usage()
{
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	bool walk = false, randsize = false, verbose = false, csum = false, rtest = false, wtest = false;
	int fd1, fd2 = 0, direct = 0, nbytes = 4096, j, o;
	unsigned long size, i, offset = 0, done = 0, unique = 0, benchmark = 0;
	void *buf1 = NULL, *buf2 = NULL;
	struct pagestuff *pages, *p;
	unsigned char c[16];
	time_t last_printed = 0;
	extern char *optarg;

	RC4_KEY writedata;
	RC4_set_key(&writedata, 16, bcache_magic);

	while ((o = getopt(argc, argv, "dnwvscwlb:")) != EOF)
		switch (o) {
		case 'd':
			direct = O_DIRECT;
			break;
		case 'n':
			walk = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 's':
			randsize = true;
			break;
		case 'c':
			csum = true;
			break;
		case 'w':
			wtest = true;
			break;
		case 'r':
			rtest = true;
			break;
		case 'l':
			klog = true;
			break;
		case 'b':
			benchmark = atol(optarg);
			break;
		default:
			usage();
		}

	argv += optind;
	argc -= optind;

	if (!rtest && !wtest)
		rtest = true;

	if (argc < 1) {
		printf("Please enter a device to test\n");
		exit(EXIT_FAILURE);
	}

	if (!csum && !benchmark && argc < 2) {
		printf("Please enter a device to compare against\n");
		exit(EXIT_FAILURE);
	}

	fd1 = open(argv[0], (wtest ? O_RDWR : O_RDONLY)|direct);
	if (!csum && !benchmark)
		fd2 = open(argv[1], (wtest ? O_RDWR : O_RDONLY)|direct);

	if (fd1 == -1 || fd2 == -1) {
		perror("Error opening device");
		exit(EXIT_FAILURE);
	}

	size = getblocks(fd1);
	if (!csum && !benchmark)
		size = MIN(size, getblocks(fd2));

	size = size / 8 - 16;
	pages = calloc(size + 16, sizeof(*pages));
	printf("size %li\n", size);

	if (posix_memalign(&buf1, 4096, 4096 * 16) ||
	    posix_memalign(&buf2, 4096, 4096 * 16)) {
		printf("Could not allocate buffers\n");
		exit(EXIT_FAILURE);
	}
	//setvbuf(stdout, NULL, _IONBF, 0);

	for (i = 0; !benchmark || i < benchmark; i++) {
		bool writing = (wtest && (i & 1)) || !rtest;
		nbytes = randsize ? drand48() * 16 + 1 : 1;
		nbytes <<= 12;

		offset >>= 12;
		offset += walk ? normal() * 20 : random();
		offset %= size;
		offset <<= 12;

		if (!(i % 200))
			flushlog();

		if (!verbose) {
			time_t now = time(NULL);
			if (now - last_printed >= 2) {
				last_printed = now;
				goto print;
			}
		} else
print:			printf("Loop %6li offset %9li sectors %3i, %6lu mb done, %6lu mb unique\n",
			       i, offset >> 9, nbytes >> 9, done >> 11, unique >> 11);

		done += nbytes >> 9;

		if (!writing)
			Pread(fd1, buf1, nbytes, offset);
		if (!writing && !csum && !benchmark)
			Pread(fd2, buf2, nbytes, offset);

		for (j = 0; j < nbytes; j += 4096) {
			p = &pages[(offset + j) / 4096];

			if (writing)
				RC4(&writedata, 4096, zero, buf1 + j);

			if (csum) {
				MD4(buf1 + j, 4096, &c[0]);

				if (writing ||
				    (!p->readcount && !p->writecount)) {
					memcpy(&p->oldcsum[0], &p->csum[0], 16);
					memcpy(&p->csum[0], c, 16);
				} else if (memcmp(&p->csum[0], c, 16))
					goto bad;
			} else if (!writing && !benchmark &&
				   memcmp(buf1 + j,
					  buf2 + j,
					  4096))
				goto bad;

			if (!p->writecount && !p->readcount)
				unique += 8;

			writing ? p->writecount++ : p->readcount++;
		}
		if (writing)
			Pwrite(fd1, buf1, nbytes, offset);
		if (writing && !csum && !benchmark)
			Pwrite(fd2, buf2, nbytes, offset);
	}
	printf("Loop %6li offset %9li sectors %3i, %6lu mb done, %6lu mb unique\n",
	       i, offset >> 9, nbytes >> 9, done >> 11, unique >> 11);
	exit(EXIT_SUCCESS);
err:
	perror("IO error");
	flushlog();
	exit(EXIT_FAILURE);
bad:
	printf("Bad read! loop %li offset %li readcount %i writecount %i\n",
	       i, (offset + j) >> 9, p->readcount, p->writecount);

	if (!memcmp(&p->oldcsum[0], c, 16))
		printf("Matches previous csum\n");

	flushlog();
	exit(EXIT_FAILURE);
}
