#ifndef _cookie_h_
#define _cookie_h_

#include <string>

class Cookie
{
public:
	Cookie();
	Cookie(const char* cookie);
	Cookie(const char* name, const char* value, const char* expires);
	Cookie(const char* name, const char* value, const char* expires, const char* path);
	~Cookie();

public:
	std::string GetCookie() const;

public:
	const char* GetExpire() const		{ return m_expire.c_str(); }
	void SetExpire(const char* expire)	{ m_expire = expire; }

	const char* GetPath() const			{ return m_path.c_str(); }
	void SetPath(const char* path)		{ m_path = path; }

	const char* GetDomain() const		{ return m_domain.c_str(); }
	void SetDomain(const char* domain)	{ m_domain = domain; }

	const char* GetVersion() const		{ return m_version.c_str(); }
	void SetVersion(const char* version){ m_version = version; }

	const char* GetMaxAge() const		{ return m_maxage.c_str(); }
	void SetMaxAge(const char* maxage)	{ m_maxage = maxage; }

	bool GetSecure() const				{ return m_secure; }
	void SetSecure(bool secure)			{ m_secure = secure; }

	bool GetHttpOnly() const			{ return m_httponly; }
	void SetHttpOnly(bool httpOnly)		{ m_httponly = httpOnly; }

	bool GetNameValue(std::string& name, std::string& value)
	{
		if(m_name.empty())
			return false;
		name = m_name;
		value = m_value;
		return true;
	}
	void SetNameValue(const char* name, const char* value)
	{
		if(0==name || 0==*name || 0==value || 0==*value) return;
		m_name = name;
		m_value = value;
	}

public:
	static std::string CreateExpire(int hours);
	static bool GetCookieValue(const char* cookies, const char* name, std::string& value);

private:
	std::string m_name;
	std::string m_value;
	std::string m_expire;
	std::string m_domain;
	std::string m_path;
	std::string m_version;
	std::string m_maxage;

	bool m_secure;
	bool m_httponly;
};

#endif /* !_cookie_h_ */
