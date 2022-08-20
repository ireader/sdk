#include "http-server.h"
#include "http-route.h"
#include "sys/path.h"
#include "urlcodec.h"
#include "utf8codec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>

extern "C" int dir_list(const char* path, int (onlist)(void* param, const char* name, int isdir), void* param);

static int http_onlist(void* param, const char* name, int dir)
{
	char link[1024];
	char item[1024];
	std::string* reply = (std::string*)param;

	UTF8Encode utf8(name);
	url_encode((const char*)utf8, -1, link, sizeof(link));

	snprintf(item, sizeof(item), "<li><a href = \"%s%s\">%s</a></li>", link, dir ? "/" : "", (const char*)utf8);
	*reply += item;
	return 0;
}

extern "C" int http_list_dir(http_session_t* session, const char* path)
{
	std::string reply;
	reply = "<html><head><title></title><meta content=\"text/html; charset=utf-8\" http-equiv=\"content-type\"/></head><body>";
	reply += "<ul>";
	dir_list(path, http_onlist, &reply);
	reply += "</ul>";
	reply += "</body></html>";

	http_server_set_header(session, "content-type", "text/html");
	http_server_set_header(session, "Access-Control-Allow-Origin", "*");
	http_server_set_header(session, "Access-Control-Allow-Headers", "*");
	http_server_set_header(session, "Access-Control-Allow-Credentials", "true");
	http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS, CONNECT");
	http_server_reply(session, 200, reply.c_str(), reply.length());
	
	printf("load path: %s\n", path);
	return 0;
}
