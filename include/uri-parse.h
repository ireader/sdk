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

#ifdef __cplusplus
}
#endif
#endif /* !_uri_parser_h_ */
