#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define HASHMAP_FLAG_HOLD	1
#define HASHMAP_LOAD_FACTOR 85

int hashmap_init(struct hashmap_t* map, int capacity, int keysize)
{
	void* p;
	p = calloc(capacity + 1 /*reserved*/, sizeof(struct hashmap_entry_t) + keysize);
	if (!p)
		return -ENOMEM;

	map->buckets = (struct hashmap_entry_t*)p;
	map->capacity = capacity;
	map->size = 0;
	map->keysize = keysize;
	return 0;
}

void hashmap_clear(struct hashmap_t* map)
{
	if (!map)
		return;
	
	map->size = 0;
	map->capacity = 0;
	
	if (map->buckets)
	{
		free(map->buckets);
		map->buckets = NULL;
	}
}

static int hashmap_find(struct hashmap_t* map, unsigned int hash, void* key)
{
	unsigned int pos;
	unsigned int distance;
	struct hashmap_entry_t* e;

	pos = hash % map->capacity;
	for (distance = 0; distance < map->capacity; distance++)
	{
		e = (struct hashmap_entry_t*)(((char*)map->buckets) + (sizeof(struct hashmap_entry_t) + map->keysize) * pos);
		if (0 == (e->flag & HASHMAP_FLAG_HOLD))
			return -1; // empty slot

		if(e->distance < distance)
			return -1; // early exit

		if (e->hash == hash && 0 == memcmp(e + 1, key, map->keysize))
			return pos;

		pos = (pos + 1) % map->capacity;
	}

	return -1;
}

static int hashmap_insert_internal(struct hashmap_t* map, struct hashmap_entry_t* buckets, unsigned int hash, void* key, const void* value)
{
	unsigned int pos;
	unsigned int distance;
	struct hashmap_entry_t* e, *old;

	pos = hash % map->capacity;
	for (distance = 0; distance < map->capacity; distance++)
	{
		e = (struct hashmap_entry_t*)((char*)buckets + (sizeof(struct hashmap_entry_t) + map->keysize) * pos);
		if (0 == (e->flag & HASHMAP_FLAG_HOLD))
		{
			// insert one
			e->flag |= HASHMAP_FLAG_HOLD;
			e->distance = distance;
			e->hash = hash;
			e->value = value;
			memcpy(e + 1, key, map->keysize);
			return 0;
		}

		if (e->hash == hash && 0 == memcmp(e + 1, key, map->keysize))
		{
			// update
			e->value = value;
			return 1;
		}

		if (e->distance < distance)
		{
			// overwrite
			old = (struct hashmap_entry_t*)((char*)buckets + (sizeof(struct hashmap_entry_t) + map->keysize) * map->capacity);
			memcpy(old, e, sizeof(struct hashmap_entry_t) + map->keysize);
			e->flag |= HASHMAP_FLAG_HOLD;
			e->distance = distance;
			e->hash = hash;
			e->value = value;
			memcpy(e + 1, key, map->keysize);

			// move
			key = old + 1;
			hash = old->hash;
			value = old->value;
			distance = old->distance;
		}

		pos = (pos + 1) % map->capacity;
	}

	return -EFAULT;
}

static int hashmap_resize(struct hashmap_t* map)
{
	int r;
	unsigned int i;
	struct hashmap_entry_t* entries, *e;
	entries = (struct hashmap_entry_t* )calloc((map->capacity << 1) + 1 /*reserved*/, sizeof(struct hashmap_entry_t) + map->keysize);
	if (!entries)
		return -ENOMEM;

	for (i = r = 0; r >= 0 && i < map->capacity; i++)
	{
		e = (struct hashmap_entry_t*)(((char*)map->buckets) + (sizeof(struct hashmap_entry_t) + map->keysize) * i);
		if (0 == (e->flag & HASHMAP_FLAG_HOLD))
			continue;
		r = hashmap_insert_internal(map, entries, e->hash, e+1, e->value);
	}

	if (r >= 0)
	{
		free(map->buckets);
		map->buckets = entries;
		map->capacity = map->capacity << 1;
	}
	return r;
}

const void* hashmap_get(struct hashmap_t* map, unsigned int hash, void* key)
{
	int pos;
	struct hashmap_entry_t* e;

	pos = hashmap_find(map, hash, key);
	if (pos >= 0 && pos < (int)map->capacity)
	{
		e = (struct hashmap_entry_t*)(((char*)map->buckets) + (sizeof(struct hashmap_entry_t) + map->keysize) * pos);
		return e->value;
	}
	return NULL;
}

int hashmap_insert(struct hashmap_t* map, unsigned int hash, void* key, const void* value)
{
	int r;
	if (map->size * 100 / map->capacity > HASHMAP_LOAD_FACTOR)
	{
		r = hashmap_resize(map);
		if (r < 0)
			return r;
	}
	
	r = hashmap_insert_internal(map, map->buckets, hash, key, value);
	if (0 == r)
		map->size += 1;
	return r > 0 ? 0 : r;
}

int hashmap_delete(struct hashmap_t* map, unsigned int hash, void* key)
{
	int pos;
	unsigned int distance;
	struct hashmap_entry_t* e, *next;

	pos = hashmap_find(map, hash, key);
	if (pos < 0 || pos >= (int)map->capacity)
		return -ENOENT;

	e = (struct hashmap_entry_t*)(((char*)map->buckets) + (sizeof(struct hashmap_entry_t) + map->keysize) * pos);
	e->flag &= ~HASHMAP_FLAG_HOLD;
	map->size -= 1;

	for (distance = e->distance; distance < map->capacity; distance++)
	{
		pos = (pos + 1) % map->capacity;
		next = (struct hashmap_entry_t*)(((char*)map->buckets) + (sizeof(struct hashmap_entry_t) + map->keysize) * pos);
		if (0 == (next->flag & HASHMAP_FLAG_HOLD))
			return 0;

		if (next->distance == 0)
			return 0;

		// move slot
		memcpy(e, next, sizeof(struct hashmap_entry_t) + map->keysize);
		e->distance -= 1;

		// clear slot
		next->flag &= ~HASHMAP_FLAG_HOLD;

		e = next;
	}

	return -ENOENT;
}


#if defined(DEBUG) || defined(_DEBUG)
static void test_basic_put_get() 
{
	struct hashmap_t map;
	hashmap_init(&map, 8, sizeof(char*));

	hashmap_insert(&map, 123, "name", "hashmap");
	hashmap_insert(&map, 456, "version", "1.0");

	assert(strcmp((char*)hashmap_get(&map, 123, "name"), "hashmap") == 0);
	assert(strcmp((char*)hashmap_get(&map, 456, "version"), "1.0") == 0);

	hashmap_clear(&map);
}

static void test_update_value()
{
	struct hashmap_t map;
	hashmap_init(&map, 8, sizeof(char*));

	hashmap_insert(&map, 123, "key", "v1");
	hashmap_insert(&map, 123, "key", "v2");

	assert(strcmp((char*)hashmap_get(&map, 123, "key"), "v2") == 0);
	assert(map.size == 1);

	hashmap_clear(&map);
}

static void test_get_missing()
{
	struct hashmap_t map;
	hashmap_init(&map, 8, sizeof(char*));

	hashmap_insert(&map, 123, "a", "1");
	hashmap_insert(&map, 456, "b", "2");

	assert(hashmap_get(&map, 1, "c") == NULL);
	assert(hashmap_get(&map, 123, "c") == NULL);

	hashmap_clear(&map);
}

static void test_auto_resizing()
{
	int i;
	struct hashmap_t map;
	const char* key[] = { "k1", "k2", "k3", "k4", "k5", "k6", "k7", "k8", "k9", "k10" };
	const char* val[] = { "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10" };
	hashmap_init(&map, 4, sizeof(char*));

	assert(sizeof(key) / sizeof(key[0]) == sizeof(val) / sizeof(val[0]));
	for (i = 0; i < sizeof(key)/sizeof(key[0]); i++) {
		hashmap_insert(&map, i, key[i], val[i]);
	}

	assert(map.size == 10);
	assert(map.capacity > 4);
	assert(strcmp((char*)hashmap_get(&map, 9, key[9]), val[9]) == 0);

	hashmap_clear(&map);
}

static void test_deletion_and_shift()
{
	struct hashmap_t map;
	hashmap_init(&map, 16, sizeof(char*));

	hashmap_insert(&map, 1, "key1", "val1");
	hashmap_insert(&map, 1, "key2", "val2");
	hashmap_insert(&map, 1, "key3", "val3");
	hashmap_insert(&map, 4, "key4", "val4");

	hashmap_delete(&map, 1, "key1");

	assert(((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 1))->distance == 0);
	assert(0 == strcmp("val2", ((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 1))->value));
	assert(((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 2))->distance == 1);
	assert(0 == strcmp("val3", ((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 2))->value));
	assert(((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 3))->flag == 0);
	assert(((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 4))->distance == 0);
	assert(0 == strcmp("val4", ((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 4))->value));

	assert(hashmap_get(&map, 1, "key1") == NULL);
	assert(strcmp((char*)hashmap_get(&map, 1, "key2"), "val2") == 0);
	assert(strcmp((char*)hashmap_get(&map, 1, "key3"), "val3") == 0);
	assert(strcmp((char*)hashmap_get(&map, 4, "key4"), "val4") == 0);
	assert(map.size == 3);

	hashmap_clear(&map);
}

static void test_robin_hood_swapping()
{
	struct hashmap_t map;
	hashmap_init(&map, 10, sizeof(char*));

	hashmap_insert(&map, 1, "A", "valA");
	hashmap_insert(&map, 2, "B", "valB");
	hashmap_insert(&map, 1, "C", "valC");

	assert(((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 1))->distance == 0);
	assert(0 == strcmp("valA", ((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 1))->value));
	assert(((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 2))->distance == 1);
	assert(0 == strcmp("valC", ((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 2))->value));
	assert(((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 3))->distance == 1);
	assert(0 == strcmp("valB", ((struct hashmap_entry_t*)(((char*)map.buckets) + (sizeof(struct hashmap_entry_t) + map.keysize) * 3))->value));

	assert(strcmp((char*)hashmap_get(&map, 1, "A"), "valA") == 0);
	assert(strcmp((char*)hashmap_get(&map, 2, "B"), "valB") == 0);
	assert(strcmp((char*)hashmap_get(&map, 1, "C"), "valC") == 0);

	hashmap_clear(&map);
}

void hashmap_test(void)
{
	test_basic_put_get();
	test_update_value();
	test_get_missing();
	test_auto_resizing();
	test_deletion_and_shift();
	test_robin_hood_swapping();
}
#endif
