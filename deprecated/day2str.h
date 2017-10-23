#ifndef _day2str_h_
#define _day2str_h_

#include <assert.h>

static inline const char* ToWeek(int weekday)
{
	static const char c_week[][10] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
	assert(sizeof(c_week) / sizeof(c_week[0]) == 7);
	return c_week[weekday % 7];
}

static inline const char* ToWeek2(int weekday)
{
	static const char c_week[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	assert(sizeof(c_week) / sizeof(c_week[0]) == 7);
	return c_week[weekday % 7];
}

static inline const char* ToMonth(int month)
{
	static const char c_month[][10] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
	assert(sizeof(c_month) / sizeof(c_month[0]) == 12);
	return c_month[month % 12];
}

static inline const char* ToMonth2(int month)
{
	static const char c_month[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	assert(sizeof(c_month) / sizeof(c_month[0]) == 12);
	return c_month[month % 12];
}

#endif /* !_day2str_h_ */
