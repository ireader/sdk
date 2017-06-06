#ifndef _deprecated_h_
#define _deprecated_h_

#if defined(_MSC_VER)
	#define attribute_deprecated __declspec(deprecated)
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
	#define attribute_deprecated __attribute__((deprecated))
#else
	#define attribute_deprecated
#endif

#endif /* !_deprecated_h_ */
