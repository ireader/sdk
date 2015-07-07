#ifndef _ctypedef_h_
#define _ctypedef_h_

#define bool_true	1
#define bool_false	0

#if defined(OS_WINDOWS)
	typedef unsigned char		bool_t;
	typedef char				int8_t;
	typedef short				int16_t;
	typedef int					int32_t;

	typedef unsigned char		uint8_t;
	typedef unsigned short		uint16_t;
	typedef unsigned int		uint32_t;

	#ifndef OS_INT64_TYPE
	typedef __int64				int64_t;
	typedef unsigned __int64	uint64_t;
	#define PRId64				"I64d"
	#define PRIu64				"I64u"
	#define PRIsize_t			"Iu"	// MSDN: Size Specification
	#define PRIptrdiff_t		"Ix"	// MSDN: Size Specification
	#define OS_INT64_TYPE
	#endif /* OS_INT64_TYPE */
#else
	/* The ISO C99 standard specifies that these macros must only be defined if explicitly requested.  */
	#define __STDC_FORMAT_MACROS
	#include <inttypes.h>
	#include <stdint.h>

	#define PRIsize_t				"zu"	// C99
	#define PRIptrdiff_t			"tx"	// C99
	#ifndef OS_INT64_TYPE
	typedef long long int			int64_t;
	typedef unsigned long long int	uint64_t;
	#define OS_INT64_TYPE
	#endif /* OS_INT64_TYPE */
#endif

#endif /* !_ctypedef_h_ */
