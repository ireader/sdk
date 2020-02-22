#ifndef _cpm_param_h_
#define _cpm_param_h_

// MIN
// MAX
// BOUND(10, 1, 100) ==> 10
// FLOOR(34, 10) ==> 3
// CEIL(34, 10) ==> 4

#if defined(__cplusplus)
#include <algorithm>

#ifndef MIN
#undef min // msvc stdlib.h
#define MIN(x, y) std::min(x, y)
#endif

#ifndef MAX
#undef max // msvc stdlib.h
#define MAX(x, y) std::max(x, y)
#endif

#ifndef FLOOR
#define FLOOR(num, deno) __FLOOR(num, deno)
template <class T>
inline T __FLOOR(const T& num, const T& demo)
{
	return num / demo;
}
#endif

#ifndef CEIL
#define CEIL(num, deno) __CEIL(num, deno)
template <class T>
inline T __CEIL(const T& num, const T& demo)
{
	return (num + demo - 1) / demo;
}
#endif

#else // __cplusplus

#if __GNUC__
#ifndef MIN
#define MIN(x, y) ( __extension__ ({ \
			typeof(x) _min1 = (x);          \
			typeof(y) _min2 = (y);          \
			(void) (&_min1 == &_min2);      \
			_min1 < _min2 ? _min1 : _min2; }))
#endif

#ifndef MAX
#define MAX(x, y) ( __extension__ ({ \
			typeof(x) _max1 = (x);          \
			typeof(y) _max2 = (y);          \
			(void) (&_max1 == &_max2);      \
			_max1 > _max2 ? _max1 : _max2; }))
#endif

#ifndef FLOOR
#define FLOOR(num, deno) ( __extension__ ({ \
			typeof(num) _num1 = (num);		\
			typeof(deno) _deno1 = (deno);	\
			(void) (&_num1 == &_deno1);		\
			_num1 / _deno1; }))
#endif

#ifndef CEIL
#define CEIL(num, deno) ( __extension__ ({ \
			typeof(num) _num1 = (num);		\
			typeof(deno) _deno1 = (deno);	\
			(void) (&_num1 == &_deno1);		\
			(_num1 + _deno1 - 1) / _deno1; }))
#endif

#else // __GNUC__

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

// FLOOR(num/demo): FLOOR(34/10) = 3 
#ifndef FLOOR
#define FLOOR(num, deno) ( (num)/ (deno) )
#endif

// CEIL(num/demo): CEIL(34/10) = CEIL((34 + 10 - 1) / 10) = 4
#ifndef CEIL
#define CEIL(num, deno) ( ((num) + (deno) - 1) / (deno) )
#endif

#endif // __GNUC__
#endif // __cplusplus


// BOUND(value, 2, 20) => [2 ~ 20]
#ifndef BOUND
#define BOUND(v, min, max) MIN(MAX(v, min), max)
#endif

#endif /* !_cpm_param_h_ */
