#include "udp-buffer.h"
#include <stdlib.h>
#include <assert.h>

struct udp_buffer_t
{
    void* ptr;
    int bytes;
    int offset;
    int capacity;
};

struct udp_buffer_t* udp_buffer_create(int bytes)
{
    struct udp_buffer_t* buffer;
    buffer = (struct udp_buffer_t*)malloc(sizeof(*buffer) + bytes);
    if (buffer)
    {
        buffer->ptr = buffer + 1;
        buffer->capacity = bytes;
        buffer->offset = 0;
        buffer->bytes = 0;
    }

    return buffer;
}

void udp_buffer_destroy(struct udp_buffer_t* buffer)
{
    if (buffer)
        free(buffer);
}

void* udp_buffer_ptr(struct udp_buffer_t* buffer)
{
    int bytes;
    assert(buffer->capacity > buffer->offset);
    bytes = buffer->capacity - buffer->offset;

    // check ring buffer remain size
    if (bytes > buffer->bytes / 2)
        return (char*)buffer->ptr + buffer->offset;
    return buffer->ptr;
}

int udp_buffer_size(struct udp_buffer_t* buffer)
{
    int bytes;
    
    if (buffer->offset + buffer->bytes <= buffer->capacity)
        return buffer->bytes;
    
    // check ring buffer remain size
    assert(buffer->capacity > buffer->offset);
    bytes = buffer->capacity - buffer->offset;
    return bytes > buffer->bytes / 2 ? bytes : (buffer->bytes - bytes);
}

void udp_buffer_lock(struct udp_buffer_t* buffer, int bytes)
{
    assert(buffer->bytes >= bytes);
    buffer->offset += bytes;
    buffer->bytes -= bytes;
}

void udp_buffer_unlock(struct udp_buffer_t* buffer, int bytes)
{
    assert(buffer->bytes + bytes <= buffer->capacity);
    buffer->bytes += bytes;
}
