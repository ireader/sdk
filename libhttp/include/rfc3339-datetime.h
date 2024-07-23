#ifndef _rfc3339_datetime_h_
#define _rfc3339_datetime_h_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char rfc3339_datetime_t[21];

const char* rfc3339_datetime_format(time_t time, rfc3339_datetime_t datetime);

#ifdef __cplusplus
}
#endif
#endif /* !_rfc3339_datetime_h_ */
