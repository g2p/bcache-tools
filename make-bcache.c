#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET64
#define _XOPEN_SOURCE 600

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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
	       "	-C, --cache		Format a cache device\n"
	       "	-B, --bdev		Format a backing device\n"
	       "	-b, --bucket		bucket size\n"
	       "	-w, --block		block size (hard sector size of SSD, often 2k)\n"
	       "	-U			UUID\n"
	       "	    --writeback		enable writeback\n"
	       "	-h, --help		display this help and exit\n");
	exit(EXIT_FAILURE);
}

int writeback;

struct option opts[7] = {
	{ "cache",	0, NULL, 'C' },
	{ "bdev",	0, NULL, 'B' },
	{ "bucket",	1, NULL, 'b' },
	{ "block",	1, NULL, 'w' },
	{ "writeback",	0, &writeback, 0 },
	{ "help",	0, NULL, 'h' },
};

void write_sb(char *dev, struct cache_sb *sb)
{
	int fd;
	char uuid[40], set_uuid[40];

	if (sb->version > 1) {
		printf("Must specify one of -C or -B\n");
		usage();
	}

	if ((sb->bucket_size & (sb->bucket_size - 1)) ||
	    (sb->block_size  & (sb->block_size - 1))) {
		printf("Block and bucket sizes must be powers of two\n");
		exit(EXIT_FAILURE);
	}

	if (sb->bucket_size < sb->block_size) {
		printf("Bad bucket size %i\n", sb->bucket_size);
		exit(EXIT_FAILURE);
	}

	if ((fd = open(dev, O_RDWR)) == -1) {
		printf("Can't open dev %s: %s\n", dev, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (sb->version == CACHE_BACKING_DEV &&
	    writeback)
		SET_BDEV_WRITEBACK(sb, 1);
	else
		SET_BDEV_WRITEBACK(sb, 0);

	sb->offset		= SB_SECTOR;
	memcpy(sb->magic, bcache_magic, 16);
	sb->nbuckets		= getblocks(fd) / sb->bucket_size;
	sb->nr_in_set		= 1;
	sb->first_bucket	= (23 / sb->bucket_size) + 1;
	uuid_unparse(sb->uuid, uuid);
	uuid_unparse(sb->set_uuid, set_uuid);
	sb->csum = csum_set(sb);

	if (sb->nbuckets < 1 << 7) {
		printf("Not enough buckets: %ju, need %u\n",
		       sb->nbuckets, 1 << 7);
		exit(EXIT_FAILURE);
	}

	printf("UUID:			%s\n"
	       "Set UUID:		%s\n"
	       "nbuckets:		%ju\n"
	       "block_size:		%u\n"
	       "bucket_size:		%u\n"
	       "nr_in_set:		%u\n"
	       "nr_this_dev:		%u\n"
	       "first_bucket:		%u\n",
	       uuid, set_uuid,
	       sb->nbuckets,
	       sb->block_size,
	       sb->bucket_size,
	       sb->nr_in_set,
	       sb->nr_this_dev,
	       sb->first_bucket);

	if (pwrite(fd, sb, sizeof(*sb), SB_SECTOR << 9) != sizeof(*sb)) {
		perror("write error\n");
		exit(EXIT_FAILURE);
	}

	fsync(fd);
	close(fd);

	uuid_generate(sb->uuid);
}

int main(int argc, char **argv)
{
	bool written = false;
	int c;
	struct cache_sb sb;

	memset(&sb, 0, sizeof(struct cache_sb));
	sb.version = 2;
	sb.block_size = 8;
	sb.bucket_size = 1024;

	uuid_generate(sb.uuid);
	uuid_generate(sb.set_uuid);

	while ((c = getopt_long(argc, argv,
				"-hCBU:w:b:",
				opts, NULL)) != -1)
		switch (c) {
		case 'C':
			sb.version = 0;
			break;
		case 'B':
			sb.version = CACHE_BACKING_DEV;
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
		case 'h':
			usage();
			break;
		case 1:
			write_sb(optarg, &sb);
			written = true;
			break;
		}

	if (!written) {
		printf("Please supply a device\n");
		usage();
	}

	return 0;
}
