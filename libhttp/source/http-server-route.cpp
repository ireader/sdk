#include "http-route.h"
#include "http-server-internal.h"
#include "sys/path.h"
#include "urlcodec.h"
#include "uri-parse.h"
#include "sha.h"
#include "base64.h"
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
static http_server_route_t s_router;

static int http_server_upgrade_websocket(http_session_t* session, const char* path, const char* wskey, const char* protocols);

static bool http_route_cmp(const http_server_route_t::http_route_t& l, const http_server_route_t::http_route_t& r)
{
	return l.first.length() < r.first.length();
}

int http_server_addroute(const char* path, http_server_handler handler)
{
	s_router.handlers.push_back(std::make_pair(path, handler));
	std::sort(s_router.handlers.begin(), s_router.handlers.end(), http_route_cmp);
	return 0;
}

int http_server_delroute(const char* path)
{
	std::vector<http_server_route_t::http_route_t>::iterator it;
	for (it = s_router.handlers.begin(); it != s_router.handlers.end(); ++it)
	{
		if (it->first == path)
		{
			s_router.handlers.erase(it);
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

	// TODO: path resolve to fix /rootpath/../pathtosystem -> /pathtosystem

	if (s_router.onupgrade && 0 == strcasecmp(method, "GET"))
	{
		const char* upgrade = http_server_get_header(session, "Upgrade");
		const char* connection = http_server_get_header(session, "Connection"); // keep-alive, Upgrade
		const char* wskey = http_server_get_header(session, "Sec-WebSocket-Key");
		const char* version = http_server_get_header(session, "Sec-WebSocket-Version");
		const char* protocols = http_server_get_header(session, "Sec-WebSocket-Protocol");
		//const char* extensions = http_server_get_header(session, "Sec-WebSocket-Extensions");

		if (upgrade && 0 == strcasecmp(upgrade, "websocket")
			&& connection && strstr(connection, "Upgrade")
			&& wskey && version && 0 == strcasecmp(version, "13"))
		{
			return http_server_upgrade_websocket(session, path, wskey, protocols);
		}
	}

	std::vector<http_server_route_t::http_route_t>::iterator it;
	for (it = s_router.handlers.begin(); it != s_router.handlers.end(); ++it)
	{
		if (0 == strncmp(it->first.c_str(), reqpath, it->first.length()))
		{
			return it->second(http, session, method, reqpath);
		}
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

void http_server_websocket_sethandler(struct websocket_handler_t* wsh, http_server_onupgrade handler, void* param)
{
	memcpy(&s_router.wsh, wsh, sizeof(s_router.wsh));
	s_router.onupgrade = handler;
	s_router.wsparam = param;
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

static int http_server_upgrade_websocket(http_session_t* session, const char* path, const char* wskey, const char* protocols)
{
	static const char* wsuuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	
	struct http_websocket_t* ws;
	ws = (struct http_websocket_t*)calloc(1, sizeof(*ws));
	if (!ws)
		return -ENOMEM;

	ws->session = session;
	memcpy(&ws->handler, &s_router.wsh, sizeof(ws->handler));
	ws->param = s_router.onupgrade(s_router.wsparam, ws, path, protocols);
	if(!ws->param)
		return http_server_send(session, 401, "", 0, NULL, NULL);

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
	return http_server_send(session, 101, "", 0, http_server_onsend_upgrade, ws);
}
