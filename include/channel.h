#ifndef _channel_h_
#define _channel_h_

#if defined(__cplusplus)
extern "C" {
#endif

struct channel_t;

struct channel_t* channel_create(int capacity, int elementsize);
void channel_destroy(struct channel_t** q);

//void channel_clear(struct channel_t* q);
int channel_count(struct channel_t* q);

// block push/pop
int channel_push(struct channel_t* q, const void* e);
int channel_pop(struct channel_t* q, void* e);

#if defined(__cplusplus)
}
#endif
#endif /* !_channel_h_ */
