#ifndef _http_server_h_
#define _http_server_h_

#include "sys/sock.h"
#include <string>

class HttpServer
{
public:
	HttpServer(socket_t sock);
	~HttpServer();

public:
	void SetTimeout(int recv, int send);
	void GetTimeout(int *recv, int *send);

	int Recv();
	const char* GetPath();
	const char* GetMethod();
	const char* GetHeader(const char *name);
	int GetContent(void **content, int *length);

	int Send(int code, const void* data, int bytes);
	int SetHeader(const char* name, const char* value);
	int SetHeader(const char* name, int value);

private:
	socket_t m_socket;
	int m_recvTimeout;
	int m_sendTimeout;
	std::string m_reply;
	void* m_parser;
	
	char m_buffer[8 * 1024];
	size_t m_remain;
};

#endif /* !_http_server_h_ */
