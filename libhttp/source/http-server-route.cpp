#include "http-route.h"
#include "http-server-internal.h"
#include "sys/path.h"
#include "urlcodec.h"
#include "uri-parse.h"
#include "sha.h"
#include "base64.h"
#include "utf8codec.h"
#include "cstringext.h"
#include <algorithm>
#include <string>
#include <vector>

struct http_server_route_t
{
	typedef std::pair<std::string, http_server_handler> http_route_t;
	std::vector<http_route_t> handlers;
	http_server_onupgrade onupgrade;
	struct websocket_handler_t wsh;
	void* wsparam; // upgrade handler parameter
};

static http_server_route_t& http_server_get_route()
{
	static http_server_route_t router;
	return router;
}

static int http_server_try_upgrade(http_session_t* session);
static int http_server_upgrade_websocket(http_session_t* session, const char* path, const char* wskey, const char* protocols);

static bool http_route_cmp(const http_server_route_t::http_route_t& l, const http_server_route_t::http_route_t& r)
{
	return l.first.length() < r.first.length();
}

int http_server_addroute(const char* path, http_server_handler handler)
{
	http_server_route_t& router = http_server_get_route();
	router.handlers.push_back(std::make_pair(path, handler));
	std::sort(router.handlers.begin(), router.handlers.end(), http_route_cmp);
	return 0;
}

int http_server_delroute(const char* path)
{
	http_server_route_t& router = http_server_get_route();
	std::vector<http_server_route_t::http_route_t>::iterator it;
	for (it = router.handlers.begin(); it != router.handlers.end(); ++it)
	{
		if (it->first == path)
		{
			router.handlers.erase(it);
			return 0;
		}
	}

	return -1; // not found
}

int http_server_route(void* http, http_session_t* session, const char* method, const char* path)
{
	char reqpath[1024];//[PATH_MAX];
	struct uri_t* uri = uri_parse(path, (int)strlen(path));
	url_decode(uri->path, -1, reqpath, sizeof(reqpath));
	uri_free(uri);

	//UTF8Decode utf8(reqpath);
	// TODO: path resolve to fix /rootpath/../pathtosystem -> /pathtosystem
	//path_resolve(buffer, sizeof(buffer), utf8, CWD, strlen(CWD));
	//path_realpath(buffer, live->path);

	http_server_route_t& router = http_server_get_route();
	if (router.onupgrade && 0 == strcasecmp(method, "GET") && http_server_try_upgrade(session))
	{
		const char* wskey = http_server_get_header(session, "Sec-WebSocket-Key");
		const char* protocols = http_server_get_header(session, "Sec-WebSocket-Protocol");
		//const char* extensions = http_server_get_header(session, "Sec-WebSocket-Extensions");
		return http_server_upgrade_websocket(session, path, wskey, protocols);
	}

	std::vector<http_server_route_t::http_route_t>::iterator it;
	for (it = router.handlers.begin(); it != router.handlers.end(); ++it)
	{
		if (0 == strncmp(it->first.c_str(), reqpath, it->first.length()))
		{
			return it->second(http, session, method, reqpath);
		}
	}

	http_server_set_status_code(session, 404, NULL);
	return http_server_send(session, "", 0, NULL, NULL);
}

void http_server_websocket_sethandler(struct websocket_handler_t* wsh, http_server_onupgrade handler, void* param)
{
	http_server_route_t& router = http_server_get_route();
	memcpy(&router.wsh, wsh, sizeof(router.wsh));
	router.onupgrade = handler;
	router.wsparam = param;
}

static int http_server_onsend_upgrade(void* param, int code, size_t bytes)
{
	struct http_websocket_t* ws;
	ws = (struct http_websocket_t*)param;
	if (0 == code)
	{
		ws->session->ws = ws;
	}
	else
	{
		free(ws);
	}
	return 0;
}

static int http_server_try_upgrade(http_session_t* session)
{
	const char* upgrade = http_server_get_header(session, "Upgrade");
	if (!upgrade || 0 != strcasecmp(upgrade, "websocket"))
		return 0;

	const char* connection = http_server_get_header(session, "Connection"); // keep-alive, Upgrade
	if (!connection || !strstr(connection, "Upgrade"))
		return 0;

	const char* wskey = http_server_get_header(session, "Sec-WebSocket-Key");
	const char* version = http_server_get_header(session, "Sec-WebSocket-Version");
	//const char* protocols = http_server_get_header(session, "Sec-WebSocket-Protocol");
	//const char* extensions = http_server_get_header(session, "Sec-WebSocket-Extensions");
	return wskey && version && 0 == strcasecmp(version, "13") ? 1 : 0;
}

static int http_server_upgrade_websocket(http_session_t* session, const char* path, const char* wskey, const char* protocols)
{
	static const char* wsuuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	
	struct http_websocket_t* ws;
	ws = (struct http_websocket_t*)calloc(1, sizeof(*ws));
	if (!ws)
		return -ENOMEM;

	http_server_route_t& router = http_server_get_route();
	ws->session = session;
	memcpy(&ws->handler, &router.wsh, sizeof(ws->handler));
	ws->param = router.onupgrade(router.wsparam, ws, path, protocols);
	if (!ws->param)
	{
		http_server_set_status_code(session, 401, NULL);
		return http_server_send(session, "", 0, NULL, NULL);
	}

	uint8_t sha1[SHA1HashSize];
	char wsaccept[64] = { 0 };
	SHA1Context ctx;
	SHA1Reset(&ctx);
	SHA1Input(&ctx, (const uint8_t*)wskey, strlen(wskey));
	SHA1Input(&ctx, (const uint8_t*)wsuuid, strlen(wsuuid));
	SHA1Result(&ctx, sha1);
	base64_encode(wsaccept, sha1, sizeof(sha1));

	http_server_set_header(session, "Upgrade", "WebSocket");
	http_server_set_header(session, "Connection", "Upgrade");
	http_server_set_header(session, "Sec-WebSocket-Accept", wsaccept);
	http_server_set_status_code(session, 101, NULL);
	return http_server_send(session, "", 0, http_server_onsend_upgrade, ws);
}
