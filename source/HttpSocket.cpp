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
	m_ptr.reserve(512);
	m_connFailed = 0;
	m_autoconnect = true;
	m_socket = socket_invalid;
	m_connTimeout = 5000;
	m_recvTimeout = 5000;	
	InitHttpHeader();
}

HttpSocket::HttpSocket(const char* ip, int port, int connTimeout/* =3000 */, int recvTimeout/* =5000 */)
{
	m_ptr.reserve(512);
	m_connFailed = 0;
	m_autoconnect = true;
	m_socket = socket_invalid;
	m_connTimeout = connTimeout;
	m_recvTimeout = recvTimeout;
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
/// reply
///
//////////////////////////////////////////////////////////////////////////
static int SocketRecvLine(mmptr& reply, socket_t socket, int timeout)
{
	const size_t c_length = 20; // content minimum length
	size_t n = reply.size();
	char* p;

	do
	{
		if(reply.capacity()<n+c_length)
		{
			if(reply.reserve(n+c_length))
				return ERROR_MEMORY;
		}

		p = (char*)reply.get() + n;
		int r = socket_recv_by_time(socket, p, c_length-1, 0, timeout);
		if(r <= 0)
			return r;

		n += r;
		p[r] = 0;
	} while(!strchr(p, '\n') && n<20*1024*1024);

	assert(reply.capacity()>n);
	return n;
}

//static int SocketRecvAll(mmptr& reply, socket_t socket, size_t size, int timeout)
//{
//	if(reply.capacity()<reply.size()+size+1)
//	{
//		if(reply.reserve(reply.size()+size+1))
//			return ERROR_MEMORY;
//	}
//
//	char* p = (char*)reply.get()+reply.size();
//	int r = socket_recv_all_by_time(socket, p, size, 0, timeout);
//	if(r <= 0)
//		return r;
//
//	p[r] = 0;
//	return r;
//}

static bool ParseNameValue(char* s, int len, char*& name, char*& value)
{
	// parse response header name value
	// name: value
	char* p = strchr(s, ':');
	if(NULL == p)
	{
		assert(false);
		return false;
	}

	name = s;
	value = p+1; // skip ':'

	while(' ' == *name) ++name; // trim left space
	do{ --p; } while(p>name && ' '==*p); // skip ':' and trim right space
	*++p = 0;

	while(' ' == *value) ++value; // trim left space
	p = s+len;
	do{ --p; } while(p>value && (' '==*p||'\r'==*p)); // trim right space
	*++p = 0;

	return 0 != *name; // name can't be empty
}

static bool ParseHttpStatus(const char* s, HttpResponse& response)
{
	int status;
	char version[16];
	char majorv, minorv;
	if(3 != sscanf(s, "HTTP/%c.%c %d", &majorv, &minorv, &status))
		return false;

	memset(version, 0, sizeof(version));
	snprintf(version, sizeof(version)-1, "%c.%c", majorv, minorv);
	response.SetStatusCode(status);
	response.SetHttpVersion(version);
	return true;
}

static bool ParseHttpHeader(char* reply, size_t len, HttpResponse& response)
{
	char* name;
	char* value;
	assert(len > 2 && reply[len]=='\n' && reply[len-1]=='\r');

	// *--p = 0; // \r -> 0
	if(!ParseNameValue(reply, len, name, value))
		return false;

	if(0 == stricmp("Content-Length", name))
	{
		// < 50 M
		int len = atoi(value);
		assert(len >= 0 && len < 50*1024*1024);
		response.SetContentLength(len);
	}
	//else if(0 == stricmp("Set-Cookie", name))
	//{
	//	std::string cookien, cookiev;
	//	Cookie cookie(value);
	//	if(cookie.GetNameValue(cookien, cookiev))
	//		SetCookie((cookien+'='+cookiev).c_str());
	//}
	else if(0 == stricmp("Connection", name))
	{
		response.SetConnectionClose(0 == stricmp("close", value));
	}
		
	response.SetHeader(name, value);
	return true;
}

int HttpSocket::RecvHeader(HttpResponse& response)
{
	m_ptr.clear();

	int r = 0;
	int lineNo = 0;	// line number
	do
	{
		// recv header
		r = SocketRecvLine(m_ptr, m_socket, m_recvTimeout);
		if(r <= 0)
			return ERROR_RECV_TIMEOUT;

		// parse header
		char* line = m_ptr;
		char* p = strchr(line, '\n');
		assert(p);
		while(p)
		{
			assert(p>line && *(p-1)=='\r');
			if(0 == lineNo)
			{
				// response status
				if(!ParseHttpStatus(line, response))
					return ERROR_REPLY;
			}
			else
			{
				if(p-1 == line)
				{
					// recv "\r\n\r\n"
					// copy content data(don't copy with '\0')
					m_ptr.set(p+1, r-(p+1-(char*)m_ptr));
					return 0;
				}

				ParseHttpHeader(line, p-line, response);
			}

			++lineNo;
			line = p+1;
			p = strchr(line, '\n');
		}

		// copy remain data
		m_ptr.set(line, r-(line-(char*)m_ptr));
	} while(r > 0);

	return 0;
}

int HttpSocket::RecvContent(size_t contentLength, mmptr& reply)
{
	assert(contentLength>0 && contentLength<1024*1024*1024);
	if(reply.capacity() < contentLength)
	{
		if(0 != reply.reserve(contentLength))
			return ERROR_MEMORY;
	}

	// copy receive data
	reply.set(m_ptr.get(), m_ptr.size());

	// receive content
	assert(contentLength>=m_ptr.size());
	int n = (int)(contentLength-m_ptr.size());
	if(n > 0)
	{
		void* p = (char*)reply.get() + reply.size();
		int r = socket_recv_all_by_time(m_socket, p, n, 0, m_recvTimeout);
		if(0 == r)
			return ERROR_RECV_TIMEOUT; // timeout
		else if(r < 0)
			return ERROR_RECV;
		assert(r == n);
	}

	reply.set(reply.get(), contentLength); // set reply size
	return 0;
}

static int RecvChunkedHelper(const char* src, int srcLen, char* dst, int chunkLen, socket_t socket, int timeout)
{
	if(chunkLen > 0)
	{
		// chunk
		if(srcLen < chunkLen) // chunk content
		{
			if(srcLen > 0)
				memmove(dst, src, srcLen);

			int r = socket_recv_all_by_time(socket, dst+srcLen, chunkLen-srcLen, 0, timeout);
			if(r != chunkLen-srcLen)
				return r;
		}
		else
		{
			memmove(dst, src, chunkLen);
		}
	}

	// recv chunked tail "\r\n"
	if(srcLen < chunkLen+2)
	{
		int n = 2-(MAX(srcLen-chunkLen, 0));
		assert(n > 0);

		char dummy[2];
		int r = socket_recv_all_by_time(socket, dummy, n, 0, timeout);
		if(r != n)
			return r;
	}
	return chunkLen+2;
}

int HttpSocket::RecvChunked(mmptr& reply)
{
	reply.clear();
	reply.reserve(m_ptr.size()+3000);
	size_t total = 0;
	int chunkLen = 0;

	int r = m_ptr.size();
	char* line = m_ptr.size()?(char*)m_ptr.get():NULL;

	do
	{
		// chunk header
		char* p = line?strchr(line, '\n'):NULL;
		if(!p)
		{
			// recv header
			r = SocketRecvLine(m_ptr, m_socket, m_recvTimeout);
			if(r <= 0)
				return ERROR_RECV;

			line = m_ptr;
			p = strchr(line, '\n');
			assert(p);
		}
		
		// chunk size
		assert(p>line && *(p-1)=='\r');
		chunkLen = str2int_radix16(line);
		if(chunkLen > 50*1024*1024)
		{
			assert(false);
			return ERROR_REPLY;
		}
		
		// alloc memory
		if(reply.capacity() < total+chunkLen)
		{
			if(reply.reserve(total+chunkLen+3000))
				return ERROR_MEMORY;
		}

		++p; // skip '\n'
		int n = r - (p-(char*)m_ptr.get());
		if(chunkLen+2!=RecvChunkedHelper(p, n, (char*)reply.get()+total, chunkLen, m_socket, m_recvTimeout))
			return ERROR_RECV;

		total += chunkLen;
		
		if(n < chunkLen+2)
		{
			line = NULL;
			m_ptr.clear();
		}
		else
		{
			line = p + chunkLen + 2;
		}
	} while(chunkLen > 0);

	reply.set(reply.get(), total); // set reply size
	return 0;
}

int HttpSocket::RecvAllServerReply(mmptr& reply)
{
	// copy receive data
	size_t total = m_ptr.size();
	reply.reserve(total+3000);
	reply.set(m_ptr.get(), m_ptr.size());

	// receive all until server close connection
	while(reply.capacity() > total)
	{
		// receive content
		void* p = (char*)reply.get() + total;
		size_t n = reply.capacity() - total;
		int r = socket_recv_by_time(m_socket, p, n, 0, m_recvTimeout);
		if(0 == r)
			break; // timeout or server close connection
		else if(r < 0)
			return ERROR_RECV;
		
		total += r;
		if(reply.capacity()<=total)
			reply.reserve(total+3000);
	}

	// server close socket, client should close it.
	reply.set(reply.get(), total); // set reply size
	Disconnect();
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
	// http header
	int r = RecvHeader(m_response);
	if(r)
	{
		assert(r < 0);
		Disconnect();
		return r;
	}

	// set cookie
	std::string value;
	if(m_response.GetHeader("Set-Cookie", value))
	{
		std::string cookien, cookiev;
		Cookie cookie(value.c_str());
		if(cookie.GetNameValue(cookien, cookiev))
			SetCookie((cookien+'='+cookiev).c_str());
	}

	// url redirect(3xx: move)
	int status = m_response.GetStatusCode();
	if(status>=300 && status<400)
	{
		std::string location;
		assert(m_response.HasHeader("location"));
		m_response.GetHeader("location", location);
		reply.set(location.c_str());
	}
	else if(m_response.HasHeader("Content-Length"))
	{
		r = m_response.GetContentLength();
		if(r > 0)
			r = RecvContent(r, reply);
	}
	else if(m_response.IsTransferEncodingTrunked())
	{
		r = RecvChunked(reply);
	}
	else
	{
		r = RecvAllServerReply(reply);
	}

	if(r)
	{
		assert(r < 0);
		Disconnect();
		return r;
	}

	// auto fill '\0'
	reply.reserve(reply.size()+1);
	reply[reply.size()] = 0;
	
	if(m_response.ShouldCloseConnection())
		Disconnect();
	return (status>=300 && status<400)?ERROR_HTTP_REDIRECT:0;
}
