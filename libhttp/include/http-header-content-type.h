#ifndef _http_header_content_type_h_
#define _http_header_content_type_h_

struct http_header_media_parameter
{
	const char* name;
	const char* value;
};

struct http_header_content_type_t
{
	char media_type[16];
	char media_subtype[16];

	int parameter_count;
	struct http_header_media_parameter* parameters;
};

/// WARNING: content-type value will be changed!!!
/// usage: 
///		struct http_header_content_type_t content;
///		const char* header = "Content-Type: text/html; charset=ISO-8859-4";
///		char* field = strdup("text/html; charset=ISO-8859-4");
///		r = http_header_content_type(field, &content);
///		check(r) and free(filed)
int http_header_content_type(char* field, struct http_header_content_type_t* content);

#endif /* !_http_header_content_type_h_ */
