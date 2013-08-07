#ifndef _HttpHelper_h_
#define _HttpHelper_h_

#include <map>
#include <vector>
#include <string>

class HttpHelper
{
public:
	HttpHelper();
	HttpHelper(int status, const char* reason=NULL);

public:
	void SetStatus(int status, const char* reason=NULL);
	void AddHeader(const char* header, const char* contentFormat, ...);
	void SetBody(const char* contentFormat, ...);

	std::string Get() const;

private:
	typedef std::pair<std::string, std::string> THttpHeader;
	typedef std::vector<THttpHeader> THttpHeaders;

	THttpHeaders m_headers;

	int m_status;
	std::string m_reason;
	std::string m_message;

private:
	typedef std::map<int, std::string> THttpStatus;
	static THttpStatus sm_status;
	static void InitStatus();
};

#endif /* !_HttpHelper_h_ */
