#include "bencode.h"
#include "torrent.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int torrent_read_int(const struct bvalue_t* node, int64_t* s)
{
	if (BT_INT != node->type)
	{
		assert(0);
		return -1;
	}

	*s = node->v.value;
	return 0;
}

static int torrent_read_string(const struct bvalue_t* node, char** s)
{
	if (BT_STRING != node->type)
	{
		assert(0);
		return -1;
	}

	*s = strdup(node->v.str.value);
	return 0;
}

static int torrent_read_string_ex(const struct bvalue_t* node, char** s)
{
	if (BT_STRING == node->type)
	{
		return torrent_read_string(node, s);
	}
	else if (BT_LIST == node->type && 1 == node->v.list.count)
	{
		return torrent_read_string(node->v.list.values, s);
	}
	else
	{
		assert(0);
		return -1;
	}
}

static int torrent_read_trackers(const struct bvalue_t* anounces, struct torrent_t* tor)
{
	void* p;
	size_t i;
	
	if (BT_LIST != anounces->type)
	{
		assert(0);
		return -1;
	}

	assert(1 == tor->tracker_count);
	p = realloc(tor->trackers, sizeof(char*) * (1 + anounces->v.list.count));
	if (!p)
		return -1;

	tor->trackers = (char**)p;
	for (i = 0; i < anounces->v.list.count; i++)
	{
		if (0 != torrent_read_string_ex(anounces->v.list.values + i, &tor->trackers[tor->tracker_count++]))
			return -1;
	}

	return 0;
}

static int torrent_read_files(const struct bvalue_t* files, struct torrent_t* tor)
{
	int r;
	size_t i, j;

	if (BT_LIST != files->type)
	{
		assert(0);
		return -1;
	}

	assert(0 == tor->file_count);
	tor->files = malloc(sizeof(tor->files[0]) * (1 + files->v.list.count));
	if (!tor->files)
		return -1;

	for (r = i = 0; i < files->v.list.count && 0 == r; i++)
	{
		struct bvalue_t* file;
		file = files->v.list.values + i;
		if (BT_DICT != file->type || 2 != file->v.dict.count)
		{
			assert(0);
			return -1;
		}

		for (j = 0; j < 2 && 0 == r; j++)
		{
			if (0 == strcmp("length", file->v.dict.names[j].name))
				r = torrent_read_int(file->v.dict.values + j, &tor->files[tor->file_count].bytes);
			else if (0 == strcmp("path", file->v.dict.names[j].name))
				r = torrent_read_string_ex(file->v.dict.values + j, &tor->files[tor->file_count].name);
			else
				r = -1;
		}
		++tor->file_count;
	}

	return r;
}

static int torrent_read_pieces(const struct bvalue_t* pieces, struct torrent_t* tor)
{
	if (BT_STRING != pieces->type || 0 != pieces->v.str.bytes % 20 || 0 == pieces->v.str.bytes)
	{
		assert(0);
		return -1;
	}

	assert(0 == tor->piece_count);
	tor->pieces = malloc(pieces->v.str.bytes);
	if (!tor->pieces)
		return -1;

	tor->piece_count = pieces->v.str.bytes / 20;
	memcpy(tor->pieces, pieces->v.str.value, pieces->v.str.bytes);
	return 0;
}

static int torrent_read_info(const struct bvalue_t* info, struct torrent_t* tor)
{
	int r;
	size_t i;
	int64_t len = 0;

	if (BT_DICT != info->type)
	{
		assert(0);
		return -1;
	}

	for (r = i = 0; i < info->v.dict.count && 0 == r; i++)
	{
		if (0 == strcmp("files", info->v.dict.names[i].name))
		{
			r = torrent_read_files(info->v.dict.values + i, tor);
		}
		else if (0 == strcmp("name", info->v.dict.names[i].name))
		{
			r = torrent_read_string(info->v.dict.values + i, &tor->file_path);
		}
		else if (0 == strcmp("length", info->v.dict.names[i].name))
		{
			r = torrent_read_int(info->v.dict.values + i, &len);
		}
		else if (0 == strcmp("pieces", info->v.dict.names[i].name))
		{
			r = torrent_read_pieces(info->v.dict.values + i, tor);
		}
		else if (0 == strcmp("piece length", info->v.dict.names[i].name))
		{
			r = torrent_read_int(info->v.dict.values + i, &tor->piece_bytes);
		}
		else
		{
			assert(0); // unknown info data
		}
	}

	// single file mode
	if (len > 0 && 0 == tor->file_count)
	{
		tor->files = malloc(sizeof(tor->files[0]));
		if (!tor->files)
			return -1;
		tor->files[0].name = strdup("");
		tor->files[0].bytes = len;
		tor->file_count = 1;
	}

	return 0;
}

int torrent_read(const uint8_t* ptr, size_t bytes, struct torrent_t* tor)
{
	int r;
	size_t i;
	struct bvalue_t root;
	bencode_read(ptr, bytes, &root);
	if (BT_DICT != root.type)
		return -1;

	memset(tor, 0, sizeof(*tor));
	tor->trackers = malloc(sizeof(char*));
	if (!tor->trackers)
		return -1;

	for (r = i = 0; i < root.v.dict.count && 0 == r; i++)
	{
		if (0 == strcmp("announce", root.v.dict.names[i].name))
		{
			r = torrent_read_string(root.v.dict.values + i, &tor->trackers[tor->tracker_count++]);
		}
		else if (0 == strcmp("announce-list", root.v.dict.names[i].name))
		{
			r = torrent_read_trackers(root.v.dict.values + i, tor);
		}
		else if (0 == strcmp("comment", root.v.dict.names[i].name))
		{
			r = torrent_read_string(root.v.dict.values + i, &tor->comment);
		}
		else if (0 == strcmp("created by", root.v.dict.names[i].name))
		{
			r = torrent_read_string(root.v.dict.values + i, &tor->author);
		}
		else if (0 == strcmp("creation date", root.v.dict.names[i].name))
		{
			r = torrent_read_int(root.v.dict.values + i, &tor->create);
		}
		else if (0 == strcmp("info", root.v.dict.names[i].name))
		{
			r = torrent_read_info(root.v.dict.values + i, tor);
		}
		else
		{
			assert(0); // unknown meta data
		}
	}

	bencode_free(&root);
	return r;
}

int torrent_write(uint8_t* buffer, size_t bytes, const struct torrent_t* tor)
{
	size_t i;
	uint8_t *ptr, *end;
	ptr = buffer;
	end = ptr + bytes;

	if (tor->tracker_count < 0 || tor->file_count < 1)
		return -1;

	if (ptr < end) *ptr++ = 'd';

	// announce
	ptr = bencode_write_string(ptr, end, "announce", 8);
	ptr = bencode_write_string(ptr, end, tor->trackers[0], strlen(tor->trackers[0]));

	// announce-list
	if (tor->tracker_count > 1)
	{
		ptr = bencode_write_string(ptr, end, "announce-list", 13);
		if (ptr < end) *ptr++ = 'l';
		for (i = 1; i < tor->tracker_count; i++)
		{
			if (ptr < end) *ptr++ = 'l';
			ptr = bencode_write_string(ptr, end, tor->trackers[i], strlen(tor->trackers[i]));
			if (ptr < end) *ptr++ = 'e';
		}
		if (ptr < end) *ptr++ = 'e';
	}

	// comment
	if (tor->comment)
	{
		ptr = bencode_write_string(ptr, end, "comment", 7);
		ptr = bencode_write_string(ptr, end, tor->comment, strlen(tor->comment));
	}

	// created by
	if (tor->author)
	{
		ptr = bencode_write_string(ptr, end, "created by", 10);
		ptr = bencode_write_string(ptr, end, tor->author, strlen(tor->author));
	}

	// creation date
	if (tor->create)
	{
		ptr = bencode_write_string(ptr, end, "creation date", 13);
		ptr = bencode_write_int(ptr, end, tor->create);
	}

	// info
	ptr = bencode_write_string(ptr, end, "info", 4);
	if (ptr < end) *ptr++ = 'd';
	if (1 == tor->file_count)
	{
		ptr = bencode_write_string(ptr, end, "name", 4);
		ptr = bencode_write_string(ptr, end, tor->files[0].name, strlen(tor->files[0].name));
		ptr = bencode_write_string(ptr, end, "length", 6);
		ptr = bencode_write_int(ptr, end, tor->files[0].bytes);
	}
	else
	{
		ptr = bencode_write_string(ptr, end, "files", 5);
		if (ptr < end) *ptr++ = 'l';
		for (i = 0; i < tor->file_count; i++)
		{
			if (ptr < end) *ptr++ = 'd';
			ptr = bencode_write_string(ptr, end, "length", 6);
			ptr = bencode_write_int(ptr, end, tor->files[i].bytes);
			ptr = bencode_write_string(ptr, end, "path", 4);
			if (ptr < end) *ptr++ = 'l';
			ptr = bencode_write_string(ptr, end, tor->files[i].name, strlen(tor->files[i].name));
			if (ptr < end) *ptr++ = 'e';
			if (ptr < end) *ptr++ = 'e';
		}
		if (ptr < end) *ptr++ = 'e';

		ptr = bencode_write_string(ptr, end, "name", 4);
		ptr = bencode_write_string(ptr, end, tor->file_path, strlen(tor->file_path));
	}

	ptr = bencode_write_string(ptr, end, "piece length", 12);
	ptr = bencode_write_int(ptr, end, tor->piece_bytes);

	ptr = bencode_write_string(ptr, end, "pieces", 6);
	ptr = bencode_write_string(ptr, end, (const char*)tor->pieces, tor->piece_count * 20);
	if (ptr < end) *ptr++ = 'e';

	if (ptr < end) *ptr++ = 'e';
	return ptr - buffer;
}

int torrent_free(struct torrent_t* tor)
{
	size_t i;

	if (tor->trackers)
	{
		for (i = 0; i < tor->tracker_count; i++)
			free(tor->trackers[i]);
		free(tor->trackers);
	}

	for (i = 0; i < tor->file_count; i++)
		free(tor->files[i].name);
	free(tor->files);

	if (tor->comment)
		free(tor->comment);
	if (tor->author)
		free(tor->author);
	if (tor->pieces)
		free(tor->pieces);
	if (tor->file_path)
		free(tor->file_path);
	return 0;
}
