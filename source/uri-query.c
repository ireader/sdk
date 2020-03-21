#include "uri-parse.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define N 64

int uri_query(const char* query, const char* end, struct uri_query_t** items)
{
	int count;
	int capacity;
	const char *p;
	struct uri_query_t items0[N], *pp;

	assert(items);
	*items = NULL;
	capacity = count = 0;

	for (p = query; p && p < end; query = p + 1)
	{
		p = strpbrk(query, "&=");
		assert(!p || *p);
		if (!p || p > end)
			break;

		if (p == query)
		{
			if ('&' == *p)
			{
				continue; // skip k1=v1&&k2=v2
			}
			else
			{
				uri_query_free(items);
				return -1;  // no-name k1=v1&=v2
			}
		}

		if (count < N)
		{
			pp = &items0[count++];
		}
		else
		{
			if (count >= capacity)
			{
				capacity = count + 64;
				pp = (struct uri_query_t*)realloc(*items, capacity * sizeof(struct uri_query_t));
				if (!pp) return -ENOMEM;
				*items = pp;
			}

			pp = &(*items)[count++];
		}

		pp->name = query;
		pp->n_name = (int)(p - query);

		if ('=' == *p)
		{
			pp->value = p + 1;
			p = strchr(pp->value, '&');
			if (NULL == p) p = end;
			pp->n_value = (int)(p - pp->value); // empty-value
		}
		else
		{
			assert('&' == *p);
			pp->value = NULL;
			pp->n_value = 0; // no-value
		}
	}

	if (count <= N && count > 0)
	{
		*items = (struct uri_query_t*)malloc(count * sizeof(struct uri_query_t));
		if (!*items) return -ENOMEM;
		memcpy(*items, items0, count * sizeof(struct uri_query_t));
	}
	else if(count > N)
	{
		memcpy(*items, items0, N * sizeof(struct uri_query_t));
	}

	return count;
}

void uri_query_free(struct uri_query_t** items)
{
	if (items && *items)
	{
		free(*items);
		*items = NULL;
	}
}

#if defined(DEBUG) || defined(_DEBUG)
void uri_query_test(void)
{
	const char* s1 = "";
	const char* s2 = "name=value&a=b&";
	const char* s3 = "name=value&&a=b&";
	const char* s4 = "name=value&&=b&";
	const char* s5 = "name=value&k1=v1&k2=v2&k3=v3&k4&k5&k6=v6";

	struct uri_query_t* items;
	assert(0 == uri_query(s1, s1 + strlen(s1), &items));
	uri_query_free(&items);

	assert(2 == uri_query(s2, s2 + strlen(s2), &items));
	assert(0 == strncmp("name", items[0].name, items[0].n_name) && 0 == strncmp("value", items[0].value, items[0].n_value));
	assert(0 == strncmp("a", items[1].name, items[1].n_name) && 0 == strncmp("b", items[1].value, items[1].n_value));
	uri_query_free(&items);

	assert(2 == uri_query(s3, s3 + strlen(s4), &items));
	assert(0 == strncmp("name", items[0].name, items[0].n_name) && 0 == strncmp("value", items[0].value, items[0].n_value));
	assert(0 == strncmp("a", items[1].name, items[1].n_name) && 0 == strncmp("b", items[1].value, items[1].n_value));
	uri_query_free(&items);

	assert(-1 == uri_query(s4, s4 + strlen(s4), &items));
	uri_query_free(&items);

	assert(7 == uri_query(s5, s5 + strlen(s5), &items));
	assert(0 == strncmp("name", items[0].name, items[0].n_name) && 0 == strncmp("value", items[0].value, items[0].n_value));
	assert(0 == strncmp("k1", items[1].name, items[1].n_name) && 0 == strncmp("v1", items[1].value, items[1].n_value));
	assert(0 == strncmp("k2", items[2].name, items[2].n_name) && 0 == strncmp("v2", items[2].value, items[2].n_value));
	assert(0 == strncmp("k3", items[3].name, items[3].n_name) && 0 == strncmp("v3", items[3].value, items[3].n_value));
	assert(0 == strncmp("k4", items[4].name, items[4].n_name) && 0 == items[4].n_value && NULL == items[4].value);
	assert(0 == strncmp("k5", items[5].name, items[5].n_name) && 0 == items[5].n_value && NULL == items[5].value);
	assert(0 == strncmp("k6", items[6].name, items[6].n_name) && 0 == strncmp("v6", items[6].value, items[6].n_value));
	uri_query_free(&items);
}
#endif
