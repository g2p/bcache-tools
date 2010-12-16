#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET64
#define _XOPEN_SOURCE 500

#include <fcntl.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "bcache.h"

char zero[4096];

long getblocks(int fd)
{
	long ret;
	struct stat statbuf;
	if (fstat(fd, &statbuf)) {
		perror("stat error\n");
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

long hatoi(const char *s)
{
	char *e;
	long long i = strtol(s, &e, 10);
	switch (*e) {
		case 't':
		case 'T':
			i *= 1024;
		case 'g':
		case 'G':
			i *= 1024;
		case 'm':
		case 'M':
			i *= 1024;
		case 'k':
		case 'K':
			i *= 1024;
	}
	return i;
}

void usage()
{
	printf("Usage: make-bcache [options] device\n"
	       "	-C Format a cache device\n"
	       "	-B Format a backing device\n"
	       "	-b bucket size\n"
	       "	-U UUID\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	bool cache = false, backingdev = false;
	int64_t nblocks, bucketsize = 0, blocksize = 8;
	int fd, i, c;
	struct cache_sb sb;
	char uuid[40];

	uuid_generate(sb.uuid);

	while ((c = getopt(argc, argv, "CBU:b:")) != -1)
		switch (c) {
		case 'C':
			cache = true;
			break;
		case 'B':
			backingdev = true;
			break;
		case 'b':
			bucketsize = hatoi(optarg) / 512;
			break;
		case 'U':
			if (uuid_parse(optarg, sb.uuid)) {
				printf("Bad uuid\n");
				exit(EXIT_FAILURE);
			}
			break;
		}

	if (!bucketsize)
		bucketsize = cache ? 256 : 8192;

	if (cache == backingdev) {
		printf("Must specify one of -C or -B\n");
		exit(EXIT_FAILURE);
	}

	if (argc <= optind) {
		printf("Please supply a device\n");
		exit(EXIT_FAILURE);
	}

	fd = open(argv[optind], O_RDWR);
	if (fd == -1) {
		perror("Can't open dev\n");
		exit(EXIT_FAILURE);
	}
	nblocks = getblocks(fd);
	printf("device is %li sectors\n", nblocks);

	if (bucketsize < blocksize ||
	    bucketsize > nblocks / 8) {
		printf("Bad bucket size %li\n", bucketsize);
		exit(EXIT_FAILURE);
	}

	memcpy(sb.magic, bcache_magic, 16);
	sb.version = backingdev ? CACHE_BACKING_DEVICE : 0;
	sb.block_size = blocksize;
	sb.bucket_size = bucketsize;
	sb.nbuckets = nblocks / sb.bucket_size;
	uuid_unparse(sb.uuid, uuid);

	do
		sb.first_bucket = ((--sb.nbuckets * sizeof(struct bucket_disk)) + (24 << 9)) / (sb.bucket_size << 9) + 1;
	while ((sb.nbuckets + sb.first_bucket) * sb.bucket_size > nblocks);

	sb.journal_start = sb.first_bucket;

	sb.btree_root = sb.first_bucket * sb.bucket_size;
	sb.btree_level = 0;

	printf("block_size:		%u\n"
	       "bucket_size:		%u\n"
	       "journal_start:		%u\n"
	       "first_bucket:		%u\n"
	       "nbuckets:		%ju\n"
	       "UUID:			%s\n",
	       sb.block_size,
	       sb.bucket_size,
	       sb.journal_start,
	       sb.first_bucket,
	       sb.nbuckets,
	       uuid);

	/* Zero out priorities */
	lseek(fd, 4096, SEEK_SET);
	for (i = 8; i < sb.first_bucket * sb.bucket_size; i++)
		if (write(fd, zero, 512) != 512)
			goto err;

	if (pwrite(fd, &sb, sizeof(sb), 4096) != sizeof(sb))
		goto err;

	fsync(fd);
	exit(EXIT_SUCCESS);
err:
	perror("write error\n");
	return 1;
}
