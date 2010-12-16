#ifndef _BCACHE_H
#define _BCACHE_H

static const char bcache_magic[] = {
	0xc6, 0x85, 0x73, 0xf6, 0x4e, 0x1a, 0x45, 0xca,
	0x82, 0x65, 0xf5, 0x7f, 0x48, 0xba, 0x6d, 0x81 };

struct cache_sb {
	uint8_t		magic[16];
#define CACHE_CLEAN		1
#define CACHE_SYNC		2
#define CACHE_BACKING_DEVICE	4
	uint32_t	version;
	uint16_t	block_size;	/* sectors */
	uint16_t	bucket_size;	/* sectors */
	uint32_t	journal_start;	/* buckets */
	uint32_t	first_bucket;	/* start of data */
	uint64_t	nbuckets;	/* device size */
	uint64_t	btree_root;
	uint16_t	btree_level;
	uint16_t	_pad[3];
	uint8_t		uuid[16];
};

struct bucket_disk {
	uint16_t	priority;
	uint8_t		generation;
} __attribute((packed));

#endif
