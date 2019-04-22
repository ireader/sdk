// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes

#include "http-reason.h"

const char* http_reason_phrase(int code)
{
	static const char *reason1xx[] = 
	{
		"Continue", // 100
		"Switching Protocols" // 101
		"Processing", // 102-RFC2518 Section 10.1
		"Early Hints", // 103-RFC8297
	};

	static const char *reason2xx[] = 
	{
		"OK", // 200
		"Created", // 201
		"Accepted", // 202
		"Non-Authoritative Information", // 203
		"No Content", // 204
		"Reset Content", // 205
		"Partial Content", // 206
		"Multi-Status", // 207-RFC4918 Section 13
		"Already Reported", // 208-RFC5842 Section 7.1
	};

	static const char *reason3xx[] = 
	{
		"Multiple Choices", // 300
		"Move Permanently", // 301
		"Found", // 302
		"See Other", // 303
		"Not Modified", // 304
		"Use Proxy", // 305
		"Unused", // 306
		"Temporary Redirect", // 307
		"Permanent Redirect", // 308-RFC738 Section 3
	};

	static const char *reason4xx[] = 
	{
		"Bad Request", // 400
		"Unauthorized", // 401
		"Payment Required", // 402
		"Forbidden", // 403
		"Not Found", // 404
		"Method Not Allowed", // 405
		"Not Acceptable", // 406
		"Proxy Authentication Required", // 407
		"Request Timeout", // 408
		"Conflict", // 409
		"Gone", // 410
		"Length Required", //411
		"Precondition Failed", // 412
		"Request Entity Too Large", // 413
		"Request-URI Too Long", // 414
		"Unsupported Media Type", // 415
		"Request Range Not Satisfiable", // 416
		"Expectation Failed", // 417
		"I'm a teapot", // 418-RFC2324 Section 2.3.2
		"", // 419
		"Bad Extension", // 420-RFC3261 Section 21.4.15
		"Extension Required", // 421-RFC3261 Section 21.4.16
		"Unprocessable Entity", // 422-RFC4918 Section 11.2
		"Interval Too Brief", // 423-RFC3261 Section 21.4.16
		"Failed Dependency", // 424-RFC4918 Section 11.4
		"Unordered Collection", // 425
		"Upgrade Required", // 426-RFC7231 Section 6.5.15
		"", // 427
		"Precondition Required", // 428-RFC6585 Section 3
		"Too Many Requests", // 429-RFC6585 Section 4
		"Stale Credentials", // 430-RFC3489 Section 11
//		"Integrity Check Failure", // 431-RFC3489 Section 11
		"Request Header Fields Too Large", // 431-RFC6585 Section 5
		"Missing Username", // 432-RFC3489 Section 11
		"Use TLS", // 433-RFC3489 Section 11
		"", // 434
		"", // 435
		"", // 436
		"Allocation Mismatch", // 437-RFC5766 Section 6
		"Stale Nonce", // 438-RFC5389 Section 11
		"", // 439
		"", // 440
		"Wrong Credentials", // 441-RFC5766 Section 6
		"Unsupported Transport Address", // 442-RFC5766 Section 6

		//"Allocation Quota Reached", // 486-RFC5766 Section 6
	};

	static const char *reason5xx[] = 
	{
		"Internal Server Error", // 500
		"Not Implemented", // 501
		"Bad Gateway", // 502
		"Service Unavailable", // 503
		"Gateway Timeout", // 504
		"HTTP Version Not Supported", // 505
		"Variant Also Negotiates", // 506-RFC2295 Section 8.1
		"Insufficient Storage", // 507-RFC4918 Section 11.5
		"Loop Detected", // 508-RFC5842 Section 7.2
		"Bandwidth Limit Exceeded", // 509-Apache Web Server/cPanel
		"Not Extended", // 510-RFC2774 Section 7
		"Network Authentication Required", // 511-RFC6585 Section 6
		"", // 512
		"Message Too Large", // 513-RFC3261 Section 21.5.7
	};

	if(code >= 100 && code < 100+sizeof(reason1xx)/sizeof(reason1xx[0]))
		return reason1xx[code-100];
	else if(code >= 200 && code < 200+sizeof(reason2xx)/sizeof(reason2xx[0]))
		return reason2xx[code-200];
	else if(code >= 300 && code < 300+sizeof(reason3xx)/sizeof(reason3xx[0]))
		return reason3xx[code-300];
	else if(code >= 400 && code < 400+sizeof(reason4xx)/sizeof(reason4xx[0]))
		return reason4xx[code-400];
	else if(code >= 500 && code < 500+sizeof(reason5xx)/sizeof(reason5xx[0]))
		return reason5xx[code-500];
	else
		return "unknown";
}
