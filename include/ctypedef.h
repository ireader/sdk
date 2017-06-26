#ifndef _ctypedef_h_
#define _ctypedef_h_

#include <stddef.h> // size_t/ptrdiff_t/nullptr_t/max_align_t

#if defined(OS_WINDOWS)
#if _MSC_VER >= 1900
	#include <inttypes.h>
	#include <stdint.h>
#else
	typedef char				int8_t;
	typedef short				int16_t;
	typedef int					int32_t;
	typedef __int64				int64_t;

	typedef unsigned char		uint8_t;
	typedef unsigned short		uint16_t;
	typedef unsigned int		uint32_t;
	typedef unsigned __int64	uint64_t;

	#define PRId64				"I64d"
	#define PRIu64				"I64u"

	#define INT64_MIN			(-9223372036854775807i64 - 1)
#endif
	#define PRIsize_t			"Iu"	// MSDN: Size Specification
	#define PRIptrdiff_t		"Ix"	// MSDN: Size Specification

#else
	/* The ISO C99 standard specifies that these macros must only be defined if explicitly requested.  */
	#define __STDC_FORMAT_MACROS
	#define __STDC_LIMIT_MACROS
	#include <inttypes.h>
	#include <stdint.h>

	#define PRIsize_t			"zu"	// C99
	#define PRIptrdiff_t		"tx"	// C99
#endif

#define bool_true				1
#define bool_false				0
typedef unsigned char			bool_t;

#endif /* !_ctypedef_h_ */
