#ifndef _peer_extended_h_
#define _peer_extended_h_

#include <stdint.h>
#include <stddef.h>

enum peer_extended_id_t
{
	BT_EXTENDED_HANDSHAKE = 0,
	BT_EXTENDED_METADATA,		// ut_metadata
	BT_EXTENDED_PEX,			// ut_pex
	BT_EXTENDED_HOLEPUNCH,		// ut_holepunch
};

struct peer_extended_t
{
	int32_t encryption;	// e
	int32_t reqq;		// reqq: An integer, the number of outstanding request messages this client supports without dropping any. The default in in libtorrent is 250.
	int32_t port;		// p: Local TCP listen port
	uint8_t ip[16];		// youip: 4-IPv4, 16-IPv6
	uint8_t ipv6;		// 0-IPv4, 1-IPv6
	char version[64];	// v: Client name and version (as a utf-8 string)

	struct
	{
		int32_t pex;
		int32_t metadata;
		int32_t holepunch;
		int32_t tex;
	} m;

	int32_t metasize;
};

enum peer_pex_flag_t
{
	PEX_ENCRYPTION	= 0x01, // prefers encryption, as indicated by e field in extension handshake
	PEX_UPLOAD_ONLY = 0x02, // seed/upload_only
	PEX_SUPPORT_UTP = 0x04, // supports uTP
	PEX_HOLEPUNCH	= 0x08, // peer indicated ut_holepunch support in extension handshake
	PEX_OUTGOING	= 0x10, // outgoing connection, peer is reachable
};

struct peer_pex_t
{
	uint8_t* flags;
	size_t n_flags;

	struct sockaddr_storage* added;
	size_t n_added;

	struct sockaddr_storage* dropped;
	size_t n_dropped;
};

struct peer_metadata_handler_t
{
	int (*request)(void* param, uint32_t piece);
	int (*reject)(void* param, uint32_t piece);
	int (*data)(void* param, uint32_t piece, const uint8_t* data, uint32_t size);
};

int peer_extended_read(const uint8_t* buffer, int bytes, struct peer_extended_t* ext);
int peer_extended_write(uint8_t* buffer, int bytes, uint16_t port, const char* version, int32_t metasize);

int peer_pex_read(const uint8_t* buffer, int bytes, struct peer_pex_t* pex);
int peer_pex_write(uint8_t* buffer, int bytes, const struct peer_pex_t* pex);

int peer_tex_read(const uint8_t* buffer, int bytes);

int peer_metadata_read(const uint8_t* buffer, uint32_t bytes, struct peer_metadata_handler_t* handler, void* param);
int peer_metadata_request_write(uint8_t* buffer, uint32_t bytes, uint32_t piece);
int peer_metadata_reject_write(uint8_t* buffer, uint32_t bytes, uint32_t piece);
int peer_metadata_data_write(uint8_t* buffer, uint32_t bytes, uint32_t piece, const uint8_t* data, uint32_t size);

#endif /* !_peer_extended_h_ */
