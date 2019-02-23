#ifndef _stun_internal_h_
#define _stun_internal_h_

#include "stun-attr.h"
#include "stun-proto.h"
#include "stun-message.h"
#include "list.h"
#include <stdint.h>
#include <assert.h>

struct stun_request_t
{
    struct list_head link;
    struct stun_message_t msg;
    void* param;
};

struct stun_response_t
{
    struct list_head link;
    uint64_t id;
    void* param;
};

#endif /* !_stun_internal_h_ */
