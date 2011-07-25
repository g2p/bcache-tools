#ifndef _BCACHE_H
#define _BCACHE_H

static const char bcache_magic[] = {
	0xc6, 0x85, 0x73, 0xf6, 0x4e, 0x1a, 0x45, 0xca,
	0x82, 0x65, 0xf5, 0x7f, 0x48, 0xba, 0x6d, 0x81 };

#define SB_LABEL_SIZE	32

struct cache_sb {
	uint64_t	csum;
	uint64_t	offset_this_sb;
	uint64_t	version;
#define CACHE_BACKING_DEV	1

	uint8_t		magic[16];

	uint8_t		uuid[16];
	uint8_t		set_uuid[16];
	uint8_t		label[SB_LABEL_SIZE];

#define CACHE_SYNC		(1U << 0)

#define BDEV_WRITEBACK_BIT		0U

#define BDEV_STATE_NONE		0U
#define BDEV_STATE_CLEAN	1U
#define BDEV_STATE_DIRTY	2U
#define BDEV_STATE_STALE	3U
	uint64_t	flags;
	uint64_t	sequence;
	uint64_t	pad[8];

	uint64_t	nbuckets;	/* device size */
	uint16_t	block_size;	/* sectors */
	uint16_t	bucket_size;	/* sectors */

	uint16_t	nr_in_set;
	uint16_t	nr_this_dev;

	uint32_t	last_mount;	/* time_t */

	uint16_t	first_bucket;
	uint16_t	njournal_buckets;
	uint64_t	journal_buckets[];
};

struct bkey {
	uint64_t	header;
	uint64_t	key;
	uint64_t	ptr[];
};

struct bucket_disk {
	uint16_t	priority;
	uint8_t		generation;
} __attribute((packed));

#endif
