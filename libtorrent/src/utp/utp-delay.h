#ifndef _utp_delay_h_
#define _utp_delay_h_

#include <stdlib.h>
#include <stdint.h>

#define N_DELAY 300

struct utp_delay_t
{
	int32_t delay[N_DELAY]; // ring buffer
	int offset;
	int num;
};

static inline struct utp_delay_t* utp_delay_create()
{
	struct utp_delay_t* delay;
	delay = (struct utp_delay_t*)calloc(1, sizeof(*delay));
	if (!delay)
		return NULL;

	return delay;
}

static inline void utp_delay_destroy(struct utp_delay_t* delay)
{
	free(delay);
}

static inline void utp_delay_push(struct utp_delay_t* delay, int32_t value)
{
	int pos;
	pos = (delay->offset + delay->num) % N_DELAY;
	delay->delay[pos] = value;
	if (delay->num < N_DELAY)
		++delay->num;
	else
		delay->offset = (delay->offset + 1) % N_DELAY;
}

static inline uint32_t utp_delay_get(struct utp_delay_t* delay)
{
	int i;
	int32_t min, value;

	// if 0 == deylay->num
	min = delay->delay[delay->offset];
	for (i = 1; i < delay->num; i++)
	{
		value = delay->delay[(delay->offset + i) % N_DELAY];
		if(min < value)
			min = value;
	}
	return min;
}

#endif /* !_utp_delay_h_ */
