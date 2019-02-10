#include "stun-client.h"
#include "stun-message.h"
#include "stun-internal.h"
#include "byte-order.h"
#include "sockutil.h"
#include "list.h"

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

struct stun_client_t
{
	int rfc; // version
	struct stun_client_handler_t handler;

	char usr[256], pwd[256];
	char realm[256], nonce[256];

	struct list_head root; // 
};

struct stun_client_t* stun_client_create(int rfc, struct stun_client_handler_t* handler)
{
	struct stun_client_t* stun;
	stun = (struct stun_client_t*)calloc(1, sizeof(*stun));
	if (stun)
	{
		stun->rfc = rfc;
		LIST_INIT_HEAD(&stun->root);
		memcpy(&stun->handler, handler, sizeof(stun->handler));
	}
	return stun;
}

int stun_client_destroy(stun_client_t* stun)
{
	struct stun_request_t* req;
	struct list_head* pos, *next;
	list_for_each_safe(pos, next, &stun->root)
	{
		req = list_entry(pos, struct stun_request_t, link);
		free(req);
	}
	free(stun);
	return 0;
}

// rfc3489 8.2 Shared Secret Requests (p13)
// rfc3489 9.3 Formulating the Binding Request (p17)
int stun_client_bind(stun_client_t* stun, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, void* param)
{
	int r;
	struct stun_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header.msgtype = STUN_METHOD_BIND;
	msg.header.length = 0;
	msg.header.cookie = STUN_MAGIC_COOKIE;
	stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));

	msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
	msg.attrs[msg.nattrs].length = (uint16_t)strlen(STUN_SOFTWARE);
	msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
	msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);

	r = stun_message_add_credentials(&msg, stun->usr, stun->pwd, stun->realm, stun->nonce);
	r = 0 == r ? stun_message_add_fingerprint(&msg) : r;
	r = 0 == r ? stun_message_write(data, bytes, &msg) : r;
	return 0 == r ? msg.header.length + STUN_HEADER_SIZE : r;
}

// rfc3489 8.1 Binding Requests (p10)
// rfc3489 9.2 Obtaining a Shared Secret (p15)
int stun_client_shared_secret(stun_client_t* stun, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, void* param)
{
	struct stun_message_t msg;
	memset(&msg, 0, sizeof(msg));

	// This request has no attributes, just the header
	msg.header.msgtype = STUN_METHOD_SHARED_SECRET;
	msg.header.length = 0;
	msg.header.cookie = STUN_MAGIC_COOKIE;
	stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));

	msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
	msg.attrs[msg.nattrs].length = strlen(STUN_SOFTWARE);
	msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
	msg.header.length += 4 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);

	return stun_message_write(data, bytes, &msg);
}

int stun_client_input(stun_client_t* stun, const void* data, int size, const struct sockaddr* addr, socklen_t addrlen)
{
	int i, r;
	struct stun_message_t msg;
	memset(&msg, 0, sizeof(msg));
	r = stun_message_read(&msg, data, size);

	for (i = 0; i < msg.nattrs; i++)
	{
		switch (msg.attrs[i].type)
		{
		case STUN_ATTR_NONCE:
			snprintf(stun->nonce, sizeof(stun->nonce), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
			break;

		case STUN_ATTR_REALM:
			snprintf(stun->realm, sizeof(stun->realm), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
			break;

		case STUN_ATTR_USERNAME:
			snprintf(stun->usr, sizeof(stun->usr), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
			break;

		case STUN_ATTR_PASSWORD:
			snprintf(stun->pwd, sizeof(stun->pwd), "%.*s", msg.attrs[i].length, msg.attrs[i].v.string);
			break;

		case STUN_ATTR_ERROR_CODE:
			switch (msg.attrs[i].v.errcode.code)
			{
			}
			break;
		}
	}

	return r;
}
