#include "http-websocket-internal.h"
#include "cpm/param.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

enum { WEBSOCKET_FRAME_MAXLENGTH = 64 * 1024 }; // 64KB

enum {
	WEBSOCKET_PARSER_INIT = 0,
	WEBSOCKET_PARSER_HEADER = 1,
	WEBSOCKET_PARSER_HEADER_EXTENDED = 2,
	WEBSOCKET_PARSER_PAYLOAD = 3,
};

int websocket_parser_destroy(struct websocket_parser_t* parser)
{
	if (parser->ptr)
	{
		assert(parser->capacity > 0);
		free(parser->ptr);
		parser->ptr = NULL;
	}

	return 0;
}

static void websocket_parser_reset(struct websocket_parser_t* parser)
{
	parser->len = 0;
	parser->state = WEBSOCKET_PARSER_INIT;
	parser->header_masking_key_off = 0;
	//memset(&parser->header, 0, sizeof(parser->header));
}

static int websocket_parser_alloc(struct websocket_parser_t* parser, size_t bytes)
{
	void* ptr;
	if (parser->capacity < bytes)
	{
		ptr = realloc(parser->ptr, bytes);
		if (!ptr)
			return -ENOMEM;
		parser->ptr = (uint8_t*)ptr;
		parser->capacity = (unsigned int)bytes;
	}

	return 0;
}

int websocket_parser_input(struct websocket_parser_t* parser, uint8_t* data, size_t bytes, websocket_parser_handler handler, void* param)
{
	int r, flags;
	size_t i, off;
	uint64_t len;
	uint64_t max_capacity;

	r = 0;
	for (off = 0; 0 == r && off < bytes;)
	{
		switch (parser->state)
		{
		case WEBSOCKET_PARSER_INIT:
			assert(0 == parser->len);
			parser->h[parser->len++] = data[off++];
			parser->state = WEBSOCKET_PARSER_HEADER;
			break;

		case WEBSOCKET_PARSER_HEADER:
			assert(1 == parser->len);
			parser->h[parser->len++] = data[off++];
			parser->state = WEBSOCKET_PARSER_HEADER_EXTENDED;
			break;

		case WEBSOCKET_PARSER_HEADER_EXTENDED:
			assert(parser->len >= 2);
			len = parser->h[1] & 0x7F;
			len = 2 + ((parser->h[1] & 0x80) /*mask*/ ? 4 : 0) + (len < 126 ? 0 : (len == 126 ? 2 : 8));
			assert(len >= parser->len && len <= sizeof(parser->h));
			i = MIN((size_t)(len - parser->len), bytes - off);
			if (i > 0)
			{
				// mask & extended payload length
				memcpy(parser->h + parser->len, data + off, i);
				parser->len += i;
				off += i;
			}

			if (parser->len == len)
			{
				r = websocket_header_read(&parser->header, parser->h, len);
				r = r < 0 ? r : 0;

				if (0 != parser->header.opcode)
					parser->header_opcode = parser->header.opcode;
				parser->len = 0; // for payload data
				parser->header_masking_key_off = 0;
				parser->state = WEBSOCKET_PARSER_PAYLOAD;

				// fix: 0-length payload (such as close)
				// Fallthrough to WEBSOCKET_PARSER_PAYLOAD
			}
			else
			{
				break;
			}

		case WEBSOCKET_PARSER_PAYLOAD:
			// Buffer to FULL-Frame except: non-fragment(continuation) frame && FIN frame && payload length < max_capability
			max_capacity = parser->max_capacity ? parser->max_capacity : WEBSOCKET_FRAME_MAXLENGTH;
			if ((parser->len > 0 || bytes - off < parser->header.len) && parser->header.fin && parser->header.len <= max_capacity && parser->header.opcode > 0 /* exclude continuation frame */)
			{
				// Merge to Full-Frame
				r = websocket_parser_alloc(parser, parser->header.len);
				len = MIN((unsigned int)(parser->header.len - parser->len), (unsigned int)(bytes - off));
				if (len > 0 && 0 == r)
				{
					if (parser->header.mask)
					{
						for (i = off; i < off + len; i++)
						{
							parser->ptr[parser->len++] = data[i] ^ parser->header.masking_key[parser->header_masking_key_off];
							parser->header_masking_key_off = (parser->header_masking_key_off + 1) % 4;
						}
					}
					else
					{
						memcpy(parser->ptr + parser->len, data + off, len);
						parser->len += len;
					}
					off += len;
				}

				if (parser->len == parser->header.len)
				{
					r = handler(param, parser->header.opcode, parser->ptr, parser->header.len, WEBSOCKET_FLAGS_START | WEBSOCKET_FLAGS_FIN);
					websocket_parser_reset(parser);
				}
			}
			else
			{
				len = MIN(parser->header.len - parser->len, (uint64_t)(bytes - off));
				if (parser->header.mask)
				{
					for (i = off; i < off + len; i++)
					{
						data[i] ^= parser->header.masking_key[parser->header_masking_key_off];
						parser->header_masking_key_off = (parser->header_masking_key_off + 1) % 4;
					}
				}

				flags = parser->header.opcode > 0 && 0 == parser->len ? WEBSOCKET_FLAGS_START : 0;
				flags |= parser->header.fin && (parser->len + len >= parser->header.len) ? WEBSOCKET_FLAGS_FIN : 0;
				r = handler(param, parser->header_opcode /*frame type*/, data + off, len, flags);
				parser->len += len;
				off += len;

				if (parser->len >= parser->header.len)
					websocket_parser_reset(parser);
			}
			break;

		default:
			assert(0);
			r = -1; // unknown state
		}
	}

	return r;
}

int websocket_header_read(struct websocket_header_t* header, const uint8_t* data, size_t bytes)
{
	int n, len;
	if (bytes < 2)
		return -1;

	header->fin = (data[0] >> 7) & 0x01;
	header->rsv = (data[0] >> 4) & 0x07;
	header->opcode = data[0] & 0x0F;
	header->mask = (data[1] >> 7) & 0x01;
	len = data[1] & 0x7F;

	n = 2 + (header->mask ? 4 : 0) + (len < 126 ? 0 : (len == 126 ? 2 : 8));
	if (bytes < (size_t)n)
		return -1;

	if (len < 126)
	{
		n = 2;
		header->len = len;
	}
	else if (126 == len)
	{
		n = 4;
		header->len = ((uint64_t)data[2] << 8) | data[3];
	}
	else
	{
		n = 10;
		assert(127 == len);
		header->len = ((uint64_t)data[2] << 56) | ((uint64_t)data[3] << 48) | ((uint64_t)data[4] << 40) | ((uint64_t)data[5] << 32) | ((uint64_t)data[6] << 24) | ((uint64_t)data[7] << 16) | ((uint64_t)data[8] << 8) | (uint64_t)data[9];
	}

	if (header->mask)
	{
		header->masking_key[0] = data[n++];
		header->masking_key[1] = data[n++];
		header->masking_key[2] = data[n++];
		header->masking_key[3] = data[n++];
	}

	return n;
}

int websocket_header_write(const struct websocket_header_t* header, uint8_t* data, size_t bytes)
{
	int i, n;
	n = 2 + (header->mask ? 4 : 0) + (header->len < 126 ? 0 : (header->len <= 0xFFFF ? 2 : 8));
	if (bytes < (size_t)n)
		return -1;

	i = 0;
	data[i++] = (uint8_t)(header->fin << 7) | (uint8_t)(header->rsv << 4) | (uint8_t)header->opcode;
	data[i++] = header->len < 126 ? (uint8_t)header->len : (header->len <= 0xFFFF ? 126 : 127);
	if (126 == data[1])
	{
		data[i++] = (uint8_t)(header->len >> 8);
		data[i++] = (uint8_t)header->len;
	}
	else if(126 < data[1])
	{
		data[i++] = (uint8_t)(header->len >> 56);
		data[i++] = (uint8_t)(header->len >> 48);
		data[i++] = (uint8_t)(header->len >> 40);
		data[i++] = (uint8_t)(header->len >> 32);
		data[i++] = (uint8_t)(header->len >> 24);
		data[i++] = (uint8_t)(header->len >> 16);
		data[i++] = (uint8_t)(header->len >> 8);
		data[i++] = (uint8_t)header->len;
	}

	if (header->mask)
	{
		data[i++] = header->masking_key[0];
		data[i++] = header->masking_key[1];
		data[i++] = header->masking_key[2];
		data[i++] = header->masking_key[3];
	}

	return i;
}
