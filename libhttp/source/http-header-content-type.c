// rfc 2616 HTTP v1.1
// 14.17 Content-Type (entity-header field) (p77)
// Content-Type = "Content-Type" ":" media-type
//
// media-type = type "/" subtype *( ";" parameter )
// type = token
// subtype = token
// 
// Content-Type: text/html; charset=ISO-8859-4
//
// 7.2.1 Type (p29)
// Default: the recipient SHOULD treat it as type "application/octetstream"

#include "http-header-content-type.h"
#include "cstringext.h"
#include "string-util.h"

#define SPECIAL_CHARS ";\r\n"

int http_header_content_type(char* field, struct http_header_content_type_t *v)
{
	const char* p1;
	const char* p = field;

	v->parameter_count = 0;

	// parse media type
	p1 = string_token(p, "/"SPECIAL_CHARS);
	if('/' == *p1)
	{
		assert(p1 - p < sizeof(v->media_type)-1);
		strncpy(v->media_type, p, p1-p);
		v->media_type[p1-p] = '\0';

		p = p1 + 1;
		p1 = string_token(p, SPECIAL_CHARS);
		assert(p1 - p < sizeof(v->media_subtype)-1);
		strncpy(v->media_subtype, p, p1-p);
		v->media_subtype[p1-p] = '\0';
	}
	else
	{
		assert(p1 - p < sizeof(v->media_type)-1);
		strncpy(v->media_type, p, p1-p);
		v->media_type[p1-p] = '\0';
	}

	if('\r' == *p1 || '\n' == *p1 || '\0' == *p1)
		return 0;

	// parse parameters
	assert(';' == *p1);
	p = p1 + 1;
	while(p && *p)
	{
		char c;
		p1 = string_token(p, "="SPECIAL_CHARS);
		if('=' == *p1)
		{
			*(char*)p1 = 0;
			v->parameters[v->parameter_count].name = p;
			v->parameters[v->parameter_count].value = p1+1;

			p = p1 + 1;
			p1 = string_token(p, SPECIAL_CHARS);
		}
		else
		{
			v->parameters[v->parameter_count].name = p;
			v->parameters[v->parameter_count].value = NULL;
		}

		c = *p1;
		*(char*)p1 = 0; // Warning: change input args value
		++v->parameter_count;

		if('\r' == *p1 || '\n' == *p1 || '\0' == *p1)
			break;
		p = p1 + 1;
	}

	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void http_header_content_type_test()
{
	struct http_header_media_parameter parameters[16];
	struct http_header_content_type_t content;
	char contentType[32];
	content.parameters = parameters;
	strcpy(contentType, "text/html;charset=ISO-8859-4");
	http_header_content_type(contentType, &content);
	assert(0 == strcmp("text", content.media_type));
	assert(0 == strcmp("html", content.media_subtype));
	assert(1==content.parameter_count);
	assert(0 == strcmp("charset", content.parameters[0].name) && 0 == strcmp("ISO-8859-4", content.parameters[0].value));
}
#endif
