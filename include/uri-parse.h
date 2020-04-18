#ifndef _uri_parser_h_
#define _uri_parser_h_

#ifdef __cplusplus
extern "C" {
#endif

struct uri_t
{
	char* scheme;

	char* userinfo;
	char* host;
	int port;

	char* path; // encoded/undecode uri path string, default "/"
	char* query; // encoded/undecode uri query string, NULL if no value
	char* fragment; // encoded/undecode uri fragment string, NULL if no value
};

/// URI parser
/// e.g: 1. http://usr:pwd@host:port/path?query#fragment
///      2. usr:pwd@host:port/path?query#fragment
///      3. /path?query#fragment
/// @param[in] uri Uniform Resource Identifier
/// @param[in] len uri length
/// @return NULL if parse failed, other-uri_t pointer, free by uri_free
struct uri_t* uri_parse(const char* uri, int len);

/// @param[in] uri return by uri_parse
void uri_free(struct uri_t* uri);

/// uri path with query + fragment
/// e.g. http://host:port/path?query#fragment --> path?query#fragment
/// @return uri path length
int uri_path(const struct uri_t* uri, char* buf, int len);

int uri_userinfo(const struct uri_t* uri, char* usr, int n1, char* pwd, int n2);

struct uri_query_t
{
	const char* name;
	int n_name;

	const char* value;
	int n_value;
};

int uri_query(const char* query, const char* end, struct uri_query_t** items);

void uri_query_free(struct uri_query_t** items);

#ifdef __cplusplus
}
#endif
#endif /* !_uri_parser_h_ */
