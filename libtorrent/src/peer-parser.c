#include "peer-parser.h"
#include "peer-message.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

enum
{
	peer_handshake = 0,

	peer_message_length1,
	peer_message_length2,
	peer_message_length3,
	peer_message_length4,
	peer_message_type,
	peer_message_payload,
};

static int peer_alloc(struct peer_parser_t* paser, size_t bytes)
{
	void* p;
	if (paser->capacity < bytes)
	{
		p = realloc(paser->buffer, bytes);
		if (!p)
			return -1;
		paser->buffer = (uint8_t*)p;
		paser->capacity = bytes;
	}
	return 0;
}

int peer_input(struct peer_parser_t* parser, const uint8_t* data, size_t bytes, int(*handler)(void* param, struct peer_parser_t*), void* param)
{
	int r;
	size_t n;
	const uint8_t* p = data;

	while (bytes > 0)
	{
		switch (parser->state)
		{
		case peer_handshake:
			if (0 != peer_alloc(parser, 68))
				return -1; // allo memory failed
			n = MIN(bytes, 68 - parser->bytes);
			memcpy(parser->buffer + parser->bytes, p, n);
			parser->bytes += n;
			bytes -= n;
			p += n;
			if (68 == parser->bytes)
			{
				parser->len = 68;
				parser->type = BT_HANDSHAKE;
				parser->bytes = 0;
				parser->state = peer_message_length1;
				r = handler(param, parser);
				if (0 != r) return r;
			}
			break;

		case peer_message_length1:
			parser->state = peer_message_length2;
			parser->len = *p++;
			bytes--;
			break;

		case peer_message_length2:
			parser->state = peer_message_length3;
			parser->len = (parser->len << 8) | *p++;
			bytes--;
			break;

		case peer_message_length3:
			parser->state = peer_message_length4;
			parser->len = (parser->len << 8) | *p++;
			bytes--;
			break;

		case peer_message_length4:
			parser->len = (parser->len << 8) | *p++;
			bytes--;
			
			if (parser->len > 0)
			{
				parser->state = peer_message_length1;
				parser->type = BT_KEEPALIVE;
				r = handler(param, parser);
				if (0 != r) return r;
			}
			else
			{
				parser->state = peer_message_type;
			}
			break;

		case peer_message_type:
			assert(0 == parser->bytes);
			parser->state = peer_message_payload;
			parser->bytes = 0;
			parser->type = *p++;
			bytes--;

			if (0 != peer_alloc(parser, parser->len))
				return -1;
			break;

		case peer_message_payload:
			assert(parser->bytes <= parser->len);
			n = MIN(bytes, parser->len - parser->bytes);
			memcpy(parser->buffer + parser->bytes, p, n);
			parser->bytes += n;
			bytes -= n;
			p += n;

			if (parser->len == parser->bytes)
			{
				parser->state = peer_message_length1;
				r = handler(param, parser);
				if (0 != r) return r;
			}
			break;
		}
	}
	return 0;
}
