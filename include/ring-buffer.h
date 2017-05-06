#ifndef _ring_buffer_h_
#define _ring_buffer_h_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* ring_buffer_create(size_t bytes);
void ring_buffer_destroy(void* rb);
void ring_buffer_clear(void* rb);

int ring_buffer_write(void* rb, const void* data, size_t bytes);
int ring_buffer_read(void* rb, void* data, size_t bytes);

/// @return readable element count
size_t ring_buffer_size(void* rb);

#ifdef __cplusplus
}
#endif
#endif /* !_ring_buffer_h_ */
