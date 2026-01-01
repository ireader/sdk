#ifndef _hashmap_h_
#define _hashmap_h_

struct hashmap_entry_t
{
	unsigned int flag;
	unsigned int hash;
	unsigned int distance; // robin hood distance
	const void* value;
};

struct hashmap_t
{
	struct hashmap_entry_t* buckets;
	unsigned int capacity;
	unsigned int size;
	unsigned int keysize;
};

int hashmap_init(struct hashmap_t* map, int capacity, int keysize);
void hashmap_clear(struct hashmap_t* map);

const void* hashmap_get(struct hashmap_t* map, unsigned int hash, void* key);
int hashmap_insert(struct hashmap_t* map, unsigned int hash, void* key, const void* value);
int hashmap_delete(struct hashmap_t* map, unsigned int hash, void* key);

#endif /* _hashmap_h_ */
