#ifndef _torrent_reader_h_
#define _torrent_reader_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct torrent_t
{
	char** trackers;
	size_t tracker_count;

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
	} *files;
	size_t file_count; // files count
	char* file_path;
};

int torrent_read(const uint8_t* ptr, size_t bytes, struct torrent_t* tor);
int torrent_write(const struct torrent_t* tor, uint8_t* ptr, size_t bytes);
int torrent_free(struct torrent_t* tor);

// info hash
int torrent_hash(const struct torrent_t* tor, uint8_t sha1[20]);

#if defined(__cplusplus)
}
#endif
#endif /* !_torrent_reader_h_ */
