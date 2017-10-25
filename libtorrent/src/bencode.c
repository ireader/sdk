/*
https://en.wikipedia.org/wiki/Bencode
Bencode uses ASCII characters as delimiters and digits.
1. An integer is encoded as i<integer encoded in base ten ASCII>e. Leading zeros are not allowed (although the number zero is still represented as "0"). Negative values are encoded by prefixing the number with a minus sign. The number 42 would thus be encoded as i42e, 0 as i0e, and -42 as i-42e. Negative zero is not permitted.
2. A byte string (a sequence of bytes, not necessarily characters) is encoded as <length>:<contents>. The length is encoded in base 10, like integers, but must be non-negative (zero is allowed); the contents are just the bytes that make up the string. The string "spam" would be encoded as 4:spam. The specification does not deal with encoding of characters outside the ASCII set; to mitigate this, some BitTorrent applications explicitly communicate the encoding (most commonly UTF-8) in various non-standard ways. This is identical to how netstrings work, except that netstrings additionally append a comma suffix after the byte sequence.
3. A list of values is encoded as l<contents>e . The contents consist of the bencoded elements of the list, in order, concatenated. A list consisting of the string "spam" and the number 42 would be encoded as: l4:spami42ee. Note the absence of separators between elements.
4. A dictionary is encoded as d<contents>e. The elements of the dictionary are encoded each key immediately followed by its value. All keys must be byte strings and must appear in lexicographical order. A dictionary that associates the values 42 and "spam" with the keys "foo" and "bar", respectively (in other words, {"bar": "spam", "foo": 42}), would be encoded as follows: d3:bar4:spam3:fooi42ee. (This might be easier to read by inserting some spaces: d 3:bar 4:spam 3:foo i42e e.)

There are no restrictions on what kind of values may be stored in lists and dictionaries; they may (and usually do) contain other lists and dictionaries. This allows for arbitrarily complex data structures to be encoded.
*/
#include "bencode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

static int bencode_read_int2(const uint8_t* str, const uint8_t* end, struct bvalue_t* value)
{
	value->type = BT_INT;
	return bencode_read_int(str, end, &value->v.value);
}

static int bencode_read_string2(const uint8_t* str, const uint8_t* end, struct bvalue_t* value)
{
	value->type = BT_STRING;
	return bencode_read_string(str, end, &value->v.str.value, &value->v.str.bytes);
}

int bencode_read_int(const uint8_t* str, const uint8_t* end, int64_t* value)
{
	const uint8_t* p = str;
	if (str >= end || *str++ != 'i')
		return 0;

	*value = strtoll((const char*)str, (char**)&str, 10);

	if (str < end && *str == 'e')
		return str + 1 - p;
	return 0;
}

int bencode_read_string(const uint8_t* str, const uint8_t* end, char** value, size_t* bytes)
{
	long len;
	char* ptr;
	const uint8_t* p = str;

	len = strtol((const char*)str, (char**)&str, 10);
	if (str >= end || *str++ != ':' || len < 0)
		return 0;

	if (str + len > end)
		return 0;

	ptr = (char*)malloc(len + 1);
	if (!ptr)
		return 0;

	memcpy(ptr, str, len);
	ptr[len] = 0;
	*value = ptr;
	*bytes = len;
	return str + len - p;
}

int bencode_read_list(const uint8_t* str, const uint8_t* end, struct bvalue_t* value)
{
	size_t n, capacity = 0;
	const uint8_t* p = str;
	if (str >= end || *str++ != 'l')
		return 0;

	value->type = BT_LIST;
	value->v.list.count = 0;
	value->v.list.values = NULL;

	while (str < end && *str != 'e')
	{
		if (value->v.list.count >= capacity)
		{
			void* ptr;
			capacity += 32;
			ptr = realloc(value->v.list.values, capacity * sizeof(struct bvalue_t));
			if (!ptr)
				break;
			value->v.list.values = (struct bvalue_t*)ptr;
		}

		n = 0;
		switch (*str)
		{
		case 'i':
			n = bencode_read_int2(str, end, value->v.list.values + value->v.list.count++);
			break;

		case 'l':
			n = bencode_read_list(str, end, value->v.list.values + value->v.list.count++);
			break;

		case 'd':
			n = bencode_read_dict(str, end, value->v.list.values + value->v.list.count++);
			break;

		default:
			if (*str >= '0' && *str <= '9')
			{
				n = bencode_read_string2(str, end, value->v.list.values + value->v.list.count++);
			}
			else
			{
				assert(0);
				break;
			}
		}

		if (0 == n)
			break;
		str += n;
	}

	if (str < end && *str == 'e')
		return str + 1 - p;

	if (value->v.list.values) 
		free(value->v.list.values);
	return 0;
}

int bencode_read_dict(const uint8_t* str, const uint8_t* end, struct bvalue_t* value)
{
	size_t n, capacity = 0;
	const uint8_t* p = str;
	if (str >= end || *str++ != 'd')
		return 0;

	value->type = BT_DICT;
	value->v.dict.count = 0;
	value->v.dict.names = NULL;
	value->v.dict.values = NULL;

	while (str < end && *str != 'e')
	{
		if (value->v.dict.count >= capacity)
		{
			void *names, *values;
			capacity += 32;
			names = realloc(value->v.dict.names, capacity * sizeof(value->v.dict.names[0]));
			values = realloc(value->v.dict.values, capacity * sizeof(value->v.dict.values[0]));
			if (!names || !values)
				break;
			value->v.dict.names = names;
			value->v.dict.values = values;
		}

		n = bencode_read_string(str, end, &value->v.dict.names[value->v.dict.count].name, &value->v.dict.names[value->v.dict.count].bytes);
		if (0 == n || str + n >= end)
			break;

		str += n;
		n = 0;
		switch (*str)
		{
		case 'i':
			n = bencode_read_int2(str, end, value->v.dict.values + value->v.dict.count++);
			break;

		case 'l':
			n = bencode_read_list(str, end, value->v.dict.values + value->v.dict.count++);
			break;

		case 'd':
			n = bencode_read_dict(str, end, value->v.dict.values + value->v.dict.count++);
			break;

		default:
			if (*str >= '0' && *str <= '9')
			{
				n = bencode_read_string2(str, end, value->v.dict.values + value->v.dict.count++);
			}
			else
			{
				assert(0);
				break;
			}
		}

		if (0 == n)
			break;
		str += n;
	}

	if (str < end && *str == 'e')
		return str + 1 - p;

	if (value->v.dict.names) free(value->v.dict.names);
	if (value->v.dict.values) free(value->v.dict.values);
	return 0;
}

uint8_t* bencode_write_int(uint8_t* buffer, const uint8_t* end, int64_t value)
{
	int r;
	r = snprintf((char*)buffer, end - buffer, "i%" PRId64 "e", value);
	return r <= 0 ? (uint8_t*)end : (buffer + r);
}

uint8_t* bencode_write_string(uint8_t* buffer, const uint8_t* end, const char* str, size_t bytes)
{
	int r;
	r = snprintf((char*)buffer, end - buffer, "%u:", (unsigned int)bytes);
	if (r <= 0 || buffer + r + bytes >= end)
		return (uint8_t*)end;
	memcpy(buffer + r, str, bytes);
	return buffer + r + bytes;
}

uint8_t* bencode_write_list(uint8_t* buffer, const uint8_t* end, const struct bvalue_t* list)
{
	size_t i;
	uint8_t* p;
	const struct bvalue_t* item;
	assert(BT_LIST == list->type);

	p = buffer;
	if(p < end) *p++ = 'l';

	for (i = 0; i < list->v.list.count && p < end; i++)
	{
		item = list->v.list.values + i;
		switch (item->type)
		{
		case BT_INT:
			p = bencode_write_int(p, end, item->v.value);
			break;

		case BT_STRING:
			p = bencode_write_string(p, end, item->v.str.value, item->v.str.bytes);
			break;

		case BT_LIST:
			p = bencode_write_list(p, end, item);
			break;

		case BT_DICT:
			p = bencode_write_dict(p, end, item);
			break;

		default:
			assert(0);
			break;
		}
	}
	if (p < end) *p++ = 'e';
	return p;
}

uint8_t* bencode_write_dict(uint8_t* buffer, const uint8_t* end, const struct bvalue_t* dict)
{
	size_t i;
	uint8_t* p;
	const struct bvalue_t* item;
	assert(BT_DICT == dict->type);

	p = buffer;
	if (p < end) *p++ = 'd';

	for (i = 0; i < dict->v.dict.count && p < end; i++)
	{
		item = dict->v.dict.values + i;

		// write name
		p = bencode_write_string(p, end, dict->v.dict.names[i].name, dict->v.dict.names[i].bytes);

		// write value
		switch (item->type)
		{
		case BT_INT:
			p = bencode_write_int(p, end, item->v.value);
			break;

		case BT_STRING:
			p = bencode_write_string(p, end, item->v.str.value, item->v.str.bytes);
			break;

		case BT_LIST:
			p = bencode_write_list(p, end, item);
			break;

		case BT_DICT:
			p = bencode_write_dict(p, end, item);
			break;

		default:
			assert(0);
			break;
		}
	}
	if (p < end) *p++ = 'e';
	return p;
}

int bencode_read(const uint8_t* ptr, size_t bytes, struct bvalue_t* value)
{
	int n;
	if (bytes < 1)
		return -1;

	value->type = BT_NONE;
	switch (*ptr)
	{
	case 'i':
		n = bencode_read_int2(ptr, ptr + bytes, value);
		break;

	case 'l':
		n = bencode_read_list(ptr, ptr + bytes, value);
		break;

	case 'd':
		n = bencode_read_dict(ptr, ptr + bytes, value);
		break;

	default:
		if (*ptr >= '0' && *ptr <= '9')
		{
			n = bencode_read_string2(ptr, ptr + bytes, value);
		}
		else
		{
			assert(0);
			return -1;
		}
	}

	return n > 0 ? n : -1;
}

int bencode_write(uint8_t* ptr, size_t bytes, const struct bvalue_t* value)
{
	uint8_t* p;
	switch (value->type)
	{
	case BT_INT:
		p = bencode_write_int(ptr, ptr + bytes, value->v.value);
		break;

	case BT_STRING:
		p = bencode_write_string(ptr, ptr + bytes, value->v.str.value, value->v.str.bytes);
		break;

	case BT_LIST:
		p = bencode_write_list(ptr, ptr + bytes, value);
		break;

	case BT_DICT:
		p = bencode_write_dict(ptr, ptr + bytes, value);
		break;

	default:
		assert(0);
		return -1;
	}

	return p - ptr;
}

int bencode_free(struct bvalue_t* value)
{
	size_t i;

	switch (value->type)
	{
	case BT_INT:
		break;

	case BT_STRING:
		free(value->v.str.value);
		break;

	case BT_LIST:
		for (i = 0; i < value->v.list.count; i++)
			bencode_free(value->v.list.values + i);
		free(value->v.list.values);
		break;

	case BT_DICT:
		for (i = 0; i < value->v.dict.count; i++)
		{
			free(value->v.dict.names[i].name);
			bencode_free(value->v.dict.values + i);
		}
		free(value->v.dict.names);
		free(value->v.dict.values);
		break;

	default:
		assert(0);
		return -1;
	}

	return 0;
}

const struct bvalue_t* bencode_find(const struct bvalue_t* node, const char* name)
{
	size_t i;
	assert(BT_DICT == node->type);
	if (BT_DICT != node->type)
		return NULL;

	for (i = 0; i < node->v.dict.count; i++)
	{
		if (0 == strcmp(name, node->v.dict.names[i].name))
			return node->v.dict.values + i;
	}

	return NULL;
}

int bencode_get_int(const struct bvalue_t* node, int32_t* value)
{
	if (BT_INT != node->type)
	{
		assert(0);
		return -1;
	}

	*value = (int32_t)node->v.value;
	return 0;
}

int bencode_get_int64(const struct bvalue_t* node, int64_t* value)
{
	if (BT_INT != node->type)
	{
		assert(0);
		return -1;
	}

	*value = node->v.value;
	return 0;
}

int bencode_get_string(const struct bvalue_t* node, char** value)
{
	if (BT_STRING != node->type)
	{
		assert(0);
		return -1;
	}

	*value = strdup(node->v.str.value);
	return 0;
}

int bencode_get_string_ex(const struct bvalue_t* node, char** value)
{
	if (BT_STRING == node->type)
	{
		return bencode_get_string(node, value);
	}
	else if (BT_LIST == node->type && 1 == node->v.list.count)
	{
		return bencode_get_string(node->v.list.values, value);
	}
	else
	{
		assert(0);
		return -1;
	}
}
