#include "torrent-sched.h"
#include "sched-piece.h"
#include "ipaddr-pool.h"
#include "torrent-internal.h"
#include "peer.h"
#include "piece.h"
#include "bitmap.h"
#include "sys/event.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/sync.hpp"
#include "cpm/shared_ptr.h"
#include "aio-connect.h"
#include "app-log.h"
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <list>

#define MIN(x, y) ( (x) < (y) ? (x) : (y))

#define N_PEER_PER_PIECE 8 // maximum 8-peers for one piece
#define N_PEER_INIT 10 // maximum 10-peer connection
#define N_PEER_TOTAL 50 // maximum peers

extern "C" int hash_sha1(const uint8_t* data, unsigned int bytes, uint8_t sha1[20]);

static int STDCALL torrent_sched_thread(void* param);

static int torrent_peer_create(torrent_sched_t* disp);
static int torrent_peer_destroy(peer::CPeer* peer);
static std::shared_ptr<peer::CPeer> torrent_peer_find(torrent_sched_t* disp, const struct sockaddr_storage* addr);
static std::shared_ptr<peer::CPeer> torrent_peer_bits(torrent_sched_t* disp, uint32_t piece);

static std::shared_ptr<peer::CPiece> torrent_piece_find(struct torrent_sched_t* disp, uint32_t piece);
static void torrent_piece_erase(torrent_sched_t* disp, uint32_t piece);
static void torrent_piece_broadcast(torrent_sched_t* disp, uint32_t piece);

struct torrent_sched_t
{
	ipaddr_pool_t* addrs;
	std::list<std::shared_ptr<peer::CPeer> > peers;
	std::list<std::shared_ptr<peer::CPiece> > pieces;
	ThreadLocker locker;

	bool running;
	event_t event;
	pthread_t thread;

	uint8_t infohash[20];
	uint8_t metainfo[512 * 1024];
	uint32_t metasize;
	struct torrent_t* tor;
	struct torrent_handler_t handler;
};

struct torrent_sched_t* torrent_sched_create(struct torrent_t* tor, struct torrent_handler_t* handler)
{
	torrent_sched_t* disp = new torrent_sched_t;
	memcpy(&disp->handler, handler, sizeof(disp->handler));
	disp->metasize = metainfo_info(tor->meta, disp->metainfo, sizeof(disp->metainfo));
	hash_sha1(disp->metainfo, disp->metasize, disp->infohash);
	disp->addrs = ipaddr_pool_create();
	disp->tor = tor;

	disp->running = true;
	event_create(&disp->event);
	thread_create(&disp->thread, torrent_sched_thread, disp);
	return disp;
}

void torrent_sched_destroy(struct torrent_sched_t* disp)
{
	if (disp->addrs)
	{
		ipaddr_pool_destroy(disp->addrs);
		disp->addrs = NULL;
	}

	disp->running = false;
	event_signal(&disp->event);
	thread_destroy(disp->thread);
	event_destroy(&disp->event);
	delete disp;
}

int torrent_sched_peers(torrent_sched_t* disp, const struct sockaddr_storage* addrs, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (torrent_peer_find(disp, addrs + i).get())
			continue;

		ipaddr_pool_push(disp->addrs, addrs + i);
	}

	event_signal(&disp->event); // dispatch
	return 0;
}

int torrent_sched_recv_piece(torrent_sched_t* disp, uint32_t piece, uint32_t bytes, const uint8_t hash[20])
{
	app_log(LOG_DEBUG, "recv piece(%u)\n", piece);
	auto p = std::make_shared<peer::CPiece>(piece, bytes, hash);
	if (!p.get())
		return ENOMEM;

	AutoThreadLocker locker(disp->locker);
	for(auto it = disp->pieces.begin(); it != disp->pieces.end(); ++it)
	{
		if ((*it)->GetPiece()->piece == piece)
			return EEXIST;
	}

	disp->pieces.push_back(p);
	event_signal(&disp->event);
	return 0;
}

int torrent_sched_send_piece(torrent_sched_t* disp, void* param, uint32_t piece, uint32_t begin, uint32_t length, const void* data)
{
	peer::CPeer* peer = (peer::CPeer*)param;
	assert(peer->m_disp == disp);
	app_log(LOG_INFO, "send piece(%u, %u, %u) to peer(%s:%hu)\n", piece, begin, length, peer->ip, peer->port);
	return peer_send_slices(peer->m_peer, piece, begin, length, (const uint8_t*)data);
}

static int torrent_sched_piece(torrent_sched_t* disp, peer::CPiece* piece)
{
	size_t slices = (size_t)((disp->tor->meta->piece_bytes + N_PIECE_SLICE - 1) / N_PIECE_SLICE);
	size_t num = DIV_ROUND_UP(slices, peer::CPeer::BITFILED);
	num = MIN(num, N_PEER_PER_PIECE);

	while (piece->Size() < num)
	{
		uint32_t begin, length;
		if (!piece->GetRegin(begin, length))
			return 0; // all done

		auto peer = torrent_peer_bits(disp, piece->GetPiece()->piece);
		if (!peer.get())
			break;

		piece->Add(peer, begin, length);
		// don't check peer_recv return value
		// timer will check peer bitrate and download bytes
		peer_recv(peer->m_peer, peer->piece, peer->begin, peer->bytes);
		app_log(LOG_DEBUG, "peer(%s:%hu) assign piece(%u, %u, %u) task\n", peer->ip, peer->port, peer->piece, peer->begin, peer->bytes);
	}

	size_t sz = piece->Size();
	return num > sz ? num - sz : 0;
}

static int torrent_sched_action(torrent_sched_t* disp)
{
	int init = 0;
	int expected = 0;

	// dispatch
	{
		AutoThreadLocker locker(disp->locker);
		for (auto it = disp->pieces.begin(); it != disp->pieces.end(); ++it)
		{
			expected += torrent_sched_piece(disp, it->get());
		}
	}

	// add more peers
	{
		AutoThreadLocker locker(disp->locker);
		for (auto it = disp->peers.begin(); it != disp->peers.end(); ++it)
		{
			if (peer::CPeer::HANDSHAKE >= (*it)->status)
				++init;
		}
	}

	size_t n = MIN(N_PEER_TOTAL, MIN(N_PEER_INIT, expected + N_PEER_PER_PIECE));
	for(size_t i = init; i < n; i++)
	{
		if (0 != torrent_peer_create(disp))
			break;
	}

	return 0;
}

static int torrent_sched_timeout(torrent_sched_t* disp)
{
	std::list<std::shared_ptr<peer::CPeer> > peers;

	// check working peer bitrate
	uint64_t clock = system_clock();
	{
		AutoThreadLocker locker(disp->locker);
		for (auto it = disp->peers.begin(); it != disp->peers.end(); ++it)
		{
			auto peer = *it;
			switch (peer->status)
			{
			case peer::CPeer::HANDSHAKE:
				if (peer->clock + peer::CPeer::TIMEOUT_HANDSHAKE < clock)
					peers.push_back(peer); // handshake + bitfield take too much times
				break;

			case peer::CPeer::WORKING:
				app_log(LOG_INFO, "peer(%s:%hu) piece: %u, total: %" PRIu64 ", bitrate: %f\n", peer->ip, peer->port, peer->piece, peer->total, peer->bitrate);

				assert(!peer->choke);
				if (peer->clock + 5000 > clock
					&& (peer->total < 100 * 1024 || (peer->bitrate > 1.0 && peer->bitrate < 10 * 1024.0)))
					peers.push_back(peer); // too slowly

			default:
				break;
			}
		}
	}

	for (auto it = peers.begin(); it != peers.end(); ++it)
	{
		auto peer = *it;
		app_log(LOG_INFO, "peer(%s:%hu) will be destroyed: piece: %u, total: %" PRIu64 ", bitrate: %f\n", peer->ip, peer->port, peer->piece, peer->total, peer->bitrate);
		torrent_peer_destroy(it->get());
	}

	if (peers.size() > 0)
	{
		event_signal(&disp->event);
		return 0;
	}

	// check piece working peer
	{
		size_t slices = (size_t)((disp->tor->meta->piece_bytes + N_PIECE_SLICE - 1) / N_PIECE_SLICE);
		size_t num = DIV_ROUND_UP(slices, peer::CPeer::BITFILED);
		num = MIN(num, N_PEER_PER_PIECE);

		uint32_t begin, length;
		AutoThreadLocker locker(disp->locker);
		for (auto it = disp->pieces.begin(); it != disp->pieces.end(); ++it)
		{
			if ((*it)->Size() < num && (*it)->GetRegin(begin, length))
			{
				event_signal(&disp->event);
				break;
			}
		}
	}

	return 0;
}

static int STDCALL torrent_sched_thread(void* param)
{
	torrent_sched_t* disp = (torrent_sched_t*)param;
	while (disp->running)
	{
		if (WAIT_TIMEOUT == event_timewait(&disp->event, 3000))
		{
			torrent_sched_timeout(disp);
		}
		else
		{
			torrent_sched_action(disp);
		}
	}
	return 0;
}

static int torrent_peer_create(torrent_sched_t* disp)
{
	struct sockaddr_storage addr;
	if (0 != ipaddr_pool_pop(disp->addrs, &addr))
	{
		app_log(LOG_INFO, "NOITFY: need more peer\n");
		disp->handler.notify(disp->tor->param, NOTIFY_NEED_MORE_PEER);
		return -1;
	}

	auto peer = std::make_shared<peer::CPeer>(disp, &addr);
	app_log(LOG_DEBUG, "create peer(%s:%hu)\n", peer->ip, peer->port);
	{
		AutoThreadLocker locker(disp->locker);
		disp->peers.push_back(peer);
	}

	return 0;
}

static int torrent_peer_destroy(peer::CPeer* peer)
{
	std::shared_ptr<peer::CPeer> ptr;
	torrent_sched_t* disp = peer->m_disp;
	AutoThreadLocker locker(disp->locker);
	for (auto it = disp->peers.begin(); it != disp->peers.end(); ++it)
	{
		if (0 == memcmp(&(*it)->addr, &peer->addr, sizeof(peer->addr)))
		{
			ptr = *it;
			disp->peers.erase(it);
			break;
		}
	}

	for (auto it = disp->pieces.begin(); it != disp->pieces.end(); ++it)
	{
		(*it)->Delete(peer);
	}

	return 0;
}

static std::shared_ptr<peer::CPeer> torrent_peer_find(torrent_sched_t* disp, const struct sockaddr_storage* addr)
{
	AutoThreadLocker locker(disp->locker);
	for (auto it = disp->peers.begin(); it != disp->peers.end(); ++it)
	{
		if (0 == memcmp(&(*it)->addr, addr, sizeof(*addr)))
			return *it;
	}
	return std::shared_ptr<peer::CPeer>();
}

static std::shared_ptr<peer::CPeer> torrent_peer_bits(torrent_sched_t* disp, uint32_t piece)
{
	for (auto it = disp->peers.begin(); it != disp->peers.end(); ++it)
	{
		if (peer::CPeer::IDLE != (*it)->status || (*it)->choke)
			continue;

		if ((*it)->bits > piece && bitmap_test_bit((*it)->bitmap, piece))
			return *it;
	}

	return std::shared_ptr<peer::CPeer>();
}

static std::shared_ptr<peer::CPiece> torrent_piece_find(struct torrent_sched_t* disp, uint32_t piece)
{
	AutoThreadLocker locker(disp->locker);
	for (auto it = disp->pieces.begin(); it != disp->pieces.end(); ++it)
	{
		if ((*it)->GetPiece()->piece == piece)
			return *it;
	}

	return std::shared_ptr<peer::CPiece>();
}

static void torrent_piece_erase(torrent_sched_t* disp, uint32_t piece)
{
	std::shared_ptr<peer::CPiece> ptr;
	{
		AutoThreadLocker locker(disp->locker);
		for (auto it = disp->pieces.begin(); it != disp->pieces.end(); ++it)
		{
			if (piece == (*it)->GetPiece()->piece)
			{
				ptr = *it;
				disp->pieces.erase(it);
				break;
			}
		}
	}

	ptr->Clear();
}

static void torrent_piece_broadcast(torrent_sched_t* disp, uint32_t piece)
{
	AutoThreadLocker locker(disp->locker);
	for (auto it = disp->peers.begin(); it != disp->peers.end(); ++it)
	{
		auto peer = *it;
		if (peer::CPeer::HANDSHAKE >= peer->status)
			continue;
		peer_have(peer->m_peer, piece);
	}
}

namespace peer
{
	static int onerror(void* param, int code)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		torrent_sched_t* disp = peer->m_disp;
		app_log(LOG_ERROR, "peer(%s:%hu) error: %d\n", peer->ip, peer->port, code);

		// remove peer
		torrent_peer_destroy(peer);
		event_signal(&disp->event); // re-dispatch
		return 0;
	}

	static int onhandshake(void* param, const uint8_t flags[8], const uint8_t info_hash[20], const uint8_t peer_id[20])
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		torrent_sched_t* disp = peer->m_disp;
		app_log(LOG_DEBUG, "peer(%s:%hu) handshake flags: 0x%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX\n", peer->ip, peer->port, flags[0], flags[1], flags[2], flags[3], flags[4], flags[5], flags[6], flags[7]);

		assert(0 == memcmp(info_hash, peer->m_disp->infohash, sizeof(peer->m_disp->infohash)));
		int r = peer_extended(peer->m_peer, peer->m_disp->tor->port, VERSION, peer->m_disp->metasize);
		if (0 == r && 0 != bitmap_weight(disp->tor->bitfield, disp->tor->meta->piece_count))
			r = peer_bitfield(peer->m_peer, disp->tor->bitfield, disp->tor->meta->piece_count);
		return r;
	}

	static int onchoke(void* param, int choke)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		torrent_sched_t* disp = peer->m_disp;
		app_log(LOG_DEBUG, "peer(%s:%hu) choke: %d\n", peer->ip, peer->port, choke);

		peer->choke = !!choke;
		if (choke)
		{
			// remove peer
			auto piece = torrent_piece_find(disp, peer->piece);
			piece->Delete(peer);
		}

		event_signal(&disp->event); // re-dispatch
		return 0;
	}

	static int oninterested(void* param, int interested)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		app_log(LOG_DEBUG, "peer(%s:%hu) interested: %d\n", peer->ip, peer->port, interested);
		return 0;
	}

	static int onhave(void* param, uint32_t piece)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		app_log(LOG_DEBUG, "peer(%s:%hu) have piece: %u\n", peer->ip, peer->port, piece);
		event_signal(&peer->m_disp->event); // re-dispatch
		return 0;
	}

	static int onbitfield(void* param, const uint8_t* bitfield, uint32_t bits)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		torrent_sched_t* disp = peer->m_disp;
		assert(bits >= disp->tor->meta->piece_count);
		bits = disp->tor->meta->piece_count;
		uint32_t weight = bitmap_weight(bitfield, bits);
		app_log(LOG_DEBUG, "peer(%s:%hu) bitfield bits: %u/%u\n", peer->ip, peer->port, weight, bits);

		bitmap_or((uint8_t*)bitfield, disp->tor->bitfield, bitfield, bits);
		if (bitmap_weight(bitfield, bits) > bitmap_weight(disp->tor->bitfield, bits))
		{
			peer_choke(peer->m_peer, 0);
			peer_interested(peer->m_peer, 1);

			peer->bits = bits;
			peer->bitmap = bitfield;
			peer->status = CPeer::IDLE;
		}
		else
		{
			app_log(LOG_INFO, "peer(%s:%hu) not interested\n", peer->ip, peer->port);
			torrent_peer_destroy(peer);
		}

		event_signal(&disp->event); // re-dispatch
		return 0;
	}

	static int onpiece(void* param, uint32_t piece, uint32_t begin, uint32_t length, const void* block)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		torrent_sched_t* disp = peer->m_disp;
		assert(0 == begin % N_PIECE_SLICE);
		assert(0 == length % N_PIECE_SLICE);
		app_log(LOG_DEBUG, "peer(%s:%hu) piece: %u, begin: %u, length: %u\n", peer->ip, peer->port, piece, begin, length);

		std::shared_ptr<CPiece> ptr = torrent_piece_find(disp, piece);
		if (!ptr.get()) return 0; // ignore

		// update bitrate
		peer->total += length;
		uint64_t clock = system_clock();
		if (clock > peer->clock)
			peer->bitrate = peer->total * 1000.0 / (clock - peer->clock);

		// update bitfield(task bitmap)
		for (uint32_t i = begin; i < begin + length; i += N_PIECE_SLICE)
		{
			assert((i - peer->begin) / N_PIECE_SLICE < 32);
			if (i >= peer->begin && i <= peer->begin + peer->bytes)
				bitmap_set(peer->bitfield, (i - peer->begin) / N_PIECE_SLICE, 1);
		}

		if (piece_write(ptr->GetPiece(), begin, length, (const uint8_t*)block))
		{
			torrent_piece_erase(disp, piece);
			if (piece_check(ptr->GetPiece()))
			{
				bitmap_set(disp->tor->bitfield, piece, 1);
				disp->handler.piece(disp->tor->param, piece, ptr->GetPiece()->data, ptr->GetPiece()->bytes);
				torrent_piece_broadcast(disp, piece);
			}
			else
			{
				// download invalid piece, retry
				AutoThreadLocker locker(disp->locker);
				disp->pieces.push_back(ptr);
			}
		}
		else
		{
			// task done
			uint32_t slices = (peer->bytes + N_PIECE_SLICE - 1) / N_PIECE_SLICE;
			if (slices == bitmap_weight(peer->bitfield, slices))
			{
				ptr->Delete(peer);
			}
		}

		event_signal(&peer->m_disp->event); // re-dispatch
		return 0;
	}

	static int onsend(void* param, uint32_t piece, uint32_t begin, uint32_t length)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		app_log(LOG_DEBUG, "send piece(%u, %u, %u) to peer(%s:%hu)\n", piece, begin, length, peer->ip, peer->port);
		return 0;
	}

	static int request(void* param, uint32_t piece, uint32_t begin, uint32_t length)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		app_log(LOG_DEBUG, "peer(%s:%hu) request piece(%u, %u, %u) \n", peer->ip, peer->port, piece, begin, length);

		// request piece
		peer->m_disp->handler.request(peer->m_disp->tor->param, peer, piece, begin, length);
		return 0;
	}

	static int metadata(void* param, const uint8_t info_hash[20])
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		app_log(LOG_DEBUG, "peer(%s:%hu) request metadata\n", peer->ip, peer->port);

		// request metadata
		return 0;
	}

	static void OnConnected(void* param, int code, aio_socket_t aio)
	{
		peer::CPeer* peer = (peer::CPeer*)param;
		torrent_sched_t* disp = peer->m_disp;
		assert(CPeer::CONNECT == peer->status);
		app_log(LOG_DEBUG, "peer(%s:%hu) OnConnected: %d\n", peer->ip, peer->port, code);
		if (0 == code)
		{
			struct peer_handler_t handler;
			memset(&handler, 0, sizeof(handler));
			handler.handshake = peer::onhandshake;
			handler.bitfield = peer::onbitfield;
			handler.interested = peer::oninterested;
			handler.choke = peer::onchoke;
			handler.have = peer::onhave;
			handler.piece = peer::onpiece;
			handler.request = peer::request;
			//handler.send = peer::onsend;
			handler.metadata = peer::metadata;
			handler.error = peer::onerror;

			peer->status = CPeer::HANDSHAKE;
			peer->m_peer = peer_create(aio, &peer->addr, &handler, peer);
			peer->clock = system_clock();
			peer_handshake(peer->m_peer, disp->infohash, disp->tor->id);
		}
		else
		{
			torrent_peer_destroy(peer);
			event_signal(&disp->event);
		}
	}

	CPeer::CPeer(struct torrent_sched_t* disp, const struct sockaddr_storage* addr)
		:status(CPeer::CONNECT), bitrate(0.0), choke(true), bitmap(NULL), bits(0)
	{
		m_peer = NULL;
		m_disp = disp;

		clock = system_clock();
		memcpy(&this->addr, addr, sizeof(struct sockaddr_storage));
		socket_addr_to((const struct sockaddr*)addr, AF_INET6 == addr->ss_family ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in), ip, &port);
		aio_connect(ip, port, CPeer::TIMEOUT_CONNECT, OnConnected, this);
	}

	CPeer::~CPeer()
	{
		app_log(LOG_DEBUG, "peer(%s:%hu) destroy\n", ip, port);
		if (m_peer)
			peer_destroy(m_peer);
		m_peer = NULL;
		m_disp = NULL;
		status = -1;
	}
}
