#ifndef _sched_piece_h_
#define _sched_piece_h_

#include "piece.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/sync.hpp"
#include "cpm/shared_ptr.h"
#include "bitmap.h"
#include <vector>
#include <assert.h>

#define DIV_ROUND_UP(v, n) ( ((v) + (n) - 1) / (n) )
#define PIECE_BYTES_TO_SLICES(bytes) DIV_ROUND_UP(bytes, N_PIECE_SLICE)

namespace peer
{
	enum { INIT = 0, IDLE, WORKING };
	struct CPeer
	{
		enum { BITFILED = 32 };

		struct peer_t* m_peer;
		struct torrent_sched_t* m_disp;
		struct sockaddr_storage addr;
		char ip[SOCKET_ADDRLEN];
		uint16_t port;
		int status; // 0-init, 1-idle, 2-work

		uint64_t clock;
		uint64_t total;
		double bitrate;

		uint32_t piece;
		uint32_t begin;
		uint32_t bytes;
		uint8_t bitfield[DIV_ROUND_UP(BITFILED, 8)];

		bool choke;
		const uint8_t* bitmap;
		uint32_t bits;

		CPeer(struct torrent_sched_t* disp, const struct sockaddr_storage* addr);
		~CPeer();
	};

	static bool compare(const std::shared_ptr<CPeer>& l, const std::shared_ptr<CPeer>& r)
	{
		return l->begin < r->begin;
	}

	class CPiece
	{
		mutable ThreadLocker m_locker;
		mutable uint8_t* m_bitfield; // task assign bitmap
		std::vector<std::shared_ptr<CPeer> > m_peers;
		piece_t* m_piece;

	public:
		CPiece(uint32_t piece, uint32_t bytes, const uint8_t sha1[20])
		{
			m_piece = piece_create(piece, bytes, sha1);
			m_bitfield = new uint8_t[DIV_ROUND_UP(PIECE_BYTES_TO_SLICES(bytes), 8)];
		}

		~CPiece()
		{
			Clear();

			if (m_piece)
				piece_destroy(m_piece);
			m_piece = NULL;

			delete[] m_bitfield;
		}

		piece_t* GetPiece() const 
		{ 
			return m_piece;
		}

	public:
		void Clear()
		{
			AutoThreadLocker guard(m_locker);
			for (auto it = m_peers.begin(); it != m_peers.end(); ++it)
			{
				(*it)->status = peer::IDLE;
			}

			m_peers.clear();
		}

		bool Add(std::shared_ptr<CPeer>& peer, uint32_t begin, uint32_t length)
		{
			assert(begin + length <= PIECE_BYTES_TO_SLICES(m_piece->bytes));
			peer->status = peer::WORKING;
			peer->clock = system_clock();
			peer->total = 0;
			peer->piece = m_piece->piece;
			peer->begin = begin * N_PIECE_SLICE;
			peer->bytes = length * N_PIECE_SLICE;
			memset(peer->bitfield, 0, sizeof(peer->bitfield));

			AutoThreadLocker guard(m_locker);
			m_peers.push_back(peer);
			return true;
		}

		bool Delete(CPeer* peer)
		{
			AutoThreadLocker guard(m_locker);
			for (auto it = m_peers.begin(); it != m_peers.end(); ++it)
			{
				if (0 == memcmp(&(*it)->addr, &peer->addr, sizeof(peer->addr)))
				{
					assert(peer::WORKING == peer->status);
					assert(m_piece->piece == peer->piece);
					peer->status = peer::IDLE;
					m_peers.erase(it);
					return true;
				}
			}
			return false;
		}

		size_t Size() const
		{
			AutoThreadLocker guard(m_locker);
			return m_peers.size();
		}

		bool GetRegin(uint32_t& begin, uint32_t& length) const
		{
			unsigned int bits = PIECE_BYTES_TO_SLICES(m_piece->bytes);
			AutoThreadLocker guard(m_locker);
			memcpy(m_bitfield, m_piece->bitfield, DIV_ROUND_UP(bits, 8));
			for (auto it = m_peers.begin(); it != m_peers.end(); ++it)
			{
				bitmap_set(m_bitfield, (*it)->begin / N_PIECE_SLICE, (*it)->bytes / N_PIECE_SLICE);
			}

			begin = bitmap_find_first_zero(m_bitfield, bits);
			if (begin >= bits)
			{
				assert(bits = bitmap_weight(m_bitfield, bits));
				return false;
			}

			length = bitmap_count_next_zero(m_bitfield, bits, begin);
			length = CPeer::BITFILED > length ? length : CPeer::BITFILED;
			return true;
		}
	};
}

#endif /* !_sched_piece_h_ */
