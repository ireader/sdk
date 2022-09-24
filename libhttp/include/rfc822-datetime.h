#ifndef _rfc822_datetime_h_
#define _rfc822_datetime_h_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char rfc822_datetime_t[30];

const char* rfc822_datetime_format(time_t time, rfc822_datetime_t datetime);

#ifdef __cplusplus
}
#endif
#endif /* !_rfc822_datetime_h_ */
