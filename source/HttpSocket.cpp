#include <assert.h>
#include <stdlib.h>
#include "cstringext.h"
#include "HttpSocket.h"
#include "StrConvert.h"
#include "cookie.h"
#include "error.h"
#include "url.h"

HttpSocket::HttpSocket()
{
	m_ptr.reserve(4*1024);
	m_connFailed = 0;
	m_autoconnect = true;
	m_socket = socket_invalid;
	m_connTimeout = 5000;
	m_recvTimeout = 5000;
	m_http = http_parser_create(HTTP_PARSER_CLIENT);
	m_response.m_http = m_http;
	InitHttpHeader();
}

HttpSocket::HttpSocket(const char* ip, int port, int connTimeout/* =3000 */, int recvTimeout/* =5000 */)
{
	m_ptr.reserve(4*1024);
	m_connFailed = 0;
	m_autoconnect = true;
	m_socket = socket_invalid;
	m_connTimeout = connTimeout;
	m_recvTimeout = recvTimeout;
	m_http = http_parser_create(HTTP_PARSER_CLIENT);
	m_response.m_http = m_http;
	InitHttpHeader();

	Connect(ip, port);
}

HttpSocket::~HttpSocket()
{
	if(socket_invalid != m_socket)
	{
		socket_close(m_socket);
		m_socket = socket_invalid;
	}

	if(m_http)
		http_parser_destroy(m_http);
}

//////////////////////////////////////////////////////////////////////////
///
/// connect/disconnect
///
//////////////////////////////////////////////////////////////////////////
int HttpSocket::Connect(const char* ip, int port)
{
	assert(ip);
	//if(!m_ip.empty())
	//	return -1; // already initialize

	m_ip = ip;
	m_port = port;
	return Connect();
}

static int socket_connect_ipv4_by_time(IN socket_t sock, IN const char* ip_or_dns, IN unsigned short port, int timeout)
{
	int r;
	r = socket_setnonblock(sock, 1);
	r = socket_connect_ipv4(sock, ip_or_dns, (unsigned short)port);
	assert(r <= 0);
#if defined(OS_WINDOWS)
	if(0!=r && WSAEWOULDBLOCK==WSAGetLastError())
#else
	if(0!=r && EINPROGRESS==errno)
#endif
	{
		// check timeout
		r = socket_select_write(sock, timeout);
		if(1 == r)
		{
#if defined(OS_LINUX)
			int errcode = 0;
			int errlen = sizeof(errcode);
			r = getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&errcode, (socklen_t*)&errlen);
			if(0 == r)
				r = -errcode;
#endif
		}
		else
		{
			r = -1;
		}
	}
	socket_setnonblock(sock, 0);
	return r;
}

int HttpSocket::Connect()
{
	if(m_ip.empty())
		return ERROR_PARAM;

	Disconnect();

	assert(socket_invalid==m_socket);
	m_socket = socket_tcp();
	if(socket_invalid == m_socket)
		return ERROR_CONNECT;

	int r = socket_connect_ipv4_by_time(m_socket, m_ip.c_str(), (unsigned short)m_port, m_connTimeout);
	if(r < 0)
	{
		Disconnect();
		++m_connFailed;
		return ERROR_CONNECT;
	}
	m_connFailed = 0;
	return 0;
}

int HttpSocket::Disconnect()
{
	m_connFailed = 0;
	if(socket_invalid != m_socket)
	{
		int r = socket_close(m_socket);
		if(r < 0)
		{
			//return -socket_geterror();
		}
		m_socket = socket_invalid;
	}
	return 0;
}

bool HttpSocket::IsConnected() const
{
	if(socket_invalid == m_socket)
		return false; // linux: FD_SET error

	// MSDN: readfds: Connection has been closed/reset/terminated.
	// return 1 if connection has been closed, 0 if connection ok
	int r = socket_readable(m_socket);
	return 0==r;
}

int HttpSocket::CheckConnection()
{
	if(IsConnected())
		return 0;

	if(!m_autoconnect && m_connFailed>0)
		return ERROR_NOT_CONNECT; // connection is disconnect

	// auto connect
	return Connect();
}

int HttpSocket::SetSocket(socket_t sock)
{
	if(sock	== m_socket)
		return 0;

	Disconnect();
	m_socket = sock;
	return 0;
}

socket_t HttpSocket::GetSocket() const
{
	return m_socket;
}

socket_t HttpSocket::DetachSocket()
{
	socket_t s = m_socket;
	m_socket = socket_invalid;
	return s;
}

//////////////////////////////////////////////////////////////////////////
///
/// get/post
///
//////////////////////////////////////////////////////////////////////////
int HttpSocket::_Get(const char* uri)
{
	int r = CheckConnection();
	if(r < 0)
		return r;

	int n = MakeHttpHeader(m_ptr, "GET", uri, 0);

	// post
	r = socket_send(m_socket, m_ptr, n, 0);
	if(r != n)
	{
		Disconnect();
		return ERROR_SEND;
	}

	assert(r == n);
	return 0;
}

int HttpSocket::_Post(const char* uri, const void* content, size_t len)
{
	int r = CheckConnection();
	if(r < 0)
		return r;

	size_t n = MakeHttpHeader(m_ptr, "POST", uri, len);

	// fill
	socket_bufvec_t vec[2];
	socket_setbufvec(vec, 0, m_ptr, n);	
	socket_setbufvec(vec, 1, (void*)content, len);
	
	// post
	r = socket_send_v(m_socket, vec, sizeof(vec)/sizeof(vec[0]), 0);
	if(r != (int)(n+len))
	{
		Disconnect();
		return ERROR_SEND;
	}

	assert(r == (int)(n+len));
	return 0;
}

//////////////////////////////////////////////////////////////////////////
///
/// Http Header
///
//////////////////////////////////////////////////////////////////////////
void HttpSocket::InitHttpHeader()
{
	assert(0 == m_headers.size());
	KeepAlive(1*3600); // 1-hour(s)
	SetHeader("Accept", "text/req,*/*;q=0.1");
	SetHeader("User-Agent", "HttpSocket v2.0");
}

HttpSocket::THttpHeaders::iterator HttpSocket::FindHeader(const char* name)
{
	for(THttpHeaders::iterator it=m_headers.begin(); it!=m_headers.end(); ++it)
	{
		const std::string& header = it->first;
		if(0 == stricmp(header.c_str(), name))
			return it;
	}
	return m_headers.end();
}

HttpSocket::THttpHeaders::const_iterator HttpSocket::FindHeader(const char* name) const
{
	for(THttpHeaders::const_iterator it=m_headers.begin(); it!=m_headers.end(); ++it)
	{
		const std::string& header = it->first;
		if(0 == stricmp(header.c_str(), name))
			return it;
	}
	return m_headers.end();
}

void HttpSocket::SetHeader(const char* name, int value)
{
	SetHeader(name, ToAString(value));
}

void HttpSocket::SetHeader(const char* name, const char* value)
{
	if(0 == stricmp("HOST", name))
	{
		// ignore
		return;
	}
	else if(0 == stricmp("Content-Length", name))
	{
		// ignore
		return;
	}

	THttpHeaders::iterator it = FindHeader(name);
	if(it != m_headers.end())
	{
		it->second = NULL==value?"":value;
	}
	else
	{
		m_headers.push_back(std::make_pair(name, NULL==value?"":value));
	}
}

void HttpSocket::KeepAlive(int ms)
{
	SetHeader("Connection", 0==ms?"close":"keep-alive");
	SetHeader("Keep-Alive", ms);
}

void HttpSocket::SetCookie(const char* cookies)
{
	// Cookie: name=value; name2=value2
	SetHeader("Cookie", cookies);
}

bool HttpSocket::GetCookie(std::string& cookies)
{
	THttpHeaders::iterator it = FindHeader("Cookie");
	if(it == m_headers.end())
		return false;

	cookies = it->second;
	return true;
}

void HttpSocket::SetContentLength(int length)
{
	SetHeader("Content-Length", length);
}

inline bool SetVersion(mmptr& reply, const char* method, const char* uri)
{
	reply += method;
	reply += ' ';
	reply += uri;
	reply += " HTTP/1.1\r\n";
	return true;
}

inline bool SetHost(mmptr& reply, const char* host, int port)
{
	reply += "HOST";
	reply += ": ";
	reply += host;
	if(80 != port)
	{
		reply += ':';
		reply += ToAString(port);
	}
	reply += "\r\n";
	return true;
}

inline bool AddHeader(mmptr& reply, const char* name, int value)
{
	reply += name;
	reply += ": ";
	reply += ToAString(value);
	reply += "\r\n";
	return true;
}

inline bool AddHeader(mmptr& reply, const char* name, const char* value)
{
	reply += name;
	reply += ": ";
	reply += value;
	reply += "\r\n";
	return true;
}

int HttpSocket::MakeHttpHeader(mmptr& reply, const char* method, const char* uri, int contentLength) const
{
	// POST http://ip:port/xxx HTTP/1.1\r\n
	// HOST: ip:port\r\n
	// Content-Length: length\r\n
	// Content-Type: application/x-www-form-urlencoded\r\n

	reply.clear();

	// http version
	SetVersion(reply, method, (uri && *uri) ? uri : "/"); // default to /
	
	// host
	void* url = url_parse(uri);
	if(url)
	{
		int port = url_getport(url);
		const char* host = url_gethost(url);
		if(host)
			SetHost(reply, host, 0==port?80:port);
		else
			SetHost(reply, m_ip.c_str(), m_port);
		url_free(url);
	}
	else
	{
		SetHost(reply, m_ip.c_str(), m_port);
	}

	// http headers
	for(THttpHeaders::const_iterator it=m_headers.begin(); it!=m_headers.end(); ++it)
	{
		const std::string& name = it->first;
		const std::string& value = it->second;

		assert(0 != stricmp("HOST", name.c_str())); // Host
		assert(0 != stricmp("Content-Length", name.c_str())); // ignore Content-Length
		AddHeader(reply, name.c_str(), value.c_str());
	}

	// Content-Type
	if(m_headers.end()==FindHeader("Content-Type") && contentLength>0)
		AddHeader(reply, "Content-Type", "application/x-www-form-urlencoded");

	// Content-Length
	AddHeader(reply, "Content-Length", contentLength);

	// http header ender: "\r\n\r\n"
	reply += "\r\n";
	return reply.size();
}

//void HttpSocket::GetHeaders()
//{
//	if(!m_headersUpdate)
//		return;
//
//	m_reqHeader.clear();
//	for(THttpHeaders::const_iterator it=m_headers.begin(); it!=m_headers.end(); ++it)
//	{
//		const std::string& name = it->first;
//		const std::string& value = it->second;
//
//		assert(0 != stricmp("HOST", name.c_str())); // Host = m_host
//		assert(0 != stricmp("Content-Length", name.c_str())); // ignore Content-Length
//
//		m_reqHeader += name;
//		m_reqHeader += ":";
//		m_reqHeader += value;
//		m_reqHeader += "\r\n";
//	}
//	m_reqHeader += "\r\n";
//	m_headersUpdate = true;
//}

int HttpSocket::Get(const char* uri, mmptr& reply)
{
	int r = _Get(uri);
	if(r < 0)
		return r;

	return GetReply(reply);
}

int HttpSocket::Post(const char* uri, const void* content, size_t len, mmptr& reply)
{
	int r = _Post(uri, content, len);
	if(r < 0)
		return r;

	return GetReply(reply);
}

int HttpSocket::GetReply(mmptr& reply)
{
	int status = 0;
	http_parser_clear(m_http);

	do
	{
		void* p = m_ptr.get();
		int r = socket_recv_by_time(m_socket, p, m_ptr.capacity(), 0, m_recvTimeout);
		if(r < 0)
		{
			Disconnect();
			return ERROR_RECV;
		}

		int bytes = r;
		status = http_parser_input(m_http, p, &bytes);
		if(status < 0)
		{
			Disconnect();
			return status; // parse error
		}

		assert(0 == bytes);
		if(0 == r && 1 == status)
		{
			Disconnect();
			return ERROR_RECV; // peer close socket, don't receive all data
		}

	} while(1 == status);

	// set reply
	reply.set(http_get_content(m_http), http_get_content_length(m_http));

	// set cookie
	const char* cookie = http_get_cookie(m_http);
	if(cookie)
	{
		std::string cookien, cookiev;
		Cookie c(cookie);
		if(c.GetNameValue(cookien, cookiev))
			SetCookie((cookien+'='+cookiev).c_str());
	}

	if(1 == http_get_connection(m_http))
		Disconnect();

	// url redirect(3xx: move)
	//int code = http_get_status_code(m_http);
	return 0;
}
