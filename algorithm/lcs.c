// Longest Common Subsequences(not Longest Common Substring)
// 
// lcs("nematode knowledge", "empty bottle") => "emt ole";

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#define MAX(a, b) ((a)>(b)?(a):(b));
#define LEN(i, j) lcs_length(m, n1, n2, i, j)

static int lcs_length(int* m, int n1, int n2, int i, int j)
{
	return (i<0||j<0) ? 0 : m[i*n2 + j];
}

static int lcs_alogrithm(const char* s1, const char* s2, int n1, int n2, int* m)
{
	int i, j;

	for(i=0; i<n1; ++i)
	{
		for(j=0; j<n2; ++j)
		{
			if(s1[i] == s2[j])
			{
				m[i*n2 + j] = LEN(i-1, j-1)+1;
			}
			else
			{
				m[i*n2 + j] = MAX(LEN(i-1, j), LEN(i, j-1));
			}
		}
	}

	return m[n1*n2-1];
}

static void lcs_sequence(const char* s1, const char* s2, int n1, int n2, const int* m, char* seq)
{
	int i, j;
	int lcs;

	lcs = m[n1*n2-1];

	for(i=n1-1; i>=0; --i)
	{
		for(j=n2-1; j>=0; --j)
		{
			if(LEN(i, j)==lcs && LEN(i,j)>LEN(i-1, j) && LEN(i, j)>LEN(i, j-1))
			{
				assert(LEN(i, j) > LEN(i-1, j-1));
				seq[--lcs] = s1[i];
				break;
			}
		}

		if(lcs < 0)
			break;
	}
}

static void lcs_print(const char* s1, const char* s2, int n1, int n2, int* m)
{
	int i, j;

	for(i=0; i<n2; i++)
		printf("\t%c", s2[i]);
	printf("\n");

	for(i=0; i<n1; i++)
	{
		printf("%c\t", s1[i]);
		for(j=0; j<n2; j++)
		{
			printf("%d\t", LEN(i, j));
		}
		printf("\n");
	}
}

/// longest common sequence
/// @param[in] s1 string source
/// @param[in] s2 string source
/// @param[out] seq result sequence(seq can be s2, but can't be s1)
/// @param[in] len seq buffer size(bytes)
/// @return <0-error, 0-ok, >0=need more seq buffer
int lcs(const char* s1, const char* s2, char* seq, int len)
{
	int* m;
	int n1, n2;
	int lcs;

	n1 = strlen(s1);
	n2 = strlen(s2);

	m = (int*)malloc(n1*n2*sizeof(int));
	if(!m)
		return -ENOMEM;

	lcs = lcs_alogrithm(s1, s2, n1, n2, m);

	lcs_print(s1, s2, n1, n2, m);

	if(len < lcs)
	{
		free(m);
		return lcs;
	}
	seq[lcs] = 0;
	lcs_sequence(s1, s2, n1, n2, m, seq);

	free(m);
	return 0;
}
