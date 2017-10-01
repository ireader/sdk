#include "http-server.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int http_server_onreply(void* param, int code, size_t bytes)
{
	(void)bytes;
	free(param);
	return code;
}

int http_server_reply(http_session_t* session, int code, const void* data, size_t bytes)
{
	void* ptr;
	
	ptr = malloc(bytes);
	if (NULL == ptr)
		return -ENOMEM;
	memcpy(ptr, data, bytes);
	return http_server_send(session, code, ptr, bytes, http_server_onreply, ptr);
}
