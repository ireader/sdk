#ifndef _channel_h_
#define _channel_h_

#if defined(__cplusplus)
extern "C" {
#endif

struct channel_t;

struct channel_t* channel_create(int capacity, int elementsize);
void channel_destroy(struct channel_t** pc);

//void channel_clear(struct channel_t* c);
int channel_count(struct channel_t* c);

// block push/pop
int channel_push(struct channel_t* c, const void* e);
int channel_pop(struct channel_t* c, void* e);

/// @param[in] timeout MS
/// @return 0-success, WAIT_TIMEOUT-timeout, other-error
int channel_push_timeout(struct channel_t* c, const void* e, int timeout);
int channel_pop_timeout(struct channel_t* c, void* e, int timeout);

#if defined(__cplusplus)
}
#endif
#endif /* !_channel_h_ */
