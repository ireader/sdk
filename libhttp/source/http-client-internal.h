#ifndef _http_client_internal_h_
#define _http_client_internal_h_

#include "http-client.h"
#include "sys/sock.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "http-parser.h"
#include "http-request.h"
#include "http-transport.h"

struct http_client_t
{
	http_client_onresponse onreply;
	void *cbparam;

	volatile int32_t ref;
	locker_t locker;
    
    char scheme[64];
	char host[128];
	unsigned short port;
	socket_t socket;

	struct {
		int conn;
		int recv;
		int send;
	} timeout;

	char *cookie;
	size_t ncookie;
	http_parser_t* parser;
	void* req;
    
    int tryagain;
    int status; // http_parser_input return value
    
    struct {
        int method;
        const char* uri;
        const struct http_header_t *headers;
        size_t n;
        const void* msg;
        size_t bytes;
    } parameters;
    
    struct {
        uint8_t* ptr;
        int off, len, cap;
        
        // http_client_read parameters
        uint8_t *data;
        int readed;
        int bytes;
        int flags;
        void (*onread)(void* param, int code, void* data, size_t bytes);
        void* param;
    } body;
    
    struct {
        int (*onredirect)(void* param, const char* urls[], int n);
        void* param;
        
        char* urls[32]; // max depth: 32
        int n;
    } redirect;
    
	void* connection;
	struct http_transport_t* transport;
    char buffer[1024];
};

struct http_transport_pool_t
{
    struct http_transport_t* t;
    
    void* map; // map schema -> connection
};

void* http_transport_pool_fetch(void* priv, const char* schema, const char* host, int port);
int http_transport_pool_put(void* priv, void* connection, void (*destroy)(void* connection));
int http_transport_pool_check(void* priv);

void http_client_release(struct http_client_t* http);
void http_client_handle(struct http_client_t *http, int code);

#endif /* !_http_client_internal_h_ */
