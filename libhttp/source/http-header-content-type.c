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
#include "ctypedef.h"
#include "cstringext.h"
#include "string-util.h"

#define SPECIAL_CHARS ";\r\n"

int http_header_content_type(const char* field, struct http_header_content_type_t *v)
{
	size_t* len;
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
		v->media_subtype[0] = '\0';
	}

	if('\r' == *p1 || '\n' == *p1 || '\0' == *p1)
		return 0;

	// parse parameters
	assert(';' == *p1);
	p = p1 + 1;
	while(p && *p && v->parameter_count < sizeof(v->parameters)/sizeof(v->parameters[0]))
	{
		while(' ' == *p) ++p; // skip blank space
		p1 = string_token(p, "="SPECIAL_CHARS);
		if('=' == *p1)
		{
			v->parameters[v->parameter_count].name = p;
			v->parameters[v->parameter_count].name_len = p1 - p;

			while(' ' == *++p1); // skip blank space
			v->parameters[v->parameter_count].value = p1;

			p = p1;
			p1 = string_token(p, SPECIAL_CHARS);
			v->parameters[v->parameter_count].value_len = p1 ? p1 - p : strlen(p);
		}
		else
		{
			v->parameters[v->parameter_count].name = p;
			v->parameters[v->parameter_count].name_len = p1 ? p1 - p : strlen(p);
			v->parameters[v->parameter_count].value = NULL;
			v->parameters[v->parameter_count].value_len = 0;
		}

		// reverse filter blank space
		p = v->parameters[v->parameter_count].name;
		len = &v->parameters[v->parameter_count].name_len;
		while(*len > 0 && ' ' == p[*len-1]) --*len; 

		p = v->parameters[v->parameter_count].value;
		len = &v->parameters[v->parameter_count].value_len;
		while(*len > 0 && ' ' == p[*len-1]) --*len;

		++v->parameter_count;

		if('\r' == *p1 || '\n' == *p1 || '\0' == *p1)
			break;
		p = p1 + 1;
	}

	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void http_header_content_type_test(void)
{
	struct http_header_content_type_t content;
	http_header_content_type("text/html; charset=ISO-8859-4", &content);
	assert(0 == strcmp("text", content.media_type));
	assert(0 == strcmp("html", content.media_subtype));
	assert(1==content.parameter_count);
	assert(0 == strncmp("charset", content.parameters[0].name, content.parameters[0].name_len) && 0 == strncmp("ISO-8859-4", content.parameters[0].value, content.parameters[0].value_len));
}
#endif
