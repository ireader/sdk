#ifndef _ring_buffer_h_
#define _ring_buffer_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ring_buffer_t
{
	uint8_t* ptr;
	size_t capacity;

	size_t offset; // read position
	size_t count;
};

int ring_buffer_alloc(struct ring_buffer_t* rb, size_t capacity);
int ring_buffer_free(struct ring_buffer_t* rb);
void ring_buffer_clear(struct ring_buffer_t* rb);

int ring_buffer_write(struct ring_buffer_t* rb, const void* data, size_t bytes);
int ring_buffer_read(struct ring_buffer_t* rb, void* data, size_t bytes);

/// @return readable element count
size_t ring_buffer_size(struct ring_buffer_t* rb);
/// @return writeable element count
size_t ring_buffer_space(struct ring_buffer_t* rb);

/// extend ring buffer memory
/// @return 0-ok, other-error
int ring_buffer_resize(struct ring_buffer_t* rb, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* !_ring_buffer_h_ */
