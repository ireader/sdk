#ifndef _http_transport_h_
#define _http_transport_h_

#ifdef __cplusplus
extern "C" {
#endif

struct http_transport_t
{
    void* priv; // internal use only
    int is_aio; // 1-aio, 0-block io
    
    int connect_timeout; // ms
    int idle_timeout; // ms
    int idle_connections; // max idle connections
    
    int read_buffer_size;
    int write_buffer_size;
    int keep_alive;
    
    /// create(or get a idle) connection
    void* (*connect)(struct http_transport_t* t, const char* scheme, const char* host, int port);
    int (*close)(void* c);
    
    void (*settimeout)(void* c, int conn, int recv, int send);

    /// @return 0-ok, other-error
    int (*recv)(void* c, void (*onrecv)(void* param, int code, const void* buf, int len), void* param);
    
    /// @return 0-ok, other-error
    int (*send)(void* c, const char* req, int nreq, const void* msg, int bytes, void (*onsend)(void* param, int code), void* param);
};

/// default http transport(same as http_transport_tcp with static tcp transport)
struct http_transport_t* http_transport_default(void);
/// default aio transport(same as http_transport_tcp_aio with static aio transport)
struct http_transport_t* http_transport_default_aio(void);

int http_transport_tcp(struct http_transport_t* tcp);
int http_transport_tcp_aio(struct http_transport_t* aio);

int http_transport_release(struct http_transport_t* t);

#ifdef __cplusplus
}
#endif
#endif /* _http_transport_h_ */
