#ifndef _ctypedef_h_
#define _ctypedef_h_

#define bool_true	1
#define bool_false	0

typedef unsigned char	bool_t;
typedef unsigned char	uint8_t;
typedef short			int16_t;
typedef unsigned short	uint16_t;
typedef int				int32_t;
typedef unsigned int	uint32_t;

#if defined(OS_WINDOWS)
	typedef __int64				int64_t;
	typedef unsigned __int64	uint64_t;
#else
	typedef long long			int64_t;
	typedef unsigned long long	uint64_t;
#endif

#endif /* !_ctypedef_h_ */
