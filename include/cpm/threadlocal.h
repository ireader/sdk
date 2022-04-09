#ifndef _threadlocal_h_
#define _threadlocal_h_

#if defined(OS_WINDOWS)
	#define THREAD_LOCAL static __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
	#define THREAD_LOCAL static __thread
#else
	#define THREAD_LOCAL 
#endif

#endif /* !_threadlocal_h_ */
