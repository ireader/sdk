#include "html-entities.h"
#include <stdio.h>
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
	wchar_t name[10];
	wchar_t number; // Entity Number
} html_entities;

static html_entities s_entities[] = {
	// HTML and XHTML processors must support the five special characters listed in the table below:
	{ L"quot",	34 }, // quotation mark(")
	{ L"apos",	39 }, // apostrophe(')
	{ L"amp",	38 }, // ampersand(&)
	{ L"lt",	60 }, // less-than(<)
	{ L"gt",	62 }, // greater-than(>)

	// ISO 8859-1 Symbols
	{ L"nbsp",	160 }, // non-breaking space( )
	{ L"pound",	163 }, // pound(£)
	{ L"yen",	165 }, // yen(¥)
	{ L"sect",	167 }, // section(§)
	{ L"copy",	169 }, // copyright(©)
	{ L"reg",	174 }, // registered trademark(®)
	{ L"euro",	8364 }, // euro(€)
};

int html_entities_count()
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

static int html_entities_find_by_name(const wchar_t* name)
{
	int i;
	for(i = 0; i < sizeof(s_entities)/sizeof(s_entities[0]); i++)
	{
		if(0==wcsncmp(s_entities[i].name, name, wcslen(s_entities[i].name)) 
			&& ';'==name[wcslen(s_entities[i].name)])
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

static int html_entities_numeric(const char **p, wchar_t *number)
{
	long code_l;
	int hexadecimal = (**p == 'x' || **p == 'X'); /* TODO: XML apparently disallows "X" */
	char *endptr;

	if (hexadecimal && (**p != '\0'))
		(*p)++;
			
	/* strtol allows whitespace and other stuff in the beginning
		* we're not interested */
	if ((hexadecimal && !isxdigit(**p)) ||
			(!hexadecimal && !isdigit(**p))) {
		return -1;
	}

	code_l = strtol(*p, &endptr, hexadecimal ? 16 : 10);
	/* we're guaranteed there were valid digits, so *endptr > buf */
	*p = endptr;

	if (*endptr != ';')
		return -1;

	/* many more are invalid, but that depends on whether it's HTML
	 * (and which version) or XML. */
	if (code_l > 0x10FFFFL)
		return -1;

	if (number != NULL)
		*number = (wchar_t)code_l;

	return 0;
}

int html_entities_decode(wchar_t* dst, const wchar_t* src, int srcLen)
{
	int j;
	wchar_t n;
	const wchar_t* p;

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
			// TODO:
			//if(0 == html_entities_numeric(&p, &n))
			//	src = p;
		}
		else
		{
			n = html_entities_find_by_name(p+1);
			if(n >= 0)
			{
				n = s_entities[n].number;
				p += wcslen(s_entities[n].name) + 2; // with '&' and ';'
			}
		}

		if(n > 0)
		{
			dst[j++] = n;
		}
		else
		{
			dst[j++] = *src++; // don't decode '&'
		}
	}

	dst[j] = '\0';
	return j;
}

int html_entities_encode(wchar_t* dst, const wchar_t* src, int srcLen)
{
	int i, j;
	const wchar_t *p;

	j = 0;
	for(p = src; p < src+srcLen && *p; p++)
	{
		i = html_entities_find_by_number(*p);
		if(i >= 0)
		{
			wsprintf(dst+j, L"&%s;", s_entities[i].name);
			j += wcslen(s_entities[i].name) + 2;
		}
		else
		{
			dst[j++] = *p;
		}
	}

	dst[j] = 0;
	return j;
}
