#include "cstringext.h"
#include "http-upload.h"

// http_header_attr_token("name = value") => "value"
static const char* http_header_attr_token(const char* source, const char* token, char sep)
{
	size_t n;
	n = strlen(token);

	if(0 != strnicmp(source, token, n))
		return NULL;

	source = skip(source+n, ' ');

	if(sep != *source)
		return NULL;

	return skip(source + 1, ' ');
}

static int http_header_attr_value(const char* source, char* value, size_t bytes)
{
	size_t i;
	
	source = skip(source, '"');
	for(i = 0; 0==bytes || i+1 < bytes; i++)
	{
		if(0==source[i] || ';'==source[i] || '"'==source[i])
			break;
		value[i] = source[i];
	}

	value[i] = 0;
	return i;
}

// Content-Type:multipart/form-data;boundary=---------------------------7d33a816d302b6
int http_get_req_upload_boundary(const char* contentType, char boundary[128])
{
	const char *pn, *pv;

	pn = skip(contentType, ' ');

	// application/x-www-form-urlencoded
	// text/plain
	// multipart/form-data
	if(0 != strnicmp(pn, "multipart/form-data;", 20))
		return -1;

	for(pn = strchr(contentType, ';'); pn; pn = strchr(pn, ';'))
	{
		pn = skip(pn+1, ' ');
		pv = http_header_attr_token(pn, "boundary", '=');
		if(!pv)
			continue;

		http_header_attr_value(pv, boundary, 0);
		return 0;
	}
	return -1;
}

static int http_get_req_upload_file(const char* contentDisposition, 
								char dispositionType[64], 
								char* field,
								char* file)
{
	// http://www.faqs.org/rfcs/rfc2183.html
	// content-disposition (must)
	// Content-Disposition: form-data; name="user"
	// name: [optional] RFC 2047

	const char *pn, *pv;

	pn = skip(contentDisposition, ' ');

	// disposition-type [must]
	http_header_attr_value(pn, dispositionType, 0);
	
	for(pn = strchr(contentDisposition, ';'); pn; pn = strchr(pn, ';'))
	{
		pn = skip(pn+1, ' ');
		pv = http_header_attr_token(pn, "name", '=');
		if(pv)
		{
			http_header_attr_value(pv, field, 0);
			continue;
		}

		pv = http_header_attr_token(pn, "filename", '=');
		if(pv)
		{
			http_header_attr_value(pv, file, 0);
			continue;
		}
	}

	return strlen(dispositionType) > 0 ? 1 : 0;
}

static int http_upload_header_parse(const char* header, char* field, char* file)
{
	const char *entity, *pv;
	char dispositionType[64];

	for(entity = header; entity; entity = strchr(entity, '\n'))
	{
		entity = skips(entity, " \r\n");
		pv = http_header_attr_token(entity, "Content-Disposition", ':');
		if(!pv)
			continue;
	
		http_get_req_upload_file(pv, dispositionType, field, file);
		assert(0 == stricmp(dispositionType, "form-data"));
		return 1;
	}
	return 0;
}

static const char* memstr(const char* s, const char* substring, size_t n)
{
	const char* pe = s + n;
	const char* p = strstr(s, substring);
	while(!p)
	{
		size_t l = strlen(s);
		s += l + 1;
		if(s >= pe)
			return NULL;
		p = strstr(s, substring);
	}
	return p;
}

int http_get_upload_data(const void* data, unsigned int size, const char* boundary, on_http_upload_data ondata, void* cbparam)
{
	// Returning Values from Forms:  multipart/form-data
	// http://www.faqs.org/rfcs/rfc2388.html

	const char *p, *pbody;
	char field[128], file[256];

	p = memstr((const char*)data, boundary, size);
	while(p)
	{
		p += strlen(boundary) + 2; // skip \r\n
		pbody = strstr(p, "\r\n\r\n");
		if(!pbody)
			break;

		// field header
		memset(file, 0, sizeof(file));
		memset(field, 0, sizeof(field));
		if(!http_upload_header_parse(p, field, file))
			return -1;

		pbody += 4;
		p = memstr(pbody, boundary, (const char*)data+size-pbody);
		if(p)
		{
			assert(p-pbody > 4);
			ondata(cbparam, field, pbody, p-4-pbody);
		}
	}

	return 0;
}
