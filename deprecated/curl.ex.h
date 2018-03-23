#ifndef _curl_ex_h_
#define _curl_ex_h_

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <curl/curl.h>

typedef void (*curl_onreply)(void* param, const void* data, size_t bytes);

static inline int curl_request(CURL* curl, curl_onreply reply, void* param);

static inline CURL* curl_create()
{
	CURL* curl;
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

#if LIBCURL_VERSION_NUM >= 0x072400
	// A lot of servers don't yet support ALPN
	curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif

	return curl;
}

static inline int curl_post(CURL* curl, const char* url, const void* data, size_t bytes, curl_onreply reply, void* param)
{
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, bytes);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	return curl_request(curl, reply, param);
}

static inline int curl_get(CURL* curl, const char* url, curl_onreply reply, void* param)
{
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 0);
	return curl_request(curl, reply, param);
}

struct curl_data_t
{
    void* ptr;
    size_t bytes;
};

static size_t curl_onwrite(uint8_t *ptr, size_t size, size_t nmemb, struct curl_data_t *reply)
{
    void* p;
	size_t total = size * nmemb;
    if (total && reply)
    {
        p = realloc(reply->ptr, reply->bytes + total);
        if (p)
        {
            memcpy((char*)p + reply->bytes, ptr, total);
            reply->ptr = p;
            reply->bytes += total;
        }
        else
        {
            assert(0);
            return 0;
        }
    }
	return total;
}

static inline int curl_request(CURL* curl, curl_onreply reply, void* param)
{
    long response_code = -1;
	struct curl_data_t data;
    memset(&data, 0, sizeof(data));

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_onwrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

	if (curl_easy_perform(curl) == CURLE_OK
		&& curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code) == CURLE_OK
		&& response_code < 300)
	{
		if(reply)
			reply(param, data.ptr, data.bytes);
        free(data.ptr);
		return CURLE_OK;
	}

	free(data.ptr);
	return -1;
}

#endif /* !_curl_ex_h_ */
