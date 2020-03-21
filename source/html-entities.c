#include "html-entities.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// HTML Entities
// Some characters are reserved in HTML.
// http://www.w3schools.com/tags/ref_entities.asp
// &entity_name;
// OR
// &#entity_number; 

typedef struct _html_entities
{
	char name[10];
	wchar_t number; // Entity Number
} html_entities;

#define html_entity(entity, code) { entity, code }

static html_entities s_entities[] = {
	// HTML and XHTML processors must support the five special characters listed in the table below:
	html_entity("quot",	34), // quotation mark(")
	html_entity("apos",	39), // apostrophe(')
	html_entity("amp",	38), // ampersand(&)
	html_entity("lt",	60), // less-than(<)
	html_entity("gt",	62), // greater-than(>)

	// ISO 8859-1 Symbols
	html_entity("nbsp",	160), // non-breaking space( )
	html_entity("pound",163), // pound(£)
	html_entity("yen",	165), // yen(¥)
	html_entity("sect",	167), // section(§)
	html_entity("copy",	169), // copyright(©)
	html_entity("reg",	174), // registered trademark(®)
	html_entity("euro",	8364), // euro(€)
};

int html_entities_count(void)
{
	return sizeof(s_entities)/sizeof(s_entities[0]);
}

void html_entities_get(int index, char name[16], wchar_t *number)
{
	if(index >= 0 && index < sizeof(s_entities)/sizeof(s_entities[0]))
	{
		if(number)
			*number = s_entities[index].number;
		if(name)
			sprintf(name, "&%s;", s_entities[index].name);
	}
}

static int html_entities_find_by_name(const char* name)
{
	int i;
	for(i = 0; i < sizeof(s_entities)/sizeof(s_entities[0]); i++)
	{
		if(0==strncmp(s_entities[i].name, name, strlen(s_entities[i].name)) 
			&& ';'==name[strlen(s_entities[i].name)])
			return i;
	}
	return -1;
}

static int html_entities_find_by_number(wchar_t number)
{
	int i;
	for(i = 0; i < sizeof(s_entities)/sizeof(s_entities[0]); i++)
	{
		if(s_entities[i].number == number)
			return i;
	}
	return -1;
}

static int html_entities_value(char* p, wchar_t number)
{
	if(number <= 0x7F)
	{
		*p = (char)number;
		return 1;
	}
	else if(number <= 0x7FF)
	{
		*p++ = (char)(0xc0 | ((number>>6)&0x1F));
		*p++ = (char)(0x80 | (number&0x3F));
		return 2;
	}
	else if(number <= 0xFFFF)
	{
		*p++ = (char)(0xe0 | ((number>>12)&0x0F));
		*p++ = (char)(0x80 | ((number>>6)&0x3F));
		*p++ = (char)(0x80 | (number&0x3F));
		return 3;
	}
	else if(number <= 0x10FFFF)
	{
		*p++ = (char)(0xF0 | (((int)number>>18)&0x07));
		*p++ = (char)(0x80 | ((number>>12)&0x3F));
		*p++ = (char)(0x80 | ((number>>6)&0x3F));
		*p++ = (char)(0x80 | (number&0x3F));
		return 4;
	}

	return 0;
}

static const char* html_entities_numeric(const char *p, wchar_t *number)
{
	long code_l;
	int hexadecimal = (*p == 'x' || *p == 'X'); /* TODO: XML apparently disallows "X" */
	char *endptr;

	if (hexadecimal && (*p != '\0'))
		p++;
			
	/* strtol allows whitespace and other stuff in the beginning
		* we're not interested */
	if ((hexadecimal && !isxdigit(*p)) ||
			(!hexadecimal && !isdigit(*p))) {
		return NULL;
	}

	code_l = strtol(p, &endptr, hexadecimal ? 16 : 10);
	/* we're guaranteed there were valid digits, so *endptr > buf */

	if (*endptr != ';')
		return NULL;

	/* many more are invalid, but that depends on whether it's HTML
	 * (and which version) or XML. */
	if (code_l > 0x10FFFFL)
		return NULL;

	if (number != NULL)
		*number = (wchar_t)code_l;

	return endptr+1;
}

int html_entities_decode(char* dst, const char* src, int srcLen)
{
	int i, j;
	wchar_t n;
	const char* p, *p0;

	j = 0;
	for(p = src; *p && p < src + srcLen; )
	{
		if('&' != p[0])
		{
			dst[j++] = *p++;
			continue;
		}

		n = 0;
		if('#' == p[1])
		{
			p0 = html_entities_numeric(p+2, &n);
			if(p0)
			{
				j += html_entities_value(dst+j, n);
				p = p0;
				continue;
			}
		}
		else
		{
			i = html_entities_find_by_name(p+1);
			if(i >= 0)
			{
				n = s_entities[i].number;
				p += strlen(s_entities[i].name) + 2; // with '&' and ';'
				j += html_entities_value(dst+j, n);
				continue;
			}
		}

		dst[j++] = *src++; // don't decode '&'
	}

	dst[j] = '\0';
	return j;
}

static const unsigned char* get_next_char(const unsigned char* s, wchar_t* w)
{
	if(0xF0 == (0xF0 & s[0]))
	{
		*w = ((((wchar_t)s[0])&0x07) << 18)
			| ((((wchar_t)s[1])&0x3F) << 12)
			| ((((wchar_t)s[2])&0x3F) << 6)
			| (((wchar_t)s[3])&0x3F);
		return s + 4;
	}
	else if(0xE0 == (0xE0 & s[0]))
	{
		*w = ((((wchar_t)s[0])&0x0F) << 12)
			| ((((wchar_t)s[1])&0x3F) << 6)
			| (((wchar_t)s[2])&0x3F);
		return s + 3;
	}
	else if(0xC0 == (0xC0 & s[0]))
	{
		*w = ((((wchar_t)s[0])&0x1F) << 6)
			| (((wchar_t)s[1])&0x3F);
		return s + 2;
	}
	else
	{
		*w = ((wchar_t)s[0]) & 0xFF;
		return s + 1;
	}
}

int html_entities_encode(char* dst, const char* src, int srcLen)
{
	int i, j;
	wchar_t n;
	const unsigned char* p, *p2;

	j = 0;
	p = (const unsigned char*)src;
	while(p < (const unsigned char*)src+srcLen && *p)
	{
		p2 = get_next_char(p, &n);

		i = html_entities_find_by_number(n);
		if(i >= 0)
		{
			sprintf(dst+j, "&%s;", s_entities[i].name);
			j += strlen(s_entities[i].name) + 2;
			p = p2;
		}
		else
		{
			while(p < p2)
				dst[j++] = *p++;
		}
	}

	dst[j] = 0;
	return j;
}
