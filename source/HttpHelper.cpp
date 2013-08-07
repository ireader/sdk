#include <stdarg.h>
#include "cstringext.h"
#include "HttpHelper.h"

HttpHelper::THttpStatus HttpHelper::sm_status;
void HttpHelper::InitStatus()
{
	if(sm_status.size() > 0)
		return;

	// Informational
	sm_status.insert(std::make_pair(100, "Continue"));
	sm_status.insert(std::make_pair(101, "Switching Protocols"));

	// Successful
	sm_status.insert(std::make_pair(200, "OK"));
	sm_status.insert(std::make_pair(201, "Created"));
	sm_status.insert(std::make_pair(202, "Accepted"));
	sm_status.insert(std::make_pair(203, "Non-Authoritative Information"));
	sm_status.insert(std::make_pair(204, "No Content"));
	sm_status.insert(std::make_pair(205, "Reset Content"));
	sm_status.insert(std::make_pair(206, "Partial Content"));

	// Redirection
	sm_status.insert(std::make_pair(300, "Multiple Choices"));
	sm_status.insert(std::make_pair(301, "Move Permanently"));
	sm_status.insert(std::make_pair(302, "Found"));
	sm_status.insert(std::make_pair(303, "See Other"));
	sm_status.insert(std::make_pair(304, "Not Modified"));
	sm_status.insert(std::make_pair(305, "Use Proxy"));
	sm_status.insert(std::make_pair(306, "Unused"));
	sm_status.insert(std::make_pair(307, "Temporary Redirect"));

	// Client Error
	sm_status.insert(std::make_pair(400, "Bad Request"));
	sm_status.insert(std::make_pair(401, "Unauthorized"));
	sm_status.insert(std::make_pair(402, "Payment Required"));
	sm_status.insert(std::make_pair(403, "Forbidden"));
	sm_status.insert(std::make_pair(404, "Not Found"));
	sm_status.insert(std::make_pair(405, "Method Not Allowed"));
	sm_status.insert(std::make_pair(406, "Not Acceptable"));
	sm_status.insert(std::make_pair(407, "Proxy Authentication Required"));
	sm_status.insert(std::make_pair(408, "Request Timeout"));
	sm_status.insert(std::make_pair(409, "Conflict"));
	sm_status.insert(std::make_pair(410, "Gone"));
	sm_status.insert(std::make_pair(411, "Length Required"));
	sm_status.insert(std::make_pair(412, "Precondition Failed"));
	sm_status.insert(std::make_pair(413, "Request Entity Too Large"));
	sm_status.insert(std::make_pair(414, "Request-URI Too Long"));
	sm_status.insert(std::make_pair(415, "Unsupported Media Type"));
	sm_status.insert(std::make_pair(416, "Request Range Not Satisfiable"));
	sm_status.insert(std::make_pair(417, "Expectation Failed"));

	// Server Error
	sm_status.insert(std::make_pair(500, "Internal Server Error"));
	sm_status.insert(std::make_pair(501, "Not Implemented"));
	sm_status.insert(std::make_pair(502, "Bad Gateway"));
	sm_status.insert(std::make_pair(503, "Service Unavailable"));
	sm_status.insert(std::make_pair(504, "Gateway Timeout"));
	sm_status.insert(std::make_pair(505, "HTTP Version Not Supported"));
}

HttpHelper::HttpHelper()
{
	InitStatus();
	SetStatus(200, NULL);
}

HttpHelper::HttpHelper(int status, const char* reason)
{
	InitStatus();
	SetStatus(status, reason);
}

void HttpHelper::SetStatus(int status, const char* reason)
{
	m_status = status;
	if(strempty(reason))
	{
		THttpStatus::const_iterator it = sm_status.find(status);
		if(it != sm_status.end())
		{
			m_reason = it->second;
			return;
		}
	}
	m_reason = reason;
}

void HttpHelper::AddHeader(const char* header, const char* contentFormat, ...)
{
	char content[256] = {0};

	va_list arg;
	va_start(arg, contentFormat);
	vsnprintf(content, sizeof(content)/sizeof(content[0])-1, contentFormat, arg);
	va_end(arg);

	// encoding
	//if(0 == stricmp("Content-Encoding", header))
	//{
	//}
	m_headers.push_back(std::make_pair(std::string(header), std::string(content)));
}

void HttpHelper::SetBody(const char* contentFormat, ...)
{
	char content[512] = {0};

	va_list arg;
	va_start(arg, contentFormat);
	vsnprintf(content, sizeof(content)/sizeof(content[0])-1, contentFormat, arg);
	va_end(arg);

	// encoding ?
	m_message = content;
}

std::string HttpHelper::Get() const
{
	// Response Status-Line:
	// HTTP-Version SP Status-Code SP Reason-Phrase CRLF
	char buffer[256] = {0};
	//_snprintf(buffer, sizeof(buffer)/sizeof(buffer[0])-1, "HTTP/1.1 %d %s\r\n", m_status, m_reason.c_str());
	_snprintf(buffer, sizeof(buffer)/sizeof(buffer[0])-1, "Status: %d %s\r\n", m_status, m_reason.c_str());
	std::string s = buffer;

	// generic-header + response-header + entity-header
	for(THttpHeaders::size_type i=0; i<m_headers.size(); ++i)
	{
		const THttpHeader& header = m_headers[i];
		s += header.first;
		s += ":";
		s += header.second;
		s += "\r\n";
	}

	// Content-Length
	_snprintf(buffer, sizeof(buffer)/sizeof(buffer[0])-1, "Content-Length:%d\r\n", m_message.size());
	s += buffer;

	// message-body
	s += "\r\n";
	s += m_message;
	return s;
}
