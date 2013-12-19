#include "http-server.h"
#include "http-parser.h"
#include "cstringext.h"
#include "error.h"
#include <string>

typedef struct _http_server_context
{
	socket_t socket;
	int recvTimeout;
	int sendTimeout;
	std::string reply;
	void* http;
	char buffer[8*1024];
	int remain;
} HttpServer;

void* http_server_create(socket_t sock)
{
	HttpServer* ctx;
	ctx = new HttpServer;
	ctx->remain = 0;
	ctx->socket = sock;
	ctx->http = http_parser_create(HTTP_PARSER_SERVER);
	return ctx;
}

int http_server_destroy(void **server)
{
	HttpServer* ctx;
	if(server && *server)
	{
		ctx = (HttpServer*)*server;
		http_parser_destroy(ctx->http);
		if(ctx->socket != socket_invalid)
			socket_close(ctx->socket);
		delete ctx;
		*server = 0;
	}
	return 0;
}

void http_server_set_timeout(void *server, int recv, int send)
{
	HttpServer* ctx = (HttpServer*)server;
	ctx->recvTimeout = recv;
	ctx->sendTimeout = send;
}

void http_server_get_timeout(void *server, int *recv, int *send)
{
	HttpServer* ctx = (HttpServer*)server;
	*recv = ctx->recvTimeout;
	*send = ctx->sendTimeout;
}

const char* http_server_get_path(void *server)
{
	HttpServer* ctx = (HttpServer*)server;
	return http_get_request_uri(ctx->http);
}

const char* http_server_get_method(void *server)
{
	HttpServer* ctx = (HttpServer*)server;
	return http_get_request_method(ctx->http);
}

int http_server_get_content(void *server, void **content, int *length)
{
	HttpServer* ctx = (HttpServer*)server;
	*content = (void*)http_get_content(ctx->http);
	*length = http_get_content_length(ctx->http);
	return 0;
}

const char* http_server_get_header(void *server, const char *name)
{
	HttpServer* ctx = (HttpServer*)server;
	return http_get_header_by_name(ctx->http, name);
}

int http_server_recv(void *server)
{
	int status;
	HttpServer* ctx = (HttpServer*)server;
	http_parser_clear(ctx->http);
	ctx->reply.clear();

	do
	{
		void* p = ctx->buffer + ctx->remain;
		int r = socket_recv_by_time(ctx->socket, p, sizeof(ctx->buffer) - ctx->remain, 0, ctx->recvTimeout);
		if(r < 0)
			return ERROR_RECV;

		ctx->remain = r;
		status = http_parser_input(ctx->http, ctx->buffer, &ctx->remain);
		if(status < 0)
			return status; // parse error

		if(0 == status)
		{
			if(ctx->remain > 0)
				memmove(ctx->buffer, ctx->buffer+(r-ctx->remain), ctx->remain);
		}

	} while(1 == status);

	return 0;
}

int http_server_send(void* server, int code, const void* data, int bytes)
{
	char status[256] = {0};
	socket_bufvec_t vec[4];
	HttpServer* ctx = (HttpServer*)server;

	sprintf(status, "HTTP/1.1 %d OK\r\n", code);

	socket_setbufvec(vec, 0, status, strlen(status));
	socket_setbufvec(vec, 1, (void*)ctx->reply.c_str(), ctx->reply.length());
	socket_setbufvec(vec, 2, (void*)"\r\n", 2);
	socket_setbufvec(vec, 3, (void*)data, bytes);

	int r = socket_send_v(ctx->socket, vec, bytes>0?4:3, 0);
	return (r==(int)(strlen(status)+ctx->reply.length()+2+bytes))?0:-1;
}

int http_server_set_header(void *server, const char* name, const char* value)
{
	char msg[256] = {0};
	HttpServer* ctx = (HttpServer*)server;
	sprintf(msg, "%s: %s\r\n", name, value);
	ctx->reply += msg;
	return 0;
}

int http_server_set_header_int(void *server, const char* name, int value)
{
	char msg[256] = {0};
	HttpServer* ctx = (HttpServer*)server;
	sprintf(msg, "%s: %d\r\n", name, value);
	ctx->reply += msg;
	return 0;
}
