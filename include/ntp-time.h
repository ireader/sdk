#ifndef _ntp_time_h_
#define _ntp_time_h_

#ifdef __cplusplus
extern "C"{
#endif

#if defined(OS_WINDOWS)
	typedef unsigned __int64 ntp64_t;
#else
	typedef unsigned long long ntp64_t;
#endif

/// NTP 64-bit timestamp
/// high 32-bit unsigned seconds since January 1, 1900 (UTC)
/// low 32-bit fraction field resolving 232(10^12 / 2^32) picoseconds
ntp64_t ntp64_now(void);


#ifdef __cplusplus
}
#endif
#endif /* !_ntp_time_h_ */
