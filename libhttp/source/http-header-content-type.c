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
#include <string.h>
#include <assert.h>

#define SPECIAL_CHARS ";\r\n"

static int http_header_content_type_parameter(const char* parameters, struct http_header_content_type_t *v)
{
    size_t *len;
    const char *p1;
    const char *p = parameters;

    while(p && *p && v->parameter_count < sizeof(v->parameters)/sizeof(v->parameters[0]))
    {
        while(' ' == *p) ++p; // skip blank space
        p1 = strpbrk(p, "="SPECIAL_CHARS);
        if(p1 && '=' == *p1)
        {
            v->parameters[v->parameter_count].name = p;
            v->parameters[v->parameter_count].name_len = (size_t)(p1 - p);

            while(' ' == *++p1); // skip blank space
            v->parameters[v->parameter_count].value = p1;

            p = p1;
            p1 = strpbrk(p, SPECIAL_CHARS);
            v->parameters[v->parameter_count].value_len = p1 ? (size_t)(p1 - p) : strlen(p);
        }
        else
        {
            v->parameters[v->parameter_count].name = p;
            v->parameters[v->parameter_count].name_len = p1 ? (size_t)(p1 - p) : strlen(p);
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
        
        if(!p1 || '\r' == *p1 || '\n' == *p1 || '\0' == *p1)
            break;
        p = p1 + 1;
    }

    return 0;
}

int http_header_content_type(const char* field, struct http_header_content_type_t *v)
{
	size_t n;
	const char *p1;
	const char *p = field;

	v->parameter_count = 0;

    // media type
	p1 = strpbrk(p, "/"SPECIAL_CHARS);
    if(!p1 || '/' != *p1)
        return -1; // invalid content-type

    n = (size_t)(p1 - p); // ptrdiff_t -> size_t
    if(n + 1 > sizeof(v->media_type))
        return -1;
    memcpy(v->media_type, p, n);
    v->media_type[n] = '\0';

    // media subtype
    p = p1 + 1;
    p1 = strpbrk(p, " "SPECIAL_CHARS);
    n = p1 ? (size_t)(p1 - p) : strlen(p); // ptrdiff_t -> size_t
    if(n + 1 > sizeof(v->media_subtype))
        return -1;
    memcpy(v->media_subtype, p, n);
    v->media_subtype[n] = '\0';

    // parameters
    if(p1)
    {
		p1 += strspn(p1, " \t");
		if (';' == *p1)
			return http_header_content_type_parameter(p1+1, v);
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

	http_header_content_type("text/; ISO-8859-4; charset=ISO-8859-4", &content);
	assert(0 == strcmp("text", content.media_type));
	assert(0 == strcmp("", content.media_subtype));
	assert(2 == content.parameter_count);
	assert(0 == strncmp("ISO-8859-4", content.parameters[0].name, content.parameters[0].name_len));
}
#endif
