#ifndef _unuse_h_
#define _unuse_h_

// UNUSED
#if defined(__cplusplus)
	#define UNUSED(x)
#else
	#if defined(_MSC_VER)
		#define UNUSED(x) x
	#elif defined(__GNUC__)
		#define UNUSED(x) x __attribute__((unused))
	#else
		#define UNUSED(x) x
	#endif
#endif

#endif /* !_unuse_h_ */
