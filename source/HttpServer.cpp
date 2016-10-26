#include "HttpServer.h"
#include "http-parser.h"
#include "cstringext.h"
#include "error.h"

HttpServer::HttpServer(socket_t sock)
{
	m_recvTimeout = 5000;
	m_sendTimeout = 5000;
	m_remain = 0;
	m_socket = sock;
	m_parser = http_parser_create(HTTP_PARSER_SERVER);
}

HttpServer::~HttpServer()
{
	if (m_parser)
	{
		http_parser_destroy(m_parser);
		m_parser = NULL;
	}
		
	if (m_socket != socket_invalid)
	{
		socket_close(m_socket);
		m_socket = socket_invalid;
	}
}

void HttpServer::SetTimeout(int recv, int send)
{
	m_recvTimeout = recv;
	m_sendTimeout = send;
}

void HttpServer::GetTimeout(int *recv, int *send)
{
	*recv = m_recvTimeout;
	*send = m_sendTimeout;
}

const char* HttpServer::GetPath()
{
	return http_get_request_uri(m_parser);
}

const char* HttpServer::GetMethod()
{
	return http_get_request_method(m_parser);
}

int HttpServer::GetContent(void **content, int *length)
{
	*content = (void*)http_get_content(m_parser);
	*length = http_get_content_length(m_parser);
	return 0;
}

const char* HttpServer::GetHeader(const char *name)
{
	return http_get_header_by_name(m_parser, name);
}

int HttpServer::Recv()
{
	int status;
	http_parser_clear(m_parser);
	m_reply.clear();

	do
	{
		void* p = m_buffer + m_remain;
		int r = socket_recv_by_time(m_socket, p, sizeof(m_buffer) - m_remain, 0, m_recvTimeout);
		if(r < 0)
			return ERROR_RECV;

		m_remain = (size_t)r;
		status = http_parser_input(m_parser, m_buffer, &m_remain);
		if(status < 0)
			return status; // parse error

		if(0 == status)
		{
			if(m_remain > 0)
				memmove(m_buffer, m_buffer+(r-m_remain), m_remain);
		}

		if(0 == r && 1 == status)
			return ERROR_RECV; // peer close socket, don't receive all data

	} while(1 == status);

	return 0;
}

int HttpServer::Send(int code, const void* data, int bytes)
{
	char status[256] = {0};
	socket_bufvec_t vec[4];

	sprintf(status, "HTTP/1.1 %d OK\r\n", code);

	socket_setbufvec(vec, 0, status, strlen(status));
	socket_setbufvec(vec, 1, (void*)m_reply.c_str(), m_reply.length());
	socket_setbufvec(vec, 2, (void*)"\r\n", 2);
	socket_setbufvec(vec, 3, (void*)data, bytes);

	int r = socket_send_v(m_socket, vec, bytes>0?4:3, 0);
	return (r==(int)(strlen(status)+m_reply.length()+2+bytes))?0:-1;
}

int HttpServer::SetHeader(const char* name, const char* value)
{
	char msg[256] = {0};
	int r = snprintf(msg, sizeof(msg), "%s: %s\r\n", name, value);
	if (r + 1 >= sizeof(msg))
		return -1;
	m_reply += msg;
	return 0;
}

int HttpServer::SetHeader(const char* name, int value)
{
	char msg[256] = {0};
	int r = snprintf(msg, sizeof(msg), "%s: %d\r\n", name, value);
	if (r + 1 >= sizeof(msg))
		return -1;
	m_reply += msg;
	return 0;
}
