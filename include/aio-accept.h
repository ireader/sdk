#ifndef _aio_accept_h_
#define _aio_accept_h_

#include "aio-socket.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Start Accept client
void* aio_accept_start(socket_t socket, aio_onaccept onaccept, void* param);

/// Cancel Accept
/// It will call aio_onaccept with code != 0 (in this or anthor thread)
/// @param[in] aio create by aio_accept_start
int aio_accept_stop(void* aio, aio_ondestroy ondestroy, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_accept_h_ */
