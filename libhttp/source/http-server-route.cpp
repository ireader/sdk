#include "http-route.h"
#include "sys/path.h"
#include "urlcodec.h"
#include "uri-parse.h"
#include <algorithm>
#include <string>
#include <vector>

typedef std::pair<std::string, http_server_handler> http_route_t;
static std::vector<http_route_t> s_handler;

static bool http_route_cmp(const http_route_t& l, const http_route_t& r)
{
	return l.first.length() < r.first.length();
}

int http_server_addroute(const char* path, http_server_handler handler)
{
	s_handler.push_back(std::make_pair(path, handler));
	std::sort(s_handler.begin(), s_handler.end(), http_route_cmp);
	return 0;
}

int http_server_delroute(const char* path)
{
	std::vector<http_route_t>::iterator it;
	for (it = s_handler.begin(); it != s_handler.end(); ++it)
	{
		if (it->first == path)
		{
			s_handler.erase(it);
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

	std::vector<http_route_t>::iterator it;
	for (it = s_handler.begin(); it != s_handler.end(); ++it)
	{
		if (0 == strncmp(it->first.c_str(), reqpath, it->first.length()))
		{
			return it->second(http, session, method, reqpath);
		}
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}
