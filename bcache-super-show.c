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

	if (pread(fd, &sb, sizeof(sb), 4096) != sizeof(sb)) {
		fprintf(stderr, "Couldn't read\n");
		exit(2);
	}

	printf("sb.magic\t\t");
	if (! memcmp(sb.magic, bcache_magic, 16)) {
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

	printf("sb.csum\t\t\t0x%" PRIx64, sb.csum);
	expected_csum = csum_set(&sb);
	if (sb.csum == expected_csum) {
		printf(" [match]\n");
	} else {
		printf(" [expected %" PRIX64 "]\n", expected_csum);
		if (! force_csum) {
			fprintf(stderr, "Corrupt superblock (bad csum)\n");
			exit(2);
		}
	}

	printf("sb.version\t\t%" PRIu64, sb.version);
	switch (sb.version) {
		case 0:
			printf(" [cache device]\n");
			break;

		// SB_BDEV macro says bdev iff version is odd; only 0 and 1
		// seem to be fully implemented however.
		case CACHE_BACKING_DEV: // 1
			printf(" [backing device]\n");
			break;

		default:
			printf(" [unknown]\n");
			// exit code?
			return 0;
	}

	putchar('\n');

	uuid_unparse(sb.uuid, uuid);
	printf("dev.uuid\t\t%s\n", uuid);

	printf(
			"dev.sectors_per_block\t%u\n"
			"dev.sectors_per_bucket\t%u\n"
			"dev.bucket_count\t%ju\n"
			"dev.cache_count\t\t%u\n", // expect version == 0 ? 1 : 0
			sb.block_size,
			sb.bucket_size,
			sb.nbuckets,
			sb.nr_this_dev);

	if (sb.version == 0) {
		printf(
				"dev.cache.first_bucket\t%u\n"
				"dev.cache.first_sector\t%u\n"
				"dev.cache.discard\t%s\n",
				sb.first_bucket,
				sb.bucket_size * sb.first_bucket,
				CACHE_DISCARD(&sb) ? "yes" : "no");
	} else if (sb.version == CACHE_BACKING_DEV) {
		printf(
				"dev.data.first_sector\t%u\n"
				"dev.data.writeback\t%s\n",
				BDEV_DATA_START,
				BDEV_WRITEBACK(&sb) ? "yes" : "no");
	}
	putchar('\n');

	uuid_unparse(sb.set_uuid, uuid);
	printf("cset.uuid\t\t%s\n", uuid);

	printf("cset.cache_count\t%u\n\n", sb.nr_in_set);

	return 0;
}
