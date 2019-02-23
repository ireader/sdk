#include "turn-client.h"
#include "stun-message.h"
#include "stun-internal.h"
#include "byte-order.h"
#include "sockutil.h"
#include "list.h"
#include <stdlib.h>
#include <stdio.h>

struct turn_permission_t
{
    struct list_head link;
    struct sockaddr_storage addr; // Note that only addresses are compared and port numbers are not considered.
    uint64_t expired; // expired clock
};

// rfc 5766 5. Allocations (p22)
struct turn_allocate_t
{
    struct list_head link;
    uint16_t id[16];
    
    int transport; // UDP/TCP/TLS
    struct sockaddr_storage relay;
    struct sockaddr_storage client;
    struct sockaddr_storage server;
    
    // By default, each Allocate or Refresh transaction resets this
    // timer to the default lifetime value of 600 seconds (10 minutes)
    uint32_t lifetime; // default 600s ~ 3600s
    int permissions;
    int channels;
    
    // authentication;
    char usr[256], pwd[256];
    char realm[256], nonce[256];
};

struct turn_client_t
{
    struct turn_client_handler_t handler;
    
    char usr[256], pwd[256];
    char realm[256], nonce[256];
    
    struct list_head root; // allocate link
};

struct turn_client_t* turn_client_create(struct turn_client_handler_t* handler)
{
    struct turn_client_t* turn;
    turn = (struct turn_client_t*)calloc(1, sizeof(*turn));
    if (turn)
    {
        LIST_INIT_HEAD(&turn->root);
        snprintf(turn->usr, sizeof(turn->usr), "tao3@outlook.com");
        snprintf(turn->pwd, sizeof(turn->pwd), "12345678");
        memcpy(&turn->handler, handler, sizeof(turn->handler));
    }
    return turn;
}

int turn_client_destroy(turn_client_t* turn)
{
    struct turn_request_t* req;
    struct list_head* pos, *next;
    list_for_each_safe(pos, next, &turn->root)
    {
        req = list_entry(pos, struct stun_request_t, link);
        free(req);
    }
    free(turn);
    return 0;
}


// rfc3489 8.2 Shared Secret Requests (p13)
// rfc3489 9.3 Formulating the Binding Request (p17)
int turn_client_allocate(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, void* param)
{
    int r;
    struct stun_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.msgtype = STUN_METHOD_ALLOCATE | STUN_METHOD_CLASS_REQUEST;
    msg.header.length = 0;
    msg.header.cookie = STUN_MAGIC_COOKIE;
    stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));
    
//    msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
//    msg.attrs[msg.nattrs].length = (uint16_t)strlen(STUN_SOFTWARE);
//    msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
//    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_REQUESTED_TRANSPORT;
    msg.attrs[msg.nattrs].length = 4;
    msg.attrs[msg.nattrs].v.u32 = TURN_TRANSPORT_UDP << 24; // UDP
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_LIFETIME;
    msg.attrs[msg.nattrs].length = 4;
    msg.attrs[msg.nattrs].v.u32 = TURN_LIFETIME;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
//    msg.attrs[msg.nattrs].type = STUN_ATTR_DONT_FRAGMENT;
//    msg.attrs[msg.nattrs].length = 0;
//    msg.header.length += 4;
//
//    msg.attrs[msg.nattrs].type = STUN_ATTR_EVEN_PORT;
//    msg.attrs[msg.nattrs].length = 1;
//    msg.attrs[msg.nattrs].v.u8 = 0x80;
//    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);

    r = stun_message_add_credentials(&msg, turn->usr, turn->pwd, turn->realm, turn->nonce);
    r = 0 == r ? stun_message_add_fingerprint(&msg) : r;
    r = 0 == r ? stun_message_write(data, bytes, &msg) : r;
    return 0 == r ? msg.header.length + STUN_HEADER_SIZE : r;
}

int turn_client_refresh(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, int expired, void* param)
{
    int r;
    struct stun_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.msgtype = STUN_METHOD_REFRESH | STUN_METHOD_CLASS_REQUEST;
    msg.header.length = 0;
    msg.header.cookie = STUN_MAGIC_COOKIE;
    stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
    msg.attrs[msg.nattrs].length = (uint16_t)strlen(STUN_SOFTWARE);
    msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_LIFETIME;
    msg.attrs[msg.nattrs].length = 4;
    msg.attrs[msg.nattrs].v.u32 = expired;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    r = stun_message_add_credentials(&msg, turn->usr, turn->pwd, turn->realm, turn->nonce);
    r = 0 == r ? stun_message_add_fingerprint(&msg) : r;
    r = 0 == r ? stun_message_write(data, bytes, &msg) : r;
    return 0 == r ? msg.header.length + STUN_HEADER_SIZE : r;
}

int turn_client_permission(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, const struct sockaddr_storage* peer, void* param)
{
    int r;
    struct stun_message_t msg;
    
    if(AF_INET != peer->ss_family && AF_INET6 != peer->ss_family)
        return -1;
    
    memset(&msg, 0, sizeof(msg));
    msg.header.msgtype = STUN_METHOD_REFRESH | STUN_METHOD_CLASS_REQUEST;
    msg.header.length = 0;
    msg.header.cookie = STUN_MAGIC_COOKIE;
    stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
    msg.attrs[msg.nattrs].length = (uint16_t)strlen(STUN_SOFTWARE);
    msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_XOR_PEER_ADDRESS;
    msg.attrs[msg.nattrs].length = peer->ss_family == AF_INET6 ? 17 : 5;
    memcpy(&msg.attrs[msg.nattrs].v.addr, peer, sizeof(struct sockaddr_storage));
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    r = stun_message_add_credentials(&msg, turn->usr, turn->pwd, turn->realm, turn->nonce);
    r = 0 == r ? stun_message_add_fingerprint(&msg) : r;
    r = 0 == r ? stun_message_write(data, bytes, &msg) : r;
    return 0 == r ? msg.header.length + STUN_HEADER_SIZE : r;
}

int turn_client_channel(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, const struct sockaddr_storage* peer, uint16_t channel, void* param)
{
    int r;
    struct stun_message_t msg;
    
    if(AF_INET != peer->ss_family && AF_INET6 != peer->ss_family)
        return -1;
    
    memset(&msg, 0, sizeof(msg));
    msg.header.msgtype = STUN_METHOD_CHANNEL_BIND | STUN_METHOD_CLASS_REQUEST;
    msg.header.length = 0;
    msg.header.cookie = STUN_MAGIC_COOKIE;
    stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
    msg.attrs[msg.nattrs].length = (uint16_t)strlen(STUN_SOFTWARE);
    msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_XOR_PEER_ADDRESS;
    msg.attrs[msg.nattrs].length = peer->ss_family == AF_INET6 ? 17 : 5;
    memcpy(&msg.attrs[msg.nattrs].v.addr, peer, sizeof(struct sockaddr_storage));
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_CHANNEL_NUMBER;
    msg.attrs[msg.nattrs].length = 4;
    msg.attrs[msg.nattrs].v.u32 = channel << 16;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    r = stun_message_add_credentials(&msg, turn->usr, turn->pwd, turn->realm, turn->nonce);
    r = 0 == r ? stun_message_add_fingerprint(&msg) : r;
    r = 0 == r ? stun_message_write(data, bytes, &msg) : r;
    return 0 == r ? msg.header.length + STUN_HEADER_SIZE : r;
}

int turn_client_send(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, const struct sockaddr* peer, void* param)
{
    int r;
    struct stun_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.msgtype = STUN_METHOD_SEND | STUN_METHOD_CLASS_INDICATION;
    msg.header.length = 0;
    msg.header.cookie = STUN_MAGIC_COOKIE;
    stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
    msg.attrs[msg.nattrs].length = (uint16_t)strlen(STUN_SOFTWARE);
    msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_DONT_FRAGMENT;
    msg.attrs[msg.nattrs].length = 0;
    msg.header.length += 4;
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_XOR_PEER_ADDRESS;
    msg.attrs[msg.nattrs].length = peer->sa_family == AF_INET6 ? 17 : 5;
    memcpy(&msg.attrs[msg.nattrs].v.addr, peer, sizeof(struct sockaddr_storage));
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    msg.attrs[msg.nattrs].type = STUN_ATTR_DATA;
    msg.attrs[msg.nattrs].length = 4;
    msg.attrs[msg.nattrs].v.u32 = 0;
    msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);
    
    r = stun_message_add_credentials(&msg, turn->usr, turn->pwd, turn->realm, turn->nonce);
    r = 0 == r ? stun_message_add_fingerprint(&msg) : r;
    r = 0 == r ? stun_message_write(data, bytes, &msg) : r;
    return 0 == r ? msg.header.length + STUN_HEADER_SIZE : r;
}

int turn_client_input(turn_client_t* turn, const void* data, int size, const struct sockaddr* addr, socklen_t addrlen)
{
    int i, r;
    struct stun_message_t msg;
    memset(&msg, 0, sizeof(msg));
    r = stun_message_read(&msg, data, size);
    struct turn_allocate_t allocate;
    memset(&allocate, 0, sizeof(allocate));
    
    for (i = 0; i < msg.nattrs; i++)
    {
        switch (msg.attrs[i].type)
        {
        case STUN_ATTR_NONCE:
            snprintf(turn->nonce, sizeof(turn->nonce), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
            break;
            
        case STUN_ATTR_REALM:
            snprintf(turn->realm, sizeof(turn->realm), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
            break;
            
        case STUN_ATTR_USERNAME:
            snprintf(turn->usr, sizeof(turn->usr), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
            break;
            
        case STUN_ATTR_PASSWORD:
            snprintf(turn->pwd, sizeof(turn->pwd), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
            break;
            
        case STUN_ATTR_ERROR_CODE:
            break;
                
        case STUN_ATTR_XOR_MAPPED_ADDRESS:
            memcpy(&allocate.client, &msg.attrs[i].v.addr, sizeof(struct sockaddr_storage));
            break;
    
        case STUN_ATTR_XOR_RELAYED_ADDRESS:
            memcpy(&allocate.relay, &msg.attrs[i].v.addr, sizeof(struct sockaddr_storage));
            break;
                
            case STUN_ATTR_LIFETIME:
                allocate.lifetime = msg.attrs[i].v.u32;
                break;
                
                case STUN_ATTR_BANDWIDTH
        }
    }
    
    return r;
}
