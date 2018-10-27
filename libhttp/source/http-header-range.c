#include "http-header-range.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static inline const char* string_token_int64(const char* str, int64_t *value)
{
	*value = 0;
	while ('0' <= *str && *str <= '9')
	{
		*value = (*value * 10) + (*str - '0');
		++str;
	}
	return str;
}

int http_header_range(const char* field, struct http_header_range_t* range, int num)
{
	int i;
	const char* p;

	p = field;
	for (i = 0; i < num && p; i++)
	{
		p = strpbrk(p, "0123456789-\r\n");
		if (NULL == p || '\r' == *p || '\n' == *p)
			return i;

		if ('-' == *p)
		{
			range[i].start = -1;
		}
		else
		{
			p = string_token_int64(p, &range[i].start);
		}

		p = strpbrk(p, "0123456789,\r\n");
		if (NULL == p || '\r' == *p || '\n' == *p || ',' == *p)
		{
			if (-1 == range[i].start)
				return -1; // invalid
			range[i].end = -1;
		}
		else
		{
			p = string_token_int64(p, &range[i].end);
		}
	}

	return p ? -1 : i;
}

#if defined(DEBUG) || defined(_DEBUG)
void http_header_range_test(void)
{
	struct http_header_range_t range[3];

	assert(1 == http_header_range("bytes=0-499", range, 3));
	assert(0 == range[0].start && 499 == range[0].end);

	assert(1 == http_header_range("bytes=500-999", range, 3));
	assert(500 == range[0].start && 999 == range[0].end);

	assert(1 == http_header_range("bytes=-500", range, 3));
	assert(-1 == range[0].start && 500 == range[0].end);

	assert(1 == http_header_range("bytes=9500-", range, 3));
	assert(9500 == range[0].start && -1 == range[0].end);

	assert(2 == http_header_range("bytes=0-0,-1", range, 3));
	assert(0 == range[0].start && 0 == range[0].end);
	assert(-1 == range[1].start && 1 == range[1].end);

	assert(2 == http_header_range("bytes=500-600,601-999", range, 3));
	assert(500 == range[0].start && 600 == range[0].end);
	assert(601 == range[1].start && 999 == range[1].end);
}
#endif
