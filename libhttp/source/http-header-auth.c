/*
S->C:
WWW-Authenticate: Digest
	realm="http-auth@example.org",
	qop="auth, auth-int",
	algorithm=SHA-256,
	nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
	opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"
WWW-Authenticate: Digest
	realm="http-auth@example.org",
	qop="auth, auth-int",
	algorithm=MD5,
	nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
	opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"

C->S:
Authorization: Digest username="Mufasa",
	realm="http-auth@example.org",
	uri="/dir/index.html",
	algorithm=MD5,
	nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
	nc=00000001,
	cnonce="f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ",
	qop=auth,
	response="8ca523f5e9506fed4657c9700eebdbec",
	opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"
*/

#include "http-header-auth.h"
#include "base64.h"
#include "md5.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static void base16(char* str, const uint8_t* data, int bytes)
{
	int i;
	const char hex[] = "0123456789abcdef";

	for (i = 0; i < bytes; i++)
	{
		str[i * 2] = hex[data[i] >> 4];
		str[i * 2 + 1] = hex[data[i] & 0xF];
	}

	str[bytes * 2] = 0;
}

static void	md5_A1(char A1[33], const char* algorithm, const char* usr, const char* pwd, const char* realm, const char* nonce, const char* cnonce)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)usr, (unsigned int)strlen(usr));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)realm, (unsigned int)strlen(realm));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)pwd, (unsigned int)strlen(pwd));
	MD5Final(md5, &ctx);

	// algorithm endwith -sess
	if (0 == strncmp(algorithm + strlen(algorithm) - 5, "-sess", 5))
	{
		MD5Init(&ctx);
		MD5Update(&ctx, md5, 16);
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)nonce, (unsigned int)strlen(nonce));
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)cnonce, (unsigned int)strlen(cnonce));
		MD5Final(md5, &ctx);
	}

	base16(A1, md5, 16);
}

static void	md5_A2(char A2[33], const char* method, const char* uri, const char* qop, const char* md5entity)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)method, (unsigned int)strlen(method));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)uri, (unsigned int)strlen(uri));
	if (0 == strcmp("auth-int", qop))
	{
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)md5entity, 32);
	}
	MD5Final(md5, &ctx);

	base16(A2, md5, 16);
}

static void md5_response(char reponse[33], const char* A1, const char* A2, const char* nonce, int nc, const char* cnonce, const char* qop)
{
	MD5_CTX ctx;
	char hex[32];
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)A1, 32);
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)nonce, (unsigned int)strlen(nonce));
	MD5Update(&ctx, (unsigned char*)":", 1);
	if (*qop)
	{
		snprintf(hex, sizeof(hex), "%08x", nc);
		MD5Update(&ctx, (unsigned char*)hex, (unsigned int)strlen(hex));
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)cnonce, (unsigned int)strlen(cnonce));
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)qop, (unsigned int)strlen(qop));
		MD5Update(&ctx, (unsigned char*)":", 1);
	}
	MD5Update(&ctx, (unsigned char*)A2, 32);
	MD5Final(md5, &ctx);

	base16(reponse, md5, 16);
}

static void	md5_username(char r[33], const char* user, const char* realm)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)user, (unsigned int)strlen(user));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)realm, (unsigned int)strlen(realm));
	MD5Final(md5, &ctx);

	base16(r, md5, 16);
}

static void	md5_entity(char r[33], const uint8_t* entity, int bytes)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)entity, bytes);
	MD5Final(md5, &ctx);

	base16(r, md5, 16);
}

int http_header_auth(const struct http_header_www_authenticate_t* auth, const char* pwd, const char* method, const char* content, int length, char* authenrization, int bytes)
{
	int n;
	char A1[33];
	char A2[33];
	char entity[33];
	struct http_header_www_authenticate_t auth2;

	memcpy(&auth2, auth, sizeof(auth2));
	if (HTTP_AUTHENTICATION_BASIC == auth->scheme)
	{
		n = snprintf(authenrization, bytes, "%s:%s", auth->username, pwd);
		if (n < 0 || n + 1 >= bytes || (n + 2) / 3 * 4 + n / 57 + 1 > sizeof(auth->response))
			return -E2BIG;
		n = (int)base64_encode(auth2.response, authenrization, n);
		return http_header_authorization_write(&auth2, authenrization, bytes);
	}
	else if (HTTP_AUTHENTICATION_DIGEST == auth->scheme)
	{
		// username
		md5_entity(entity, (const uint8_t*)content, length); // empty entity
		md5_A1(A1, auth->algorithm, auth->username, pwd, auth->realm, auth->nonce, auth->cnonce);
		md5_A2(A2, method, auth->uri, auth->qop, entity);
		
		md5_response(auth2.response, A1, A2, auth->nonce, auth->nc, auth->cnonce, auth->qop);
		if (auth->userhash) md5_username(auth2.username, auth->username, auth->realm);
		return http_header_authorization_write(&auth2, authenrization, bytes);
	}
	else
	{
		// nothing to do
		assert(0);
		return -1;
	}
}

#if defined(_DEBUG) || defined(DEBUG)
void http_header_auth_test(void)
{
	char buffer[1024];
	struct http_header_www_authenticate_t auth;
	memset(&auth, 0, sizeof(auth));
	auth.scheme = HTTP_AUTHENTICATION_DIGEST;
	strcpy(auth.username, "Mufasa");
	//strcpy(auth.pwd, "Circle Of Life");
	auth.nc = 1;
	strcpy(auth.realm, "testrealm@host.com");
	strcpy(auth.opaque, "5ccc069c403ebaf9f0171e9517f40e41");
	strcpy(auth.nonce, "dcd98b7102dd2f0e8b11d0f600bfb0c093");
	strcpy(auth.cnonce, "0a4f113b");
	strcpy(auth.qop, "auth");
	strcpy(auth.uri, "/dir/index.html");
	http_header_auth(&auth, "Circle Of Life", "GET", NULL, 0, buffer, sizeof(buffer));
	assert(strstr(buffer, "response=\"6629fae49393a05397450978507c4ef1\""));

	memset(&auth, 0, sizeof(auth));
	auth.scheme = HTTP_AUTHENTICATION_BASIC;
	strcpy(auth.username, "Aladdin");
	http_header_auth(&auth, "open sesame", "GET", NULL, 0, buffer, sizeof(buffer));
	assert(strstr(buffer, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="));
}
#endif
