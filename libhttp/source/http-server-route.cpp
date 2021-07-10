#include <string>
#include <vector>
#include <algorithm>
#include "http-route.h"
#include "http-server-internal.h"
#include "urlcodec.h"

struct http_server_route_t
{
	typedef std::pair<std::string, http_server_handler> http_route_t;
	std::vector<http_route_t> handlers;
};

static http_server_route_t& http_server_get_route()
{
	static http_server_route_t router;
	return router;
}

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
	//struct uri_t* uri = uri_parse(path, (int)strlen(path));
	url_decode(path, -1, reqpath, sizeof(reqpath));
	//uri_free(uri);

	//UTF8Decode utf8(reqpath);
	// TODO: path resolve to fix /rootpath/../pathtosystem -> /pathtosystem
	//path_resolve(buffer, sizeof(buffer), utf8, CWD, strlen(CWD));
	//path_realpath(buffer, live->path);

	http_server_route_t& router = http_server_get_route();
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
