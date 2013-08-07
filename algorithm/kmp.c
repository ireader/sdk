// Knuth-Morris-Pratt Algorithm
// http://www.ics.uci.edu/~eppstein/161/960227.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void kmp_overlap(const char* pattern, int n, int* overlap)
{
	int i, j;

	overlap[0] = 0;

	for(i=0, j=1; j<n; j++)
	{
		assert(i < n);
		if(pattern[j] == pattern[i])
		{
			overlap[j] = ++i;
		}
		else
		{
			i = 0;
			overlap[j] = 0;
		}
	}
}

static const char* kmp_match(const char* s, const char* pattern, int n1, int n2, int* overlap)
{
	int i, j;

	i = 0;
	j = 0;
	while(i < n1 && j<n2)
	{
		//assert(i+j >= 0 && i+j<n1);
		if(s[i] == pattern[j])
		{
			++j;
			++i;
		}
		else
		{
			j = j>0?overlap[j-1]:0;
			i += j>0?0:1;
		}
	}

	assert(i>=j);
	return j==n2?s+i-j:0;
}

const char* kmp(const char* s, const char* pattern)
{
	int n1, n2;
	int* overlap;
	const char* p;

	assert(pattern);
	n1 = strlen(s);
	n2 = strlen(pattern);
	overlap = (int*)malloc(sizeof(int)*(n2+1));
	if(!overlap)
		return NULL;

	kmp_overlap(pattern, n2, overlap);
	p = kmp_match(s, pattern, n1, n2, overlap);

	free(overlap);
	return p;
}
