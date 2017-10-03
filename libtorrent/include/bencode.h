#ifndef _bencode_h_
#define _bencode_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum btype_t { BT_NONE, BT_INT, BT_STRING, BT_LIST, BT_DICT };

struct bvalue_t
{
	enum btype_t type;

	union
	{
		int64_t value;

		struct
		{
			char* value;
			size_t bytes;
		} str;

		struct
		{
			struct bvalue_t* values;
			size_t count;
		} list;

		struct
		{
			struct
			{
				char* name;
				size_t bytes;
			} *names;
			struct bvalue_t* values;
			size_t count;
		} dict;
	} v;
};

const uint8_t* bencode_read_int(const uint8_t* str, const uint8_t* end, int64_t* value);
const uint8_t* bencode_read_string(const uint8_t* str, const uint8_t* end, char** value, size_t* bytes);
const uint8_t* bencode_read_list(const uint8_t* str, const uint8_t* end, struct bvalue_t* value);
const uint8_t* bencode_read_dict(const uint8_t* str, const uint8_t* end, struct bvalue_t* value);

uint8_t* bencode_write_int(uint8_t* buffer, const uint8_t* end, int64_t value);
uint8_t* bencode_write_string(uint8_t* buffer, const uint8_t* end, const char* str, size_t bytes);
uint8_t* bencode_write_list(uint8_t* buffer, const uint8_t* end, const struct bvalue_t* list);
uint8_t* bencode_write_dict(uint8_t* buffer, const uint8_t* end, const struct bvalue_t* dict);

int bencode_read(const uint8_t* ptr, size_t bytes, struct bvalue_t* value);
int bencode_write(uint8_t* ptr, size_t bytes, const struct bvalue_t* value);
int bencode_free(struct bvalue_t* value);

int bencode_get_int(const struct bvalue_t* node, int64_t* value);
int bencode_get_string(const struct bvalue_t* node, char** value);
int bencode_get_string_ex(const struct bvalue_t* node, char** value);

#if defined(__cplusplus)
}
#endif
#endif /* !_bencode_h_ */
