/*
 * Author: Kent Overstreet <kmo@daterainc.com>
 *
 * GPLv2
 */

#define _FILE_OFFSET_BITS	64
#define __USE_FILE_OFFSET64
#define _XOPEN_SOURCE 500

#include <blkid.h>
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

int main(int argc, char **argv)
{
	bool udev = false;
	int i, o;
	extern char *optarg;
	struct cache_sb sb;
	char uuid[40];
	blkid_probe pr;

	while ((o = getopt(argc, argv, "o:")) != EOF)
		switch (o) {
		case 'o':
			if (strcmp("udev", optarg)) {
				printf("Invalid output format %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			udev = true;
			break;
		}


	argv += optind;
	argc -= optind;

	for (i = 0; i < argc; i++) {
		int fd = open(argv[i], O_RDONLY);
		if (fd == -1)
			continue;

		if (!(pr = blkid_new_probe()))
			continue;
		if (blkid_probe_set_device(pr, fd, 0, 0))
			continue;
		/* probe partitions too */
		if (blkid_probe_enable_partitions(pr, true))
			continue;
		/* bail if anything was found
		 * probe-bcache isn't needed once blkid recognizes bcache */
		if (!blkid_do_probe(pr)) {
			continue;
		}

		if (pread(fd, &sb, sizeof(sb), SB_START) != sizeof(sb))
			continue;

		if (memcmp(sb.magic, bcache_magic, 16))
			continue;

		uuid_unparse(sb.uuid, uuid);

		if (udev)
			printf("ID_FS_UUID=%s\n"
			       "ID_FS_UUID_ENC=%s\n"
			       "ID_FS_TYPE=bcache\n",
			       uuid, uuid);
		else
			printf("%s: UUID=\"\" TYPE=\"bcache\"\n", uuid);
	}

	return 0;
}
