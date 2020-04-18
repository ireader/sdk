#include "tools.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int tools_write(const char* filename, const void* ptr, int len)
{
	size_t n = 0;
	FILE* fp = fopen(filename, "wb");
	if(!fp) 
		return -(int)errno;

	n = fwrite(ptr, 1, len, fp);	
	fclose(fp);
    return len == n ? len : -(int)errno;
}

int tools_append(const char* filename, const void* ptr, int len)
{
	size_t n = 0;
	FILE* fp = fopen(filename, "wb+");
	if(!fp) 
		return -(int)errno;

	fseek(fp, 0, SEEK_END);
	n = fwrite(ptr, 1, len, fp);
	fclose(fp);
    return len == n ? len : -(int)errno;
}

int tools_cat(const char* filename, char* buf, int len)
{
	size_t n = 0;
	FILE* fp = fopen(filename, "r");
	if(!fp) 
		return -(int)errno;

	n = fread(buf, 1, len, fp);	
	fclose(fp);
    return len == n ? len : -(int)errno;
}

int tools_cat_binary(const char* filename, char* buf, int len)
{
	size_t n = 0;
	FILE* fp = fopen(filename, "rb");
	if(!fp) 
		return -(int)errno;

	n = fread(buf, 1, len, fp);	
	fclose(fp);
    return len == n ? len : -(int)errno;
}

// tools_grep("abc\ndef\ncf", "c") => "abc\ncf"
int tools_grep(const char* in, const char* pattern, char* buf, int len)
{
	size_t n = 0;
	const char *l;
	const char *r = in;
	while(r && *r)
	{
		r = strstr(r, pattern);
		if(r)
		{
			// backward find (left)
			l = r;
			while(l >= in)
			{
				if('\n' == *l)
					break;
				--l;
			}
			l = l>=in?l+1:in; // begin

			// forward find (right)
			r = strchr(r+1, '\n');
			if(r)
			{
				if(r+1-l > len-n)
					break;

				strncpy(buf+n, l, r+1-l);
				n = r+1-in;
				buf[n] = 0;
			}
			else
			{
				if(in+strlen(in)-l > len-n)
					break;

				strcpy(buf+n, l);
			}
		}
	}
	return (int)strlen(buf);
}

int tools_tokenline(const char* str, tools_tokenline_fcb fcb, ...)
{
	int n, ret;
	const char *l, *r;

	va_list val;

	l = str;
	do
	{
		r = strchr(l, '\n');
		if(r)
			n = (int)(r+1-l);
		else
			n = (int)strlen(l);

		va_start(val, fcb);
		ret = fcb(l, n, val);
		va_end(val);

		if(ret)
			return ret;

		l = r+1;
	} while(r);

	return 0;
}
