#ifndef _ice_transport_h_
#define _ice_transport_h_

#include "rtsp-media.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ice_transport_t;

struct ice_transport_handler_t
{
	/// receive data
	void (*ondata)(void* param, int stream, int component, const void* data, int bytes);

	/// Gather server reflexive and relayed candidates
	void (*onbind)(void* param, int code);

	/// ICE nominated
	/// @param[in] flags connected stream bitmask flags, base 0, from Least Significant Bit(LSB), 1-connected, 0-failed
	/// @param[in] mask all streams, base 0, from Least Significant Bit(LSB), 1-connected, 0-failed
	void (*onconnected)(void* param, uint64_t flags, uint64_t mask);
};

struct ice_transport_t* ice_transport_create(int controlling, struct ice_transport_handler_t* handler, void* param);
int ice_transport_destroy(struct ice_transport_t* avt);

/// Get media stream sdp attribute(connect, candidate, ...)
int ice_transport_getsdp(struct ice_transport_t* avt, int stream, char* buf, int bytes);

int ice_transport_getaddr(struct ice_transport_t* avt, int stream, int component, struct sockaddr_storage* local);

int ice_transport_bind(struct ice_transport_t* avt, int stream, int component, const struct sockaddr* stun, int turn, const char* usr, const char* pwd);

int ice_transport_connect(struct ice_transport_t* avt, const struct rtsp_media_t* avmedia, int count);

int ice_transport_send(struct ice_transport_t* avt, int stream, int component, const void* data, int bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_ice_transport_h_ */
