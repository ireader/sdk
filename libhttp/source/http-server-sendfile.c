#include "http-server-internal.h"
#include "http-header-range.h"
#include "rfc822-datetime.h"
#include "sys/path.h"
#include "ctypedef.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define N_SENDFILE (2 * 1024 * 1024)

#if defined(OS_WINDOWS)
#define fseek _fseeki64
#endif

struct http_sendfile_t
{
	struct http_session_t* session;
	http_server_onsend onsend;
	void* param;

	int code;
	FILE* fp;
	size_t capacity;
	size_t bytes;
	int64_t sent;
	int64_t total;
	uint8_t* ptr;
};

static struct http_sendfile_t* http_file_open(const char* filename)
{
	FILE* fp;
	int64_t size;
	size_t capacity;
	struct http_sendfile_t* sendfile;

	size = path_filesize(filename);
	fp = fopen(filename, "rb");
	if (NULL == fp || size < 0)
		return NULL;

	capacity = (size_t)(size < N_SENDFILE ? size : N_SENDFILE);
	sendfile = (struct http_sendfile_t*)malloc(sizeof(*sendfile) + capacity + 10 /*chunk*/ + 5 /*last chunk*/);
	if (NULL == sendfile)
	{
		fclose(fp);
		return NULL;
	}

	memset(sendfile, 0, sizeof(*sendfile));
	sendfile->fp = fp;
	sendfile->ptr = (uint8_t*)(sendfile + 1);
	sendfile->total = size;
	sendfile->capacity = capacity;
	sendfile->code = 200;
	return sendfile;
}

static void http_file_close(struct http_sendfile_t* sendfile)
{
	if (sendfile->fp)
	{
		fclose(sendfile->fp);
		sendfile->fp = NULL;
	}
	free(sendfile);
}

static void http_file_read(struct http_sendfile_t* sendfile)
{
    size_t size;
	static const char* hex = "0123456789ABCDEF";

    size = (sendfile->total - sendfile->sent) > (int64_t)sendfile->capacity ? sendfile->capacity : (size_t)(sendfile->total - sendfile->sent);

    if (1 == sendfile->session->http_transfer_encoding_flag)
	{
		sendfile->bytes = fread(sendfile->ptr + 8, 1, size, sendfile->fp);
		sendfile->sent += sendfile->bytes;

        if (sendfile->bytes > 0)
		{
			// chunk
			assert(sendfile->bytes < (1 << 24));
			sendfile->ptr[0] = (uint8_t)hex[(sendfile->bytes >> 20) & 0x0F];
			sendfile->ptr[1] = (uint8_t)hex[(sendfile->bytes >> 16) & 0x0F];
			sendfile->ptr[2] = (uint8_t)hex[(sendfile->bytes >> 12) & 0x0F];
			sendfile->ptr[3] = (uint8_t)hex[(sendfile->bytes >> 8) & 0x0F];
			sendfile->ptr[4] = (uint8_t)hex[(sendfile->bytes >> 4) & 0x0F];
			sendfile->ptr[5] = (uint8_t)hex[sendfile->bytes & 0x0F];
			sendfile->ptr[6] = '\r';
			sendfile->ptr[7] = '\n';
			sendfile->ptr[sendfile->bytes + 8] = '\r';
			sendfile->ptr[sendfile->bytes + 9] = '\n';
			sendfile->bytes += 10;
		}

        if (0 == sendfile->bytes || sendfile->sent == sendfile->total)
        {
            assert(0 != sendfile->bytes || feof(sendfile->fp));

            // last-chunk
            sendfile->ptr[sendfile->bytes + 0] = '0'; // length
            sendfile->ptr[sendfile->bytes + 1] = '\r';
            sendfile->ptr[sendfile->bytes + 2] = '\n';

            // \r\n
            sendfile->ptr[sendfile->bytes + 3] = '\r';
            sendfile->ptr[sendfile->bytes + 4] = '\n';
            sendfile->bytes += 5;
        }
	}
	else
	{
		sendfile->bytes = fread(sendfile->ptr, 1, size, sendfile->fp);
		sendfile->sent += sendfile->bytes;
	}
}

static int http_server_onsendfile(void* param, int code, size_t bytes)
{
	struct http_sendfile_t* sendfile;
	sendfile = (struct http_sendfile_t*)param;

	if (0 == code)
	{
		assert(sendfile->bytes <= bytes); // bytes: http header + content
		if (sendfile->sent == sendfile->total)
		{
			if (sendfile->onsend)
				code = sendfile->onsend(sendfile->param, 0, (size_t)sendfile->total);
			http_file_close(sendfile);
			return code;
		}
		
		http_file_read(sendfile);
		code = aio_tcp_transport_send(sendfile->session->transport, sendfile->ptr, sendfile->bytes);
	}

	if(0 != code)
	{
		if (sendfile->onsend)
			sendfile->onsend(sendfile->param, code, 0);
		http_file_close(sendfile);
		assert(1 != code);
		return code;
	}

	return 1; // HACK: has more data to send
}

static int http_session_range(struct http_sendfile_t* sendfile)
{
	int n;
	const char* prange;
	struct http_header_range_t range[3];

	prange = http_server_get_header(sendfile->session, "Range");
	if (prange)
	{
		n = http_header_range(prange, range, sizeof(range)/sizeof(range[0]));
		if (1 != n || 0 == sendfile->total)
			return -1;

		if (-1 == range[0].start)
		{
			// rewind
			if (-1 == range[0].end || 0 == range[0].end)
				return -1;

			range[0].start = (sendfile->total >= range[0].end) ? sendfile->total - range[0].end : 0;
			range[0].end = sendfile->total - 1;
		}
		else
		{
			// seek
			if (range[0].start >= sendfile->total)
				return -1;

			if (-1 != range[0].end)
			{
				// If the last-byte-pos value is absent, or if the value is greater than or equal to the current length of the entity-body,
				// last-byte-pos is taken to be equal to one less than the current length of the entity-body in bytes.
				if (range[0].end >= sendfile->total)
					range[0].end = sendfile->total - 1;
			}
			else
			{
				range[0].end = sendfile->total - 1;
			}
		}

		assert(range[0].start <= range[0].end);
		n = snprintf((char*)sendfile->ptr, sendfile->capacity, "bytes %" PRId64 "-%" PRId64 "/%" PRId64, range[0].start, range[0].end, sendfile->total);
		http_session_add_header(sendfile->session, "Content-Range", (char*)sendfile->ptr, n);

		fseek(sendfile->fp, range[0].start, SEEK_SET);
		sendfile->total = range[0].end + 1 - range[0].start;
		assert(sendfile->total > 0);
		sendfile->code = 206;

		// add Date header
		rfc822_datetime_t datetime;
		rfc822_datetime_format(time(NULL), datetime);
		http_server_set_header(sendfile->session, "Date", datetime);

		// TODO: add cache control here
	}

	return 0;
}

int http_server_sendfile(struct http_session_t* session, const char* localpath, http_server_onsend onsend, void* param)
{
	int n;
	struct http_sendfile_t* sendfile;

	sendfile = http_file_open(localpath);
	if (NULL == sendfile)
		return -ENOENT;

	sendfile->session = session;
	sendfile->onsend = onsend;
	sendfile->param = param;

	if (0 != http_session_range(sendfile))
	{
		// 416 Requested Range Not Satisfiable
		return http_server_send(sendfile->session, 416, NULL, 0, NULL, NULL);
	}

	if (0 == session->http_content_length_flag)
	{
		n = snprintf((char*)sendfile->ptr, sendfile->capacity, "%" PRId64, sendfile->total);
		http_session_add_header(session, "Content-Length", (char*)sendfile->ptr, n);
	}

	http_file_read(sendfile);

	return http_server_send(session, sendfile->code, sendfile->ptr, sendfile->bytes, http_server_onsendfile, sendfile);
}
