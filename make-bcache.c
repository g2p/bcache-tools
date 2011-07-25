#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET64
#define _XOPEN_SOURCE 600

#include <ctype.h>
#include <errno.h>
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

uint64_t getblocks(int fd)
{
	uint64_t ret;
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

uint64_t hatoi(const char *s)
{
	char *e;
	long long i = strtoll(s, &e, 10);
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
	       "	-w block size (hard sector size of SSD, often 2k)\n"
	       "	-U UUID\n"
	       "	-S Set UUID\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	bool cache = false, backingdev = false;
	int fd, c;
	int64_t nblocks;
	char uuid[40], set_uuid[40], *dev;
	struct cache_sb sb;

	memset(&sb, 0, sizeof(struct cache_sb));

	uuid_generate(sb.uuid);
	uuid_generate(sb.set_uuid);

	while ((c = getopt(argc, argv, "CBU:w:b:j:")) != -1)
		switch (c) {
		case 'C':
			cache = true;
			break;
		case 'B':
			backingdev = true;
			break;
		case 'b':
			sb.bucket_size = hatoi(optarg) / 512;
			break;
		case 'w':
			sb.block_size = hatoi(optarg) / 512;
			break;
		case 'U':
			if (uuid_parse(optarg, sb.uuid)) {
				printf("Bad uuid\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'S':
			if (uuid_parse(optarg, sb.set_uuid)) {
				printf("Bad uuid\n");
				exit(EXIT_FAILURE);
			}
			break;
		}

	if (!sb.block_size)
		sb.block_size = 4;

	if (!sb.bucket_size)
		sb.bucket_size = cache ? 256 : 8192;

	if (cache == backingdev) {
		printf("Must specify one of -C or -B\n");
		usage();
	}

	if (argc <= optind) {
		printf("Please supply a device\n");
		exit(EXIT_FAILURE);
	}

	dev = argv[optind];
	fd = open(dev, O_RDWR);
	if (fd == -1) {
		printf("Can't open dev %s: %s\n", dev, strerror(errno));
		exit(EXIT_FAILURE);
	}
	nblocks = getblocks(fd);
	printf("device is %ju sectors\n", nblocks);

	sb.offset_this_sb	= 8;
	memcpy(sb.magic, bcache_magic, 16);
	sb.version		= backingdev ? CACHE_BACKING_DEV : 0;
	sb.nbuckets		= nblocks / sb.bucket_size;
	sb.nr_in_set		= 1;
	uuid_unparse(sb.uuid, uuid);
	uuid_unparse(sb.set_uuid, set_uuid);

	sb.first_bucket = (23 / sb.bucket_size) + 1;

	if (cache)
		if (sb.bucket_size < sb.block_size ||
		    sb.bucket_size > nblocks / 8) {
			printf("Bad bucket size %i\n", sb.bucket_size);
			exit(EXIT_FAILURE);
		}

	printf("UUID:			%s\n"
	       "Set UUID:		%s\n"
	       "nbuckets:		%ju\n"
	       "block_size:		%u\n"
	       "bucket_size:		%u\n"
	       "nr_in_set:		%u\n"
	       "nr_this_dev:		%u\n"
	       "first_bucket:		%u\n"
	       "sizeof sb:		%lu\n",
	       uuid, set_uuid,
	       sb.nbuckets,
	       sb.block_size,
	       sb.bucket_size,
	       sb.nr_in_set,
	       sb.nr_this_dev,
	       sb.first_bucket,
	       sizeof(sb));

	if (pwrite(fd, &sb, sizeof(sb), 4096) != sizeof(sb))
		goto err;

	fsync(fd);
	exit(EXIT_SUCCESS);
err:
	perror("write error\n");
	return 1;
}
