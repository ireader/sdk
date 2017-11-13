#ifndef _metainfo_h_
#define _metainfo_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct metainfo_t
{
	char** trackers;
	size_t tracker_count;

	// BEP 5 => Torrent File Extensions
	// trackerless torrent
	int* node_port;
	char** nodes;
	size_t node_count;

	int64_t create;
	char* comment;
	char* author;

	int64_t piece_bytes; // bytes per piece
	uint8_t *pieces; // piece sha1 hash, 20 bytes per piece
	size_t piece_count; // pieces length = piece_count * 20

	struct
	{
		char* name; // concat with file_path
		int64_t bytes; // file size
		int64_t offset; // file offet (byte)
	} *files;
	size_t file_count; // files count
	char* file_path;
};

int metainfo_read(const uint8_t* ptr, size_t bytes, struct metainfo_t* meta);
int metainfo_write(const struct metainfo_t* meta, uint8_t* ptr, size_t bytes);
int metainfo_free(struct metainfo_t* meta);

// info hash
int metainfo_hash(const struct metainfo_t* meta, uint8_t sha1[20]);

/// @return >0-info size, <=0-error
int metainfo_info(const struct metainfo_t* meta, uint8_t* ptr, size_t bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_metainfo_h_ */
