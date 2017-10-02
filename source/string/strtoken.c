#include <string.h>

char *strtoken(char *s, const char *delim, char **saveptr)
{
	char *tok;

	if (!s && !(s = *saveptr))
		return NULL;

	/* skip leading delimiters */
	s += strspn(s, delim);

	/* s now points to the first non delimiter char, or to the end of the string */
	if (!*s) {
		*saveptr = NULL;
		return NULL;
	}
	tok = s++;

	/* skip non delimiters */
	s += strcspn(s, delim);
	if (*s) {
		*s = 0;
		*saveptr = s + 1;
	}
	else {
		*saveptr = NULL;
	}

	return tok;
}
