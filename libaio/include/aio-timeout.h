#ifndef _aio_timeout_h_
#define _aio_timeout_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct aio_timeout_t
{
	struct aio_timeout_t* prev; // internal use only
	struct aio_timeout_t* next; // internal use only

	int32_t ref; // internal use only
	uint64_t clock; // set by aio_timeout_active
	int timeout; // set by aio_timeout_settimeout
	int enable; // set aio_timeout_enable

	void (*notify)(void* param); // timeout callback, can't be NULL
	void* param; // user-defined parameter

	void (*cancel)(void* param);
	void* param2;
};

void aio_timeout_init(void);
void aio_timeout_clean(void);
void aio_timeout_process(void);

/// add aio timeout notify(disable status)
void aio_timeout_add(struct aio_timeout_t* timeout, int timeoutMS, void (*notify)(void* param), void* param);
/// Delete timeout notify
void aio_timeout_delete(struct aio_timeout_t* timeout, void (*cancel)(void* param), void* param);

void aio_timeout_start(struct aio_timeout_t* timeout);
void aio_timeout_stop(struct aio_timeout_t* timeout);

void aio_timeout_settimeout(struct aio_timeout_t* timeout, int ms);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_timeout_h_ */
