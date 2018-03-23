#ifndef _udp_buffer_h_
#define _udp_buffer_h_

struct udp_buffer_t;

/// @bytes[in] buffer size in byte
struct udp_buffer_t* udp_buffer_create(int bytes);
void udp_buffer_destroy(struct udp_buffer_t* buffer);

/// Get idle buffer pointer
void* udp_buffer_ptr(struct udp_buffer_t* buffer);
/// Get idle buffer size
int udp_buffer_size(struct udp_buffer_t* buffer);

/// Take bytes from buffer
void udp_buffer_lock(struct udp_buffer_t* buffer, int bytes);
/// Return byte to buffer(must in order)
void udp_buffer_unlock(struct udp_buffer_t* buffer, int bytes);

#endif /* !_udp_buffer_h_ */
