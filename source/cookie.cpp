#include "cookie.h"
#include <time.h>
#include <string>
#include <assert.h>
#include "cstringext.h"
#include "cppstringext.h"

Cookie::Cookie()
{
	m_path = "/";
	m_secure = false;
	m_httponly = false;
}

Cookie::Cookie(const char* name, const char* value, const char* expires)
{
	m_path = "/";
	m_name = name;
	m_value = value;
	m_secure = false;
	m_httponly = false;
}

Cookie::Cookie(const char* name, const char* value, const char* expires, const char* path)
{
	if(NULL==name || 0==*name || NULL==value || 0==*value || NULL==expires || NULL==path)
	{
		assert(false);
	}

	m_path = path;
	m_name = name;
	m_value = value;
	m_expire = expires;
	m_secure = false;
	m_httponly = false;
}

static bool ParseCookieItem(const char* p, std::string& name, std::string& value)
{
	if(NULL==p || 0==*p)
		return false;

	const char* pn1 = p + strspn(p, " \t");
	const char* pn2 = pn1+1;
	while(isalnum(*pn2))
		++pn2;

	const char* pv1 = pn2 + strspn(pn2, " \t");
	if('=' == *pv1)
	{
		pv1 += strspn(pv1+1, " \t");
		const char* pv2 = p+strlen(p)-1;
		while(isspace(*pv2) && pv2 > pv1)
			--pv2;

		if(pv2 < pv1)
			return false; // empty item

		name = std::string(pn1, pn2-pn1);
		value = std::string(pv1, pv2+1-pv1);
	}
	else
	{
		name = std::string(pn1, pn2-pn1);
	}
	return true;
}

Cookie::Cookie(const char* cookie)
{
	std::vector<std::string> vec;
	Split(cookie, ';', vec);

	for(size_t i=0; i<vec.size(); i++)
	{
		std::string name, value;
		const std::string& s = vec[i];
		if(!ParseCookieItem(s.c_str(), name, value))
			continue;

		if(0 == strcasecmp(name.c_str(), "expires"))
		{
			SetExpire(value.c_str());
		}
		else if(0 == strcasecmp(name.c_str(), "path"))
		{
			SetPath(value.c_str());
		}
		else if(0 == strcasecmp(name.c_str(), "domain"))
		{
			SetDomain(value.c_str());
		}
		else if(0 == strcasecmp(name.c_str(), "version"))
		{
			SetVersion(value.c_str());
		}
		else if(0 == strcasecmp(name.c_str(), "max-age"))
		{
			SetMaxAge(value.c_str());
		}
		else if(0 == strcasecmp(name.c_str(), "secure"))
		{
			SetSecure(true);
		}
		else if(0 == strcasecmp(name.c_str(), "httponly"))
		{
			SetHttpOnly(true);
		}
		else
		{
			SetNameValue(name.c_str(), value.c_str());
		}
	}
}

Cookie::~Cookie()
{
}

inline void AddCookieItem(std::string& cookie, const char* name, const std::string& value)
{
	if(value.empty())
		return;

	cookie += name;
	cookie += '=';
	cookie += value;
	cookie += "; ";
}

inline void AddCookieItem(std::string& cookie, const char* name, bool value)
{
	if(!value)
		return;

	cookie += name;
	cookie += "; ";
}

std::string Cookie::GetCookie() const
{
	assert(!m_name.empty());

	std::string cookie;
	AddCookieItem(cookie, m_name.c_str(), m_value);
	if(!m_expire.empty())
		AddCookieItem(cookie, "expires", m_expire);
	AddCookieItem(cookie, "path", m_path);
	AddCookieItem(cookie, "domain", m_domain);
	AddCookieItem(cookie, "version", m_version);
	AddCookieItem(cookie, "max-age", m_maxage);
	AddCookieItem(cookie, "secure", m_secure);
	AddCookieItem(cookie, "httponly", m_httponly);

	return cookie.substr(0, cookie.length()-2);
}

std::string Cookie::CreateExpire(int hours)
{
	if(0 == hours)
		return "";

	const char week[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	const char month[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	time_t t = time(NULL);
	t += hours * 3600; // current + expireDay
	tm* gmt = gmtime(&t);

	char buffer[64] = {0};
	snprintf(buffer, sizeof(buffer), "%s, %02d-%s-%02d %02d:%02d:%02d GMT",
		week[gmt->tm_wday],	// weekday
		gmt->tm_mday,		// day
		month[gmt->tm_mon],	// month
		gmt->tm_year+1900,	// year
		gmt->tm_hour,		// hour
		gmt->tm_min,		// minute
		gmt->tm_sec);		// second

	return std::string(buffer);
}

bool Cookie::GetCookieValue(const char* cookies, const char* name, std::string& value)
{
	std::vector<std::string> vec;
	Split(cookies, ';', vec);
	if(vec.size() < 1)
		return false;

	for(size_t i=0; i<vec.size(); i++)
	{
		const std::string& v = vec[i];
		Cookie cookie(v.c_str());
		std::string cookieName, cookieValue;
		if(!cookie.GetNameValue(cookieName, cookieValue))
			continue;

		if(0 == strcasecmp(name, cookieName.c_str()))
		{
			value = cookieValue;
			return true;
		}
	}
	return false;
}
