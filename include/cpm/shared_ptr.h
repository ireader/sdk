#ifndef _shared_ptr_h_
#define _shared_ptr_h_

#include <memory>

// https://en.wikipedia.org/wiki/Visual_C%2B%2B
#if defined(OS_WINDOWS)
#define VC5		1100	// MS VC++ 5.0
#define VC6		1200	// MS VC++ 6.0
#define VS2002	1300	// MS VC++ 7.0
#define VS2003	1310	// MS VC++ 7.1
#define VS2005	1400	// MS VC++ 8.0
#define VS2008	1500	// MS VC++ 9.0
#define VS2010	1600	// MS VC++ 10.0
#define VS2012	1700	// MS VC++ 11.0
#define VS2013	1800	// MS VC++ 12.0
#define VS2015	1900	// MS VC++ 13.0
#define VS14	2000	// MS VC++ 14.0

#if _MSC_VER <= VS2008
#define shared_ptr tr1::shared_ptr
#endif

#elif __GNUC__ && __cplusplus < 201103L
// since GCC 4.3
// http://stackoverflow.com/questions/8171444/c-stdshared-ptr-usage-and-information
#include <tr1/memory>
#define shared_ptr tr1::shared_ptr

namespace std
{
	template<typename T>
	inline std::shared_ptr<T> make_shared()
	{
		return std::shared_ptr<T>(new T());
	}

	template<typename T, typename Arg1>
	inline std::shared_ptr<T> make_shared(Arg1 arg1)
	{
		return std::shared_ptr<T>(new T(arg1));
	}

	template<typename T, typename Arg1, typename Arg2>
	inline std::shared_ptr<T> make_shared(Arg1 arg1, Arg2 arg2)
	{
		return std::shared_ptr<T>(new T(arg1, arg2));
	}

	template<typename T, typename Arg1, typename Arg2, typename Arg3>
	inline std::shared_ptr<T> make_shared(Arg1 arg1, Arg2 arg2, Arg3 arg3)
	{
		return std::shared_ptr<T>(new T(arg1, arg2, arg3));
	}

	template<typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
	inline std::shared_ptr<T> make_shared(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
	{
		return std::shared_ptr<T>(new T(arg1, arg2, arg3, arg4));
	}

	template<typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
	inline std::shared_ptr<T> make_shared(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
	{
		return std::shared_ptr<T>(new T(arg1, arg2, arg3, arg4, arg5));
	}

	template<typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
	inline std::shared_ptr<T> make_shared(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
	{
		return std::shared_ptr<T>(new T(arg1, arg2, arg3, arg4, arg5, arg6));
	}
}

#endif // OS_WINDOWS

#endif /* !_shared_ptr_h_ */
