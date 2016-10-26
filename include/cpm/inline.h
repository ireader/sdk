#ifndef _inline_h_
#define _inline_h_

// inline keyword
#if !defined(__cplusplus)
	#if _MSC_VER && _MSC_VER < 1900 // VS2015
		#define inline __inline
	#elif __GNUC__ && !__GNUC_STDC_INLINE__ && !__GNUC_GNU_INLINE__
		#define inline static __inline__ //__attribute__((unused))
		//#define inline static __inline__ __attribute__((always_inline))
	#else
		//#define inline static inline
	#endif
#endif

#endif /* !_inline_h_ */
