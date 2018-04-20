#ifndef _work_queue_h_
#define _work_queue_h_

// FIFO work queue

#if defined(__cplusplus)
extern "C" {
#endif

struct work_queue_t;

typedef void (*work_queue_worker)(void* param, struct work_queue_t* q);

struct work_queue_t* work_queue_create(int size, int capacity, work_queue_worker worker, void* param);
void work_queue_destroy(struct work_queue_t* q);
void work_queue_clear(struct work_queue_t* q);
int work_queue_count(struct work_queue_t* q);

int work_queue_push(struct work_queue_t* q, const void* job);
const void* work_queue_pop(struct work_queue_t* q);

void work_queue_process(struct work_queue_t* q);
struct work_queue_t* work_queue_dispatch(int timeoutMS);

#if defined(__cplusplus)
}
#endif

#endif /* !_work_queue_h_ */
