#ifndef _platform_timer_h_
#define _platform_timer_h_

typedef void*			timer_t;
#define timer_invalid	NULL


typedef void (*fcbTimer)(timer_t id, void* param);
int timer_create(timer_t* id, int period, fcbTimer callback, void* param);
int timer_destroy(timer_t id);

int timer_setperiod(timer_t id, int period);
int timer_getperiod(timer_t id, int* period);

#endif /* !_platform_timer_h_ */
