#include "bencode.h"
#include "metainfo.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int hash_sha1(const uint8_t* data, unsigned int bytes, uint8_t sha1[20]);

static int metainfo_read_trackers(const struct bvalue_t* anounces, struct metainfo_t* meta)
{
	void* p;
	size_t i;
	
	if (BT_LIST != anounces->type)
	{
		assert(0);
		return -1;
	}

	assert(1 == meta->tracker_count);
	p = realloc(meta->trackers, sizeof(char*) * (1 + anounces->v.list.count));
	if (!p)
		return -1;

	meta->trackers = (char**)p;
	if (1 == meta->tracker_count)
	{
		// if announce-list exist, then ignore announce
		meta->tracker_count = 0;
		free(meta->trackers[0]);
	}

	for (i = 0; i < anounces->v.list.count; i++)
	{
		if (0 != bencode_get_string_ex(anounces->v.list.values + i, &meta->trackers[meta->tracker_count++]))
			return -1;
	}

	return 0;
}

static int metainfo_read_nodes(const struct bvalue_t* nodes, struct metainfo_t* meta)
{
	void* p;
	size_t i;
	const struct bvalue_t* node;

	if (BT_LIST != nodes->type)
	{
		assert(0);
		return -1;
	}

	assert(0 == meta->node_count);
	p = realloc(meta->nodes, sizeof(char*) * (1 + nodes->v.list.count));
	if (!p)
		return -1;

	meta->nodes = (char**)p;
	meta->node_port = (int*)realloc(meta->node_port, sizeof(meta->node_port[0]) * (1 + nodes->v.list.count));

	for (i = 0; i < nodes->v.list.count; i++)
	{
		node = nodes->v.list.values + i;
		if (BT_LIST == node->type && 2 == node->v.list.count)
		{
			if (0 != bencode_get_string_ex(node->v.list.values, &meta->nodes[meta->node_count])
				|| 0 != bencode_get_int(node->v.list.values + 1, &meta->node_port[meta->node_count]))
				return -1;
			++meta->node_count;
		}
	}

	return 0;
}

static int metainfo_read_files(const struct bvalue_t* files, struct metainfo_t* meta)
{
	int r = 0;
	size_t i, j;
	int64_t offset = 0;

	if (BT_LIST != files->type)
	{
		assert(0);
		return -1;
	}

	assert(0 == meta->file_count);
	meta->files = malloc(sizeof(meta->files[0]) * (1 + files->v.list.count));
	if (!meta->files)
		return -1;

	for (i = 0; i < files->v.list.count && 0 == r; i++)
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
				r = bencode_get_int64(file->v.dict.values + j, &meta->files[meta->file_count].bytes);
			else if (0 == strcmp("path", file->v.dict.names[j].name))
				r = bencode_get_string_ex(file->v.dict.values + j, &meta->files[meta->file_count].name);
			else
				r = -1;
		}

		meta->files[meta->file_count].offset = offset;
		offset += meta->files[meta->file_count].bytes;
		++meta->file_count;
	}

	return r;
}

static int metainfo_read_pieces(const struct bvalue_t* pieces, struct metainfo_t* meta)
{
	if (BT_STRING != pieces->type || 0 != pieces->v.str.bytes % 20 || 0 == pieces->v.str.bytes)
	{
		assert(0);
		return -1;
	}

	assert(0 == meta->piece_count);
	meta->pieces = malloc(pieces->v.str.bytes);
	if (!meta->pieces)
		return -1;

	meta->piece_count = pieces->v.str.bytes / 20;
	memcpy(meta->pieces, pieces->v.str.value, pieces->v.str.bytes);
	return 0;
}

static int metainfo_read_info(const struct bvalue_t* info, struct metainfo_t* meta)
{
	int r = 0;
	size_t i = 0;
	int64_t len = 0;

	if (BT_DICT != info->type)
	{
		assert(0);
		return -1;
	}

	for (i = 0; i < info->v.dict.count && 0 == r; i++)
	{
		if (0 == strcmp("files", info->v.dict.names[i].name))
		{
			r = metainfo_read_files(info->v.dict.values + i, meta);
		}
		else if (0 == strcmp("name", info->v.dict.names[i].name))
		{
			r = bencode_get_string(info->v.dict.values + i, &meta->file_path);
		}
		else if (0 == strcmp("length", info->v.dict.names[i].name))
		{
			r = bencode_get_int64(info->v.dict.values + i, &len);
		}
		else if (0 == strcmp("pieces", info->v.dict.names[i].name))
		{
			r = metainfo_read_pieces(info->v.dict.values + i, meta);
		}
		else if (0 == strcmp("piece length", info->v.dict.names[i].name))
		{
			r = bencode_get_int64(info->v.dict.values + i, &meta->piece_bytes);
		}
		else
		{
			assert(0); // unknown info data
		}
	}

	// single file mode
	if (len > 0 && 0 == meta->file_count)
	{
		meta->files = malloc(sizeof(meta->files[0]));
		if (!meta->files)
			return -1;
		meta->files[0].name = strdup("");
		meta->files[0].bytes = len;
		meta->files[0].offset = 0;
		meta->file_count = 1;
	}

	return 0;
}

int metainfo_read(const uint8_t* ptr, size_t bytes, struct metainfo_t* meta)
{
	int r = 0;
	size_t i;
	struct bvalue_t root;
	r = bencode_read(ptr, bytes, &root);
	if (r <= 0 || BT_DICT != root.type)
		return -1;

	memset(meta, 0, sizeof(*meta));
	meta->trackers = malloc(sizeof(char*));
	if (!meta->trackers)
		return -1;

	for (i = 0; i < root.v.dict.count && 0 == r; i++)
	{
		if (0 == strcmp("announce", root.v.dict.names[i].name))
		{
			r = bencode_get_string(root.v.dict.values + i, &meta->trackers[meta->tracker_count++]);
		}
		else if (0 == strcmp("announce-list", root.v.dict.names[i].name))
		{
			r = metainfo_read_trackers(root.v.dict.values + i, meta);
		}
		else if (0 == strcmp("comment", root.v.dict.names[i].name))
		{
			r = bencode_get_string(root.v.dict.values + i, &meta->comment);
		}
		else if (0 == strcmp("created by", root.v.dict.names[i].name))
		{
			r = bencode_get_string(root.v.dict.values + i, &meta->author);
		}
		else if (0 == strcmp("creation date", root.v.dict.names[i].name))
		{
			r = bencode_get_int64(root.v.dict.values + i, &meta->create);
		}
		else if (0 == strcmp("info", root.v.dict.names[i].name))
		{
			r = metainfo_read_info(root.v.dict.values + i, meta);
		}
		else if (0 == strcmp("nodes", root.v.dict.names[i].name))
		{
			r = metainfo_read_nodes(root.v.dict.values + i, meta);
		}
		else
		{
			assert(0); // unknown meta data
		}
	}

	bencode_free(&root);
	return r;
}

static uint8_t* metainfo_write_info(uint8_t* ptr, const uint8_t* end, const struct metainfo_t* meta)
{
	size_t i;

	if (ptr < end) *ptr++ = 'd';
	if (1 == meta->file_count)
	{
		ptr = bencode_write_string(ptr, end, "name", 4);
		ptr = bencode_write_string(ptr, end, meta->files[0].name, strlen(meta->files[0].name));
		ptr = bencode_write_string(ptr, end, "length", 6);
		ptr = bencode_write_int(ptr, end, meta->files[0].bytes);
	}
	else
	{
		ptr = bencode_write_string(ptr, end, "files", 5);
		if (ptr < end) *ptr++ = 'l';
		for (i = 0; i < meta->file_count; i++)
		{
			if (ptr < end) *ptr++ = 'd';
			ptr = bencode_write_string(ptr, end, "length", 6);
			ptr = bencode_write_int(ptr, end, meta->files[i].bytes);
			ptr = bencode_write_string(ptr, end, "path", 4);
			if (ptr < end) *ptr++ = 'l';
			ptr = bencode_write_string(ptr, end, meta->files[i].name, strlen(meta->files[i].name));
			if (ptr < end) *ptr++ = 'e';
			if (ptr < end) *ptr++ = 'e';
		}
		if (ptr < end) *ptr++ = 'e';

		ptr = bencode_write_string(ptr, end, "name", 4);
		ptr = bencode_write_string(ptr, end, meta->file_path, strlen(meta->file_path));
	}

	ptr = bencode_write_string(ptr, end, "piece length", 12);
	ptr = bencode_write_int(ptr, end, meta->piece_bytes);

	ptr = bencode_write_string(ptr, end, "pieces", 6);
	ptr = bencode_write_string(ptr, end, (const char*)meta->pieces, meta->piece_count * 20);
	if (ptr < end) *ptr++ = 'e';

	return ptr;
}

int metainfo_write(const struct metainfo_t* meta, uint8_t* buffer, size_t bytes)
{
	size_t i;
	uint8_t *ptr, *end;
	ptr = buffer;
	end = ptr + bytes;

	if (meta->tracker_count < 0 || meta->file_count < 1)
		return -1;

	if (ptr < end) *ptr++ = 'd';

	// announce
	ptr = bencode_write_string(ptr, end, "announce", 8);
	ptr = bencode_write_string(ptr, end, meta->trackers[0], strlen(meta->trackers[0]));

	// announce-list
	if (meta->tracker_count > 1)
	{
		ptr = bencode_write_string(ptr, end, "announce-list", 13);
		if (ptr < end) *ptr++ = 'l';
		for (i = 1; i < meta->tracker_count; i++)
		{
			if (ptr < end) *ptr++ = 'l';
			ptr = bencode_write_string(ptr, end, meta->trackers[i], strlen(meta->trackers[i]));
			if (ptr < end) *ptr++ = 'e';
		}
		if (ptr < end) *ptr++ = 'e';
	}

	// comment
	if (meta->comment)
	{
		ptr = bencode_write_string(ptr, end, "comment", 7);
		ptr = bencode_write_string(ptr, end, meta->comment, strlen(meta->comment));
	}

	// created by
	if (meta->author)
	{
		ptr = bencode_write_string(ptr, end, "created by", 10);
		ptr = bencode_write_string(ptr, end, meta->author, strlen(meta->author));
	}

	// creation date
	if (meta->create)
	{
		ptr = bencode_write_string(ptr, end, "creation date", 13);
		ptr = bencode_write_int(ptr, end, meta->create);
	}

	// info
	ptr = bencode_write_string(ptr, end, "info", 4);
	ptr = metainfo_write_info(ptr, end, meta);

	if (ptr < end) *ptr++ = 'e';
	return ptr - buffer;
}

int metainfo_free(struct metainfo_t* meta)
{
	size_t i;

	if (meta->trackers)
	{
		for (i = 0; i < meta->tracker_count; i++)
			free(meta->trackers[i]);
		free(meta->trackers);
	}

	if (meta->nodes)
	{
		for (i = 0; i < meta->node_count; i++)
			free(meta->nodes[i]);
		free(meta->nodes);
	}

	if (meta->node_port)
		free(meta->node_port);

	for (i = 0; i < meta->file_count; i++)
		free(meta->files[i].name);
	free(meta->files);

	if (meta->comment)
		free(meta->comment);
	if (meta->author)
		free(meta->author);
	if (meta->pieces)
		free(meta->pieces);
	if (meta->file_path)
		free(meta->file_path);
	return 0;
}

int metainfo_hash(const struct metainfo_t* meta, uint8_t sha1[20])
{
	uint8_t *info, *ptr;

	if (meta->file_count < 1)
		return -1;

	info = (uint8_t*)malloc(2 * 1024 * 1024);
	ptr = metainfo_write_info(info, info + 2 * 1024 * 1024, meta);
	if (ptr + 1 < info + 2 * 1024 * 1024)
	{
		hash_sha1(info, ptr - info, sha1);
	}
	free(info);

	return 0;
}
