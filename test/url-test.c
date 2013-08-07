#include "url.h"
#include <assert.h>
#include <string.h>

static void url_standard()
{
	void *uri;
	const char *name, *value;

	uri = url_parse("http://www.microsoft.com");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("http", url_getscheme(uri)));
	assert(0==strcmp("/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	url_free(uri);

	uri = url_parse("http://www.microsoft.com:80");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("http", url_getscheme(uri)));
	assert(0==strcmp("/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	assert(80==url_getport(uri));
	url_free(uri);

	uri = url_parse("http://www.microsoft.com:80/");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("http", url_getscheme(uri)));
	assert(0==strcmp("/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	assert(80==url_getport(uri));
	url_free(uri);

	// with path
	uri = url_parse("http://www.microsoft.com/china/");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("http", url_getscheme(uri)));
	assert(0==strcmp("/china/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	url_free(uri);

	uri = url_parse("http://www.microsoft.com/china/default.html");
	assert(0==strcmp("http", url_getscheme(uri)));
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	url_free(uri);

	// with param
	uri = url_parse("http://www.microsoft.com/china/default.html?encoding=utf-8");
	assert(0==strcmp("http", url_getscheme(uri)));
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(1==url_getparam_count(uri));
	assert(0==url_getparam(uri, 0, &name, &value));
	assert(0==strcmp("encoding", name)&&0==strcmp("utf-8", value));
	url_free(uri);

	uri = url_parse("http://www.microsoft.com/china/default.html?encoding=utf-8&font=small");
	assert(0==strcmp("http", url_getscheme(uri)));
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(2==url_getparam_count(uri));
	assert(0==url_getparam(uri, 0, &name, &value));
	assert(0==strcmp("encoding", name)&&0==strcmp("utf-8", value));
	assert(0==url_getparam(uri, 1, &name, &value));
	assert(0==strcmp("font", name)&&0==strcmp("small", value));
	url_free(uri);
}

static void url_without_scheme()
{
	void *uri;
	const char *name, *value;

	uri = url_parse("www.microsoft.com");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	url_free(uri);

	uri = url_parse("www.microsoft.com:80");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	assert(80==url_getport(uri));
	url_free(uri);

	uri = url_parse("www.microsoft.com:80/");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	assert(80==url_getport(uri));
	url_free(uri);

	// with path
	uri = url_parse("www.microsoft.com/china/");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/china/", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	url_free(uri);

	uri = url_parse("www.microsoft.com/china/default.html");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	url_free(uri);

	// with param
	uri = url_parse("www.microsoft.com/china/default.html?encoding=utf-8");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(1==url_getparam_count(uri));
	assert(0==url_getparam(uri, 0, &name, &value));
	assert(0==strcmp("encoding", name)&&0==strcmp("utf-8", value));
	url_free(uri);

	uri = url_parse("www.microsoft.com/china/default.html?encoding=utf-8&font=small");
	assert(0==strcmp("www.microsoft.com", url_gethost(uri)));
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(2==url_getparam_count(uri));
	assert(0==url_getparam(uri, 0, &name, &value));
	assert(0==strcmp("encoding", name)&&0==strcmp("utf-8", value));
	assert(0==url_getparam(uri, 1, &name, &value));
	assert(0==strcmp("font", name)&&0==strcmp("small", value));
	url_free(uri);
}

static void url_without_host()
{
	void *uri;
	const char *name, *value;

	uri = url_parse("/china/default.html");
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(0==url_getparam_count(uri));
	url_free(uri);

	uri = url_parse("/china/default.html?encoding=utf-8&font=small");
	assert(0==strcmp("/china/default.html", url_getpath(uri)));
	assert(2==url_getparam_count(uri));
	assert(0==url_getparam(uri, 0, &name, &value));
	assert(0==strcmp("encoding", name)&&0==strcmp("utf-8", value));
	assert(0==url_getparam(uri, 1, &name, &value));
	assert(0==strcmp("font", name)&&0==strcmp("small", value));
	url_free(uri);
}

void url_test()
{
	void *uri;
	char *name, *value;
	uri = url_parse("");
	assert(0==strcmp("", url_gethost(uri)));
	assert(0==strcmp("/", url_getpath(uri)));
	url_free(uri);

	uri = url_parse("/");
	assert(0 == url_gethost(uri));
	assert(0==strcmp("/", url_getpath(uri)));
	url_free(uri);

	url_standard();
	url_without_scheme();
	url_without_host();
}
