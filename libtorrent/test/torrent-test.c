#include "torrent.h"
#include "tracker.h"
#include "bitmap.h"
#include "aio-worker.h"
#include "sys/system.h"
#include "sys/atomic.h"
#include <assert.h>
#include <stdio.h>

struct torrent_test_t
{
	uint16_t port;
	uint8_t id[20];
	uint8_t sha1[20];

	torrent_t* tor;
	struct tracker_t* tracker[5];
	const struct metainfo_t* metainfo;

	int32_t next;
};

static void tracker_onreply(void* param, int code, const struct tracker_reply_t* reply)
{
	struct torrent_test_t* ctx;
	ctx = (struct torrent_test_t*)param;
	if (0 == code)
	{
		FILE* fp = fopen("peers.bin", "wb");
		fwrite(reply->peers, sizeof(struct sockaddr_storage), reply->peer_count, fp);
		fclose(fp);

		torrent_input_peers(ctx->tor, reply->peers, reply->peer_count);
	}
}

static void tracker_update(struct torrent_test_t* ctx)
{
	size_t i;
	const struct metainfo_t* metainfo = ctx->metainfo;
	for (i = 0; i < metainfo->tracker_count && i < 5; i++)
	{
		assert(0 == tracker_query(ctx->tracker[i], 0, metainfo->piece_bytes * metainfo->piece_count, 0, TRACKER_EVENT_STARTED, tracker_onreply, ctx));
	}

	//struct sockaddr_storage addr[200];
	//FILE* fp = fopen("peers.bin", "rb");
	//int r = fread(addr, sizeof(struct sockaddr_storage), sizeof(addr)/sizeof(addr[0]), fp);
	//fclose(fp);

	////socket_addr_from_ipv4(addr, "211.186.235.32", 13393);
	//torrent_input_peers(ctx->tor, addr, r);
}

static void torrent_onnotify(void* param, int notify)
{
	struct torrent_test_t* ctx;
	ctx = (struct torrent_test_t*)param;

	switch (notify)
	{
	case NOTIFY_NEED_MORE_PEER:
		//tracker_update(ctx);
		break;

	default:
		break;
	}
}

static void torrent_onpiece(void* param, uint32_t piece, const void* data, uint32_t bytes)
{
	FILE* fp;
	char name[64];
	struct torrent_test_t* ctx;
	ctx = (struct torrent_test_t*)param;

	assert(bytes == ctx->metainfo->piece_bytes);
	sprintf(name, "%u.bin", piece);
	fp = fopen(name, "wb");
	fwrite(data, 1, bytes, fp);
	fclose(fp);

	torrent_get_piece(ctx->tor, atomic_increment32(&ctx->next));
}

static void torrent_onrequest(void* param, void* peer, uint32_t piece, uint32_t begin, uint32_t length)
{
	struct torrent_test_t* ctx;
	ctx = (struct torrent_test_t*)param;
}

void torrent_test(const struct metainfo_t* metainfo)
{
	size_t i;
	struct torrent_test_t ctx;
	struct torrent_handler_t handler;

	aio_worker_init(4);
	handler.piece = torrent_onpiece;
	handler.request = torrent_onrequest;
	handler.notify = torrent_onnotify;

	memset(&ctx, 0, sizeof(ctx));
	ctx.port = 15000;
	ctx.metainfo = metainfo;
	metainfo_hash(metainfo, ctx.sha1);
	torrent_peer_id("libtorrent", ctx.id);
	for (i = 0; i < metainfo->tracker_count && i < 5; i++)
	{
		ctx.tracker[i] = tracker_create(metainfo->trackers[i], ctx.sha1, ctx.id, ctx.port);
	}

	ctx.tor = torrent_create(metainfo, ctx.id, 15000, &handler, &ctx);
	tracker_update(&ctx);

	ctx.next = 1;
	torrent_get_piece(ctx.tor, 0);
	torrent_get_piece(ctx.tor, 1);

	while (1)
	{
		uint8_t* bitfield;
		uint32_t bits;

		system_sleep(2000);
		torrent_get_bitfield(ctx.tor, &bitfield, &bits);
		if (bits == bitmap_weight(bitfield, bits))
			break;
	}

	torrent_destroy(ctx.tor);
	for (i = 0; i < metainfo->tracker_count && i < 5; i++)
	{
		tracker_destroy(ctx.tracker[i]);
	}
	aio_worker_clean(4);
}
