#ifndef _HttpdSocket_h_
#define _HttpdSocket_h_

#include <map>
#include <list>
#include <string>
#include "sys/sock.h"
#include "cstringext.h"
#include "mmptr.h"

class HttpResponse
{
public:
	HttpResponse()
	{
		m_httpVersion = "1.1";	// http version
		m_statusCode = 200;		// http response ok
		m_contentLength = 0;
		m_connectionClose = false;
	}
	~HttpResponse(){}

public:
	const char* GetHttpVersion() const	{ return m_httpVersion.c_str(); }
	int GetStatusCode() const			{ return m_statusCode; }
	int GetContentLength() const		{ return m_contentLength; }
	bool ShouldCloseConnection() const	{ return m_connectionClose; } // Connection: close
	
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

	void SetHeader(const std::string& name, const std::string& value)
	{
		m_headers.insert(std::make_pair(name, value));
	}

	bool GetHeader(const std::string& name, std::string& value) const
	{
		THeaders::const_iterator it = m_headers.find(name);
		if(it == m_headers.end())
			return false;
		value = it->second;
		return true;
	}

	bool HasHeader(const char* name) const { return m_headers.find(name)!=m_headers.end(); }

	void SetHttpVersion(const char* version){ m_httpVersion = version; }
	void SetStatusCode(int status)		{ m_statusCode = status; }
	void SetContentLength(int length)	{ m_contentLength = length; }
	void SetConnectionClose(bool close) { m_connectionClose = close; }

private:
	std::string m_httpVersion;
	int m_statusCode;
	int m_contentLength;
	bool m_connectionClose;

	struct less
	{
		bool operator()(const std::string& l, const std::string& r) const
		{
			return stricmp(l.c_str(), r.c_str())<0;
		}
	};
	typedef std::map<std::string, std::string, less> THeaders;
	THeaders m_headers;
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
	int Get(const char* uri, mmptr& reply);
	int Post(const char* uri, const void* content, size_t len, mmptr& reply);

public:
	int GetReply(mmptr& reply);

private:
	int _Get(const char* uri);
	// @return send length
	int _Post(const char* uri, const void* content, size_t len);

	// @return =0 - ok, <0 - error
	int RecvHeader(HttpResponse& response);
	// @return >0 - receive content length, -1 - timeout, <0 - error
	int RecvContent(size_t contentLength, mmptr& reply);
	// @return >0
	int RecvChunked(mmptr& reply);
	// @return >0
	int RecvAllServerReply(mmptr& reply);

public:
	// Post + RecvContent
	// @return =0 - ok, <0 - error

private:
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

	mmptr m_ptr;
	HttpResponse m_response;

	int m_port;
	std::string m_ip;
};

#endif /* !_HttpdSocket_h_ */
