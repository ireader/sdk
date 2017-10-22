#ifndef _torrent_internal_h_
#define _torrent_internal_h_

#include "torrent.h"
#include "piece.h"
#include "list.h"
#include <stdint.h>

#define __STDC_FORMAT_MACROS
#define VERSION "-XL0012-"

struct torrent_sched_t;

struct torrent_t
{
	const struct metainfo_t* meta;

	uint8_t* bitfield;
	int pieces; // maximum download pieces

	uint8_t id[20];
	uint16_t port;
	struct torrent_sched_t* disp;
	void* param;
};

#endif /* !_torrent_internal_h_ */
