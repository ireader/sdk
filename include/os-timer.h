#ifndef _platform_timer_h_
#define _platform_timer_h_

typedef void*			timer_t;
typedef void (*timer_func)(timer_t id, void* param);

int timer_init();
int timer_cleanup();

int timer_add(timer_t *id, int period, timer_func callback, void* cbparam);
int timer_remove(timer_t *id);

#endif /* !_platform_timer_h_ */
