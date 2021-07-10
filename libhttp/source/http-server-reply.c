#include "http-server.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int http_server_onreply(void* param, int code, size_t bytes)
{
	(void)bytes;
	if (param){
		free(param);
	}	
	return code;
}

int http_server_reply(http_session_t* session, int code, const void* data, size_t bytes)
{
	void* ptr = NULL;
	
	if (bytes > 0)
	{
		ptr = malloc(bytes);
		if (NULL == ptr)
			return -ENOMEM;
		memcpy(ptr, data, bytes);
	}

	http_server_set_status_code(session, code, NULL);
	return http_server_send(session, ptr, bytes, http_server_onreply, ptr);
}
