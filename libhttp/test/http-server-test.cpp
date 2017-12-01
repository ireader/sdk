#include "aio-socket.h"
#include "aio-worker.h"
#include "http-server.h"
#include "http-route.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/path.h"
#include <assert.h>

static int http_server_ondownload(void* /*http*/, http_session_t* session, const char* /*method*/, const char* path)
{
	char header[1024];

	path += 10; // map "/download/*" to current path
	if (path_testfile(path))
	{
		// add your MIME
		//http_server_set_content_type(session, "text/html");

		// choose transfer encoding
		//http_server_set_header(session, "Transfer-Encoding", "chunked");

		// map server filename to download filename
		const char* name = path_basename(path);
		int n = snprintf(header, sizeof(header), "attachment; filename=\"%s\"", name);
		assert(n > 0 && n < sizeof(header));
		http_server_set_header(session, "Content-Disposition", name);

		return http_server_sendfile(session, path, NULL, NULL);
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

extern "C" void http_server_test(const char* ip, int port)
{
	size_t num = 0;

	num = system_getcpucount() * 2;
	aio_worker_init(num + 1);

	http_server_t* http = http_server_create(ip, port);
	http_server_set_handler(http, http_server_route, http);
	http_server_addroute("/download/", http_server_ondownload);

	// timeout process
	while (aio_socket_process(10000) >= 0)
	{
	}

	http_server_destroy(http);
	aio_worker_clean(num + 1);
}
