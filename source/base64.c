#include "base64.h"
#include <assert.h>

static char encode_table[64] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M',
	'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m',
	'n','o','p','q','r','s','t','u','v','w','x','y','z',
	'0','1','2','3','4','5','6','7','8','9','+','/'
};

static unsigned char decode_table[256];

size_t base64_encode(char* target, const void *source, size_t bytes)
{
#define ENC(c) encode_table[c]

	size_t i, j=0;
	size_t n3bytes = bytes/3*3;
    const unsigned char *ptr = (const unsigned char*)source;

    for (i = 0; i < n3bytes; i += 3) {
        target[j++] = ENC((ptr[i] >> 2) & 077);            /* c1 */
        target[j++] = ENC(((ptr[i] << 4) & 060) | ((ptr[i+1] >> 4) & 017)); /*c2*/
        target[j++] = ENC(((ptr[i+1] << 2) & 074) | ((ptr[i+2] >> 6) & 03));/*c3*/
        target[j++] = ENC(ptr[i+2] & 077);         /* c4 */

        // add '\n' per 76 bytes
        // 54 = 76 / 4 * 3
        //if(i > 0 && 0 == (i % 57)) target[j++] = '\n'; // RFC-2045 p25
	}

	if(bytes >= n3bytes+1) {
		/* There were only 2 bytes in that last group */
		target[j++] = ENC((ptr[i] >> 2) & 077);

		if(bytes < n3bytes+2) {
			/* There was only 1 byte in that last group */
			target[j++] = ENC(((ptr[i] << 4) & 060)); /*c2*/
			target[j++] = '='; /*c3*/
		}
		else{
			target[j++] = ENC(((ptr[i] << 4) & 060) | ((ptr[i+1] >> 4) & 017)); /*c2*/
			target[j++] = ENC(((ptr[i+1] << 2) & 074));/*c3*/
		}

        target[j++] = '='; /*c4*/
	} 

	return j;
}

size_t base64_decode(void* target, const char *source, size_t bytes)
{
	/* single character decode */
#define DEC(c) decode_table[(int)c]
#define MAXVAL 63

	static int first = 1;

	size_t i, j;
	register const unsigned char *bufin = (const unsigned char*)source;
	register unsigned char *bufout = (unsigned char*)target;

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

	assert(0 == bytes % 4);

    i = j = 0;
	for(j=0; j+4 < bytes; j += 4){
		bufout[i++] = (DEC(bufin[j+0]) << 2) | (DEC(bufin[j+1]) >> 4);
		bufout[i++] = (DEC(bufin[j+1]) << 4) | (DEC(bufin[j+2]) >> 2);
		bufout[i++] = (DEC(bufin[j+2]) << 6) | DEC(bufin[j+3]);

        //if(0 == (i % 57))
        //{
        //    assert('\n' == bufin[j+4] || '\r' == bufin[j+4]);
        //    if('\n' == bufin[j+4] || '\r' == bufin[j+4])
        //        ++j;
        //    if('\n' == bufin[j+4] || '\r' == bufin[j+4])
        //        ++j;
        //}
	}

	// save memory(decode target buffer size = encode source buffer size)
	if(j < bytes)
	{
		assert(j+4 == bytes); bufout[i++] = (DEC(bufin[j+0]) << 2) | (DEC(bufin[j+1]) >> 4);
		if('=' != bufin[j+2]) bufout[i++] = (DEC(bufin[j+1]) << 4) | (DEC(bufin[j+2]) >> 2);
		if('=' != bufin[j+3]) bufout[i++] = (DEC(bufin[j+2]) << 6) | DEC(bufin[j+3]);
	}
	return i;
}

#if defined(DEBUG) || defined(_DEBUG)
#include <string.h>
void base64_test(void)
{
	const char* s;
	const char* r;
	char source[512];
	char target[512];

	assert(8 == base64_encode(source, "4444", 4)); // NDQ0NA==
	assert(4 == base64_decode(target, source, 8));
	assert(0 == memcmp(target, "4444", 4));

	assert(8 == base64_encode(source, "55555", 5)); // NTU1NTU=
	assert(5 == base64_decode(target, source, 8));
	assert(0 == memcmp(target, "55555", 5));

	assert(8 == base64_encode(source, "666666", 6)); // NjY2NjY2
	assert(6 == base64_decode(target, source, 8));
	assert(0 == memcmp(target, "666666", 6));

	// RFC2617 2.Basic Authentication Scheme (p6)
	s = "Aladdin:open sesame";
	r = "QWxhZGRpbjpvcGVuIHNlc2FtZQ==";
	assert(strlen(r) == base64_encode(source, s, strlen(s)));
	assert(strlen(s) == base64_decode(target, source, strlen(r)));
	assert(0 == memcmp(source, r, strlen(r)));
	assert(0 == memcmp(target, s, strlen(s)));
}
#endif
