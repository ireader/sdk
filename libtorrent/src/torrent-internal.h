#ifndef _torrent_internal_h_
#define _torrent_internal_h_

#include "sys/sock.h"
#include "sys/locker.h"
#include "torrent.h"
#include "piece.h"
#include "heap.h"
#include <stdint.h>

#define __STDC_FORMAT_MACROS
#define VERSION "-XL0012-"

struct torrent_sched_t;

struct torrent_t
{
	const struct metainfo_t* meta;

	size_t piece_count;
	struct piece_t** pieces;
	struct heap_t *peers;

	int concurrent; // maximum download pieces
	uint8_t* bitfield;

	uint8_t id[20];
	uint16_t port;
	locker_t locker;
	struct torrent_sched_t* sched;
	void* param;
};

#endif /* !_torrent_internal_h_ */
