#include "http-transport.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

struct http_transport_pool_t
{
    struct http_transport_t* t;
    
    void* map; // map schema -> connection
};

void* http_transport_pool_create(struct http_transport_t* transport)
{
    struct http_transport_pool_t* pool;
    pool = (struct http_transport_pool_t*)calloc(1, sizeof(*pool));
    if(!pool) return NULL;
    
    pool->t = transport;
    return pool;
}

int http_transport_pool_release(void* priv)
{
    struct http_transport_pool_t* pool;
    pool = (struct http_transport_pool_t*)priv;
    free(pool);
    return 0;
}

void* http_transport_pool_fetch(void* priv, const char* schema, const char* host, int port)
{
    return NULL;
}

int http_transport_pool_put(void* priv, void* connection, void (*destroy)(void* connection))
{
    struct http_transport_pool_t* pool;
    pool = (struct http_transport_pool_t*)priv;
    destroy(connection);
    return 0;
}

int http_transport_pool_check(void* priv)
{
    struct http_transport_pool_t* pool;
    pool = (struct http_transport_pool_t*)priv;
    // check max idle time
    return 0;
}

int http_transport_release(struct http_transport_t* t)
{
    if (t == http_transport_default() || t == http_transport_default_aio())
    {
        //http_transport_pool_release(t->priv);
        return 0; // nothing to do
    }
    
    //http_transport_pool_release(t->priv);
    free(t);
    return 0;
}
