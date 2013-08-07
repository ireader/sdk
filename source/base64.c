#include <stdio.h>
#include <stdlib.h>
#include "base64.h"

static char encode_table[64] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M',
	'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m',
	'n','o','p','q','r','s','t','u','v','w','x','y','z',
	'0','1','2','3','4','5','6','7','8','9','+','/'
};

static unsigned char decode_table[256];

char* base64_encode(IN CONST unsigned char * bufin, IN unsigned int nbytes)
{
#define ENC(c) encode_table[c]

	unsigned int i;
	unsigned int n3bytes = nbytes/3*3;
	register char *outptr = 0;
	char* bufcoded = 0;
	bufcoded = (char*)malloc((nbytes+2)/3*4+1);
	if(0 == bufcoded)
		return 0;

	outptr = bufcoded;
	for (i=0; i<n3bytes; i += 3) {
		*(outptr++) = ENC((*bufin >> 2) & 077);            /* c1 */
		*(outptr++) = ENC(((*bufin << 4) & 060) | ((bufin[1] >> 4) & 017)); /*c2*/
		*(outptr++) = ENC(((bufin[1] << 2) & 074) | ((bufin[2] >> 6) & 03));/*c3*/
		*(outptr++) = ENC(bufin[2] & 077);         /* c4 */

		bufin += 3;
	}

	if(nbytes >= n3bytes+1) {
		/* There were only 2 bytes in that last group */
		(*outptr++) = ENC((*bufin >> 2) & 077);

		if(nbytes < n3bytes+2) {
			/* There was only 1 byte in that last group */
			*(outptr++) = ENC(((*bufin << 4) & 060)); /*c2*/
			*(outptr++) = '='; /*c3*/
		}
		else{
			*(outptr++) = ENC(((*bufin << 4) & 060) | ((bufin[1] >> 4) & 017)); /*c2*/
			*(outptr++) = ENC(((bufin[1] << 2) & 074));/*c3*/
		}
		*(outptr++) = '='; /*c4*/
	} 
	
	*outptr = '\0';
	return bufcoded;
}

unsigned char* base64_decode(IN CONST char * bufcoded, IN unsigned int nbytes)
{
	/* single character decode */
#define DEC(c) decode_table[(int)c]
#define MAXVAL 63

	static int first = 1;

	unsigned int j;
	unsigned int n4bytes = nbytes/4*4;
	register CONST char *bufin = bufcoded;
	register unsigned char *bufout = NULL;
	unsigned char* bufplain = NULL;
	
	bufplain = (unsigned char*)malloc((nbytes+3)/4*3+1);
	if(0 == bufplain)
		return 0;
	bufout = bufplain;

	/* If this is the first call, initialize the mapping table.
	* This code should work even on non-ASCII machines.
	*/
	if(first) {
		first = 0;
		for(j=0; j<256; j++) decode_table[j] = 0;

		for(j=0; j<64; j++) decode_table[(int)encode_table[j]] = (unsigned char) j;
#if 0
		pr2six['A']= 0; pr2six['B']= 1; pr2six['C']= 2; pr2six['D']= 3; 
		pr2six['E']= 4; pr2six['F']= 5; pr2six['G']= 6; pr2six['H']= 7; 
		pr2six['I']= 8; pr2six['J']= 9; pr2six['K']=10; pr2six['L']=11; 
		pr2six['M']=12; pr2six['N']=13; pr2six['O']=14; pr2six['P']=15; 
		pr2six['Q']=16; pr2six['R']=17; pr2six['S']=18; pr2six['T']=19; 
		pr2six['U']=20; pr2six['V']=21; pr2six['W']=22; pr2six['X']=23; 
		pr2six['Y']=24; pr2six['Z']=25; pr2six['a']=26; pr2six['b']=27; 
		pr2six['c']=28; pr2six['d']=29; pr2six['e']=30; pr2six['f']=31; 
		pr2six['g']=32; pr2six['h']=33; pr2six['i']=34; pr2six['j']=35; 
		pr2six['k']=36; pr2six['l']=37; pr2six['m']=38; pr2six['n']=39; 
		pr2six['o']=40; pr2six['p']=41; pr2six['q']=42; pr2six['r']=43; 
		pr2six['s']=44; pr2six['t']=45; pr2six['u']=46; pr2six['v']=47; 
		pr2six['w']=48; pr2six['x']=49; pr2six['y']=50; pr2six['z']=51; 
		pr2six['0']=52; pr2six['1']=53; pr2six['2']=54; pr2six['3']=55; 
		pr2six['4']=56; pr2six['5']=57; pr2six['6']=58; pr2six['7']=59; 
		pr2six['8']=60; pr2six['9']=61; pr2six['+']=62; pr2six['/']=63;
#endif
	}

	for(j=0; j<n4bytes; j += 4){
		*(bufout++) = (unsigned char) (DEC(*bufin) << 2 | DEC(bufin[1]) >> 4);
		*(bufout++) = (unsigned char) (DEC(bufin[1]) << 4 | DEC(bufin[2]) >> 2);
		*(bufout++) = (unsigned char) (DEC(bufin[2]) << 6 | DEC(bufin[3]));
		bufin += 4;
	}

	*bufout = '\0';
	return bufplain;
}

void base64_free(void* p)
{
	free(p);
}
