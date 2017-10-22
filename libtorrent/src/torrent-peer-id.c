#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "torrent-internal.h"

int hash_sha1(const uint8_t* data, unsigned int bytes, uint8_t sha1[20]);

int torrent_peer_id(const char* usr, uint8_t id[20])
{
	hash_sha1((const uint8_t*)usr, strlen(usr), id);
	memcpy(id, VERSION, strlen(VERSION));
	return 0;
}
