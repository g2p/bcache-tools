/*
 * Author: Gabriel de Perthuis <g2p.code@gmail.com>
 *
 * GPLv2
 */


#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET64
#define _XOPEN_SOURCE 500

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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


static void usage()
{
	fprintf(stderr, "Usage: bcache-super-show [-f] <device>\n");
}


static bool accepted_char(char c)
{
	if ('0' <= c && c <= '9')
		return true;
	if ('A' <= c && c <= 'Z')
		return true;
	if ('a' <= c && c <= 'z')
		return true;
	if (strchr(".-_", c))
		return true;
	return false;
}

static void print_encode(char* in)
{
	for (char* pos = in; *pos; pos++)
		if (accepted_char(*pos))
			putchar(*pos);
		else
			printf("%%%x", *pos);
}


int main(int argc, char **argv)
{
	bool force_csum = false;
	int o;
	extern char *optarg;
	struct cache_sb sb;
	char uuid[40];
	uint64_t expected_csum;

	while ((o = getopt(argc, argv, "f")) != EOF)
		switch (o) {
			case 'f':
				force_csum = 1;
				break;

			default:
				usage();
				exit(1);
		}

	argv += optind;
	argc -= optind;

	if (argc != 1) {
		usage();
		exit(1);
	}

	int fd = open(argv[0], O_RDONLY);
	if (fd < 0) {
		printf("Can't open dev %s: %s\n", argv[0], strerror(errno));
		exit(2);
	}

	if (pread(fd, &sb, sizeof(sb), SB_START) != sizeof(sb)) {
		fprintf(stderr, "Couldn't read\n");
		exit(2);
	}

	printf("sb.magic\t\t");
	if (!memcmp(sb.magic, bcache_magic, 16)) {
		printf("ok\n");
	} else {
		printf("bad magic\n");
		fprintf(stderr, "Invalid superblock (bad magic)\n");
		exit(2);
	}

	printf("sb.first_sector\t\t%" PRIu64, sb.offset);
	if (sb.offset == SB_SECTOR) {
		printf(" [match]\n");
	} else {
		printf(" [expected %ds]\n", SB_SECTOR);
		fprintf(stderr, "Invalid superblock (bad sector)\n");
		exit(2);
	}

	printf("sb.csum\t\t\t%" PRIX64, sb.csum);
	expected_csum = csum_set(&sb);
	if (sb.csum == expected_csum) {
		printf(" [match]\n");
	} else {
		printf(" [expected %" PRIX64 "]\n", expected_csum);
		if (!force_csum) {
			fprintf(stderr, "Corrupt superblock (bad csum)\n");
			exit(2);
		}
	}

	printf("sb.version\t\t%" PRIu64, sb.version);
	switch (sb.version) {
		// These are handled the same by the kernel
		case BCACHE_SB_VERSION_CDEV:
		case BCACHE_SB_VERSION_CDEV_WITH_UUID:
			printf(" [cache device]\n");
			break;

		// The second adds data offset support
		case BCACHE_SB_VERSION_BDEV:
		case BCACHE_SB_VERSION_BDEV_WITH_OFFSET:
			printf(" [backing device]\n");
			break;

		default:
			printf(" [unknown]\n");
			// exit code?
			return 0;
	}

	putchar('\n');

	char label[SB_LABEL_SIZE + 1];
	strncpy(label, (char*)sb.label, SB_LABEL_SIZE);
	label[SB_LABEL_SIZE] = '\0';
	printf("dev.label\t\t");
	if (*label)
		print_encode(label);
	else
		printf("(empty)");
	putchar('\n');

	uuid_unparse(sb.uuid, uuid);
	printf("dev.uuid\t\t%s\n", uuid);

	printf("dev.sectors_per_block\t%u\n"
	       "dev.sectors_per_bucket\t%u\n",
	       sb.block_size,
	       sb.bucket_size);

	if (!SB_IS_BDEV(&sb)) {
		// total_sectors includes the superblock;
		printf("dev.cache.first_sector\t%u\n"
		       "dev.cache.cache_sectors\t%ju\n"
		       "dev.cache.total_sectors\t%ju\n"
		       "dev.cache.ordered\t%s\n"
		       "dev.cache.discard\t%s\n"
		       "dev.cache.pos\t\t%u\n"
		       "dev.cache.replacement\t%ju",
		       sb.bucket_size * sb.first_bucket,
		       sb.bucket_size * (sb.nbuckets - sb.first_bucket),
		       sb.bucket_size * sb.nbuckets,
		       CACHE_SYNC(&sb) ? "yes" : "no",
		       CACHE_DISCARD(&sb) ? "yes" : "no",
		       sb.nr_this_dev,
			   CACHE_REPLACEMENT(&sb));
		switch (CACHE_REPLACEMENT(&sb)) {
			case CACHE_REPLACEMENT_LRU:
				printf(" [lru]\n");
				break;
			case CACHE_REPLACEMENT_FIFO:
				printf(" [fifo]\n");
				break;
			case CACHE_REPLACEMENT_RANDOM:
				printf(" [random]\n");
				break;
			default:
				putchar('\n');
		}

	} else {
		uint64_t first_sector;
		if (sb.version == BCACHE_SB_VERSION_BDEV) {
			first_sector = BDEV_DATA_START_DEFAULT;
		} else {
			if (sb.keys == 1 || sb.d[0]) {
				fprintf(stderr,
					"Possible experimental format detected, bailing\n");
				exit(3);
			}
			first_sector = sb.data_offset;
		}

		printf("dev.data.first_sector\t%ju\n"
		       "dev.data.cache_mode\t%ju",
		       first_sector,
		       BDEV_CACHE_MODE(&sb));
		switch (BDEV_CACHE_MODE(&sb)) {
			case CACHE_MODE_WRITETHROUGH:
				printf(" [writethrough]\n");
				break;
			case CACHE_MODE_WRITEBACK:
				printf(" [writeback]\n");
				break;
			case CACHE_MODE_WRITEAROUND:
				printf(" [writearound]\n");
				break;
			case CACHE_MODE_NONE:
				printf(" [no caching]\n");
				break;
			default:
				putchar('\n');
		}

		printf("dev.data.cache_state\t%ju",
		       BDEV_STATE(&sb));
		switch (BDEV_STATE(&sb)) {
			case BDEV_STATE_NONE:
				printf(" [detached]\n");
				break;
			case BDEV_STATE_CLEAN:
				printf(" [clean]\n");
				break;
			case BDEV_STATE_DIRTY:
				printf(" [dirty]\n");
				break;
			case BDEV_STATE_STALE:
				printf(" [inconsistent]\n");
				break;
			default:
				putchar('\n');
		}
	}
	putchar('\n');

	uuid_unparse(sb.set_uuid, uuid);
	printf("cset.uuid\t\t%s\n", uuid);

	return 0;
}
