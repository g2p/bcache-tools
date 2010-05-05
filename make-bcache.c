#define _XOPEN_SOURCE 500

#include <fcntl.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

static const char bcache_magic[] = {
	0xc6, 0x85, 0x73, 0xf6, 0x4e, 0x1a, 0x45, 0xca,
	0x82, 0x65, 0xf5, 0x7f, 0x48, 0xba, 0x6d, 0x81 };

struct cache_sb {
	uint8_t  magic[16];
	uint32_t version;
	uint16_t block_size;		/* sectors */
	uint16_t bucket_size;		/* sectors */
	uint32_t journal_start;		/* buckets */
	uint32_t first_bucket;		/* start of data */
	uint64_t nbuckets;		/* device size */
	uint64_t btree_root;
	uint16_t btree_level;
};

struct bucket_disk {
	uint16_t	priority;
	uint8_t		generation;
} __attribute((packed));

char zero[4096];

int main(int argc, char **argv)
{
	long n;
	int fd, i;
	struct stat statbuf;
	struct cache_sb sb;

	if (argc < 2) {
		printf("Please supply a device\n");
		exit(EXIT_FAILURE);
	}

	fd = open(argv[1], O_RDWR);
	if (!fd) {
		perror("Can't open dev\n");
		exit(EXIT_FAILURE);
	}

	if (fstat(fd, &statbuf)) {
		perror("stat error\n");
		exit(EXIT_FAILURE);
	}
	if (!S_ISBLK(statbuf.st_mode))
		n = statbuf.st_blocks;
	else
		if (ioctl(fd, BLKGETSIZE, &n)) {
			perror("ioctl error");
			exit(EXIT_FAILURE);
		}

	memcpy(sb.magic, bcache_magic, 16);
	sb.version = 0;
	sb.block_size = 8;
	sb.bucket_size = 32;
	sb.nbuckets = n / sb.bucket_size;

	do
		sb.first_bucket = ((--sb.nbuckets * sizeof(struct bucket_disk))
				   + 4096 * 3) / (sb.bucket_size * 512) + 1;
	while ((sb.nbuckets + sb.first_bucket) * sb.bucket_size * 512
	       > statbuf.st_size);

	sb.journal_start = sb.first_bucket;

	sb.btree_root = sb.first_bucket * sb.bucket_size;
	sb.btree_level = 0;

	printf("block_size:		%u\n"
	       "bucket_size:		%u\n"
	       "journal_start:		%u\n"
	       "first_bucket:		%u\n"
	       "nbuckets:		%ju\n",
	       sb.block_size,
	       sb.bucket_size,
	       sb.journal_start,
	       sb.first_bucket,
	       sb.nbuckets);

	/* Zero out priorities */
	lseek(fd, 4096, SEEK_SET);
	for (i = 8; i < sb.first_bucket * sb.bucket_size; i++)
		if (write(fd, zero, 512) != 512)
			goto err;

	if (pwrite(fd, &sb, sizeof(sb), 4096) != sizeof(sb))
		goto err;

	exit(EXIT_SUCCESS);
err:
	perror("write error\n");
	return 1;
}
