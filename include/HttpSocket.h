#ifndef _HttpdSocket_h_
#define _HttpdSocket_h_

#include <list>
#include <string>
#include "sys/sock.h"
#include "cstringext.h"
#include "http-parser.h"
#include "mmptr.h"

class HttpResponse
{
public:
	int GetStatusCode() const			{ return http_get_status_code(m_http); }
	int GetContentLength() const		{ return http_get_content_length(m_http); }
	const char* GetReply() const		{ return (const char*)http_get_content(m_http); }
	bool ShouldCloseConnection() const	{ return !!http_get_connection(m_http); } // Connection: close

	bool IsTransferEncodingTrunked() const
	{
		std::string value;
		if(!GetHeader("Transfer-Encoding", value))
			return false;

		return 0==stricmp("chunked", value.c_str());
	}

	bool GetContentEncoding(std::string& value) const
	{
		return GetHeader("Content-Encoding", value);
	}

	bool GetHeader(const std::string& name, std::string& value) const
	{
		const char* v = http_get_header_by_name(m_http, name.c_str());
		if(NULL == v)
			return false;
		value.assign(v);
		return true;
	}

	bool HasHeader(const char* name) const
	{
		return !!http_get_header_by_name(m_http, name);
	}

public:
	void* m_http;
};

class HttpSocket
{
public:
	HttpSocket();
	HttpSocket(const char* ip, int port, int connTimeout=3000, int recvTimeout=5000);
	~HttpSocket();

public:
	int SetSocket(socket_t sock); // must SetHeader("HOST", host)
	socket_t GetSocket() const;
	socket_t DetachSocket();
	
	// connection
	int Connect();
	int Connect(const char* ip, int port);
	int Disconnect();
	bool IsConnected() const;
	
	int GetConnTimeout() const { return m_connTimeout; }
	int GetRecvTimeout() const { return m_recvTimeout; }
	void SetConnTimeout(int timeout) { m_connTimeout = timeout; }
	void SetRecvTimeout(int timeout) { m_recvTimeout = timeout; }	
	void EnableAutoConnect(bool enable){ m_autoconnect = enable; }

	void GetHost(std::string& ip, int& port){ ip=m_ip; port=m_port; }

public:
	void SetHeader(const char* name, int value);
	void SetHeader(const char* name, const char* value);
	void KeepAlive(int ms);

	void SetCookie(const char* cookies); // SetCookie("name=value; name2=value2")
	bool GetCookie(std::string& cookies);

	const HttpResponse& GetResponse() const{ return m_response; }

public:
	int Get(const char* uri);
	int Post(const char* uri, const void* content, size_t len);

private:
	int _Get(const char* uri);
	// @return send length
	int _Post(const char* uri, const void* content, size_t len);

	int _GetReply();

	int CheckConnection();

private:
	// header api
	typedef std::pair<std::string, std::string> THttpHeader;
	typedef std::list<THttpHeader> THttpHeaders;
	THttpHeaders::iterator FindHeader(const char* name);
	THttpHeaders::const_iterator FindHeader(const char* name) const;

	void InitHttpHeader();
	void SetContentLength(int length);
	int MakeHttpHeader(mmptr&reply, const char* method, const char* uri, int contentLength) const;

private:
	int m_connFailed;
	bool m_autoconnect;
	int m_connTimeout;
	int m_recvTimeout;
	socket_t m_socket;
	THttpHeaders m_headers;

	void *m_http;
	mmptr m_ptr;
	HttpResponse m_response;

	int m_port;
	std::string m_ip;
};

#endif /* !_HttpdSocket_h_ */
