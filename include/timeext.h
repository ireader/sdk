#ifndef _timeext_h_
#define _timeext_h_

inline const char* ToWeek(int weekday)
{
	const char c_week[][10] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

	if(weekday>=0 && weekday<sizeof(c_week)/sizeof(c_week[0]))
		return c_week[weekday];
	return NULL;
}

inline const char* ToWeek2(int weekday)
{
	const char c_week[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

	if(weekday>=0 && weekday<sizeof(c_week)/sizeof(c_week[0]))
		return c_week[weekday];
	return NULL;
}

inline const char* ToMonth(int month)
{
	const char c_month[][10] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

	if(month>=0 && month<sizeof(c_month)/sizeof(c_month[0]))
		return c_month[month];
	return NULL;
}

inline const char* ToMonth2(int month)
{
	const char c_month[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	if(month>=0 && month<sizeof(c_month)/sizeof(c_month[0]))
		return c_month[month];
	return NULL;
}

#endif /* !_timeext_h_ */
