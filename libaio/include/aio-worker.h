#ifndef _aio_worker_h_
#define _aio_worker_h_

#if defined(__cplusplus)
extern "C" {
#endif

/// call aio_socket_init and create aio worker thread(aio_socket_process)
/// @param[in] num aio worker thread count
void aio_worker_init(int num);
void aio_worker_clean(int num);

#if defined(__cplusplus)
}
#endif
#endif /* !_aio_worker_h_ */
