#include "http-server-internal.h"
#include "sys/path.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define N_SENDFILE 65535

struct http_sendfile_t
{
	struct http_session_t* session;
	http_server_onsend onsend;
	void* param;

	FILE* fp;
	size_t bytes;
	size_t total;
	uint8_t* ptr;
	int eof;
};

static void http_server_readfile(struct http_sendfile_t* sendfile)
{
	static const char* hex = "0123456789ABCDEF";

	sendfile->bytes = fread(sendfile->ptr + 6, 1, N_SENDFILE, sendfile->fp);
	if (0 == sendfile->bytes)
	{
		// last-chunk
		sendfile->ptr[0] = 0; // length
		sendfile->ptr[1] = 0;
		sendfile->ptr[2] = 0;
		sendfile->ptr[3] = 0;
		sendfile->ptr[4] = '\r';
		sendfile->ptr[5] = '\n';

		// \r\n
		sendfile->ptr[6] = '\r';
		sendfile->ptr[7] = '\n';
		sendfile->bytes = 8;
		sendfile->eof = 1;
	}
	else
	{
		// chunk
		sendfile->ptr[0] = (uint8_t)hex[(sendfile->bytes >> 12) & 0x0F];
		sendfile->ptr[1] = (uint8_t)hex[(sendfile->bytes >> 8) & 0x0F];
		sendfile->ptr[2] = (uint8_t)hex[(sendfile->bytes >> 4) & 0x0F];
		sendfile->ptr[3] = (uint8_t)hex[sendfile->bytes & 0x0F];
		sendfile->ptr[4] = '\r';
		sendfile->ptr[5] = '\n';
		sendfile->ptr[sendfile->bytes + 6] = '\r';
		sendfile->ptr[sendfile->bytes + 7] = '\n';
		sendfile->bytes += 8;
	}	
}

static void http_server_onsendfile(void* param, int code, size_t bytes)
{
	struct http_sendfile_t* sendfile;
	sendfile = (struct http_sendfile_t*)param;

	if (0 == code)
	{
		assert(sendfile->bytes <= bytes); // bytes: http header + content
		sendfile->total += sendfile->bytes - 8;
		if (1 == sendfile->eof)
		{
			if (sendfile->onsend)
				sendfile->onsend(sendfile->param, 0, sendfile->total);
			fclose(sendfile->fp);
			free(sendfile);
			return;
		}
		
		http_server_readfile(sendfile);
		code = aio_tcp_transport_send(sendfile->session->transport, sendfile->ptr, sendfile->bytes);
	}

	if(0 != code)
	{
		if (sendfile->onsend)
			sendfile->onsend(sendfile->param, code, sendfile->total);
		fclose(sendfile->fp);
		free(sendfile);
	}
}

int http_server_sendfile(struct http_session_t* session, const char* localpath, const char* filename, http_server_onsend onsend, void* param)
{
	int n;
	FILE* fp;
	char content_disposition[512];
	struct http_sendfile_t* sendfile;

	n = snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s\"", filename ? filename : path_basename(localpath));

	fp = fopen(localpath, "rb");
	if (NULL == fp)
		return -ENOENT;

	sendfile = (struct http_sendfile_t*)malloc(sizeof(*sendfile) + N_SENDFILE + 8);
	if (NULL == sendfile)
	{
		fclose(fp);
		return -ENOMEM;
	}

	sendfile->session = session;
	sendfile->onsend = onsend;
	sendfile->param = param;
	sendfile->fp = fp;
	sendfile->ptr = (uint8_t*)(sendfile + 1);
	sendfile->total = 0;
	sendfile->eof = 0;
	http_server_readfile(sendfile);

	http_server_set_header(session, "Transfer-Encoding", "chunked");
	http_server_set_header(session, "Content-Disposition", content_disposition);
	http_server_set_content_type(session, "application/octet-stream"); // add MIME

	return http_server_send(session, 200, sendfile->ptr, sendfile->bytes, http_server_onsendfile, sendfile);
}
