#include "stun-server.h"
#include "stun-message.h"
#include "stun-internal.h"

int stun_server_bind(const uint8_t tid[12])
{
	struct stun_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header.msgtype = STUN_METHOD_BIND & 0x100;
	msg.header.length = 0;
	msg.header.cookie = STUN_MAGIC_COOKIE;
	memmove(msg.header.tid, tid, sizeof(msg.header.tid));

	msg.attrs[msg.nattrs].type = STUN_ATTR_SOFTWARE;
	msg.attrs[msg.nattrs].length = strlen(STUN_SOFTWARE);
	msg.attrs[msg.nattrs].v.data = STUN_SOFTWARE;
	msg.header.length += 8 + ALGIN_4BYTES(msg.attrs[msg.nattrs++].length);

	return 0;
}
