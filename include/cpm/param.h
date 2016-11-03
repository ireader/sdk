#ifndef _cpm_param_h_
#define _cpm_param_h_

#ifndef __cplusplus
#ifndef min
#if __GNUC__
#define min(x, y) ( __extension__ ({ \
			typeof(x) _min1 = (x);          \
			typeof(y) _min2 = (y);          \
			(void) (&_min1 == &_min2);      \
			_min1 < _min2 ? _min1 : _min2; }))
#else
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#endif // min

#ifndef max
#if __GNUC__
#define max(x, y) ( __extension__ ({ \
			typeof(x) _max1 = (x);          \
			typeof(y) _max2 = (y);          \
			(void) (&_max1 == &_max2);      \
			_max1 > _max2 ? _max1 : _max2; }))
#else
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif
#endif // max
#endif // __cplusplus

#endif /* !_cpm_param_h_ */
