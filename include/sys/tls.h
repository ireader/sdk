#ifndef __platform_tls_h_
#define __platform_ls_h_

#if defined(OS_WINDOWS)
#include <windows.h>
typedef DWORD tlskey_t;
#else
#include <pthread.h>
typedef pthread_key_t tlskey_t;
#endif

// Thread Local Storage
//-------------------------------------------------------------------------------------
// int tls_create(tlskey_t* key);
// int tls_destroy(tlskey_t key);
// int tls_setvalue(tlskey_t key, void* value);
// void* tls_getvalue(tlskey_t key);
//-------------------------------------------------------------------------------------

///@return 0-ok, other-error
static inline int tls_create(tlskey_t* key)
{
#if defined(OS_WINDOWS)
	*key = TlsAlloc();
	return TLS_OUT_OF_INDEXES == *key ? GetLastError() : 0;
#else
	return pthread_key_create(key, NULL);
#endif
}

///@return 0-ok, other-error
static inline int tls_destroy(tlskey_t key)
{
#if defined(OS_WINDOWS)
	return TlsFree(key) ? 0 : GetLastError();
#else
	return pthread_key_delete(key);
#endif
}

///@return 0-ok, other-error
static inline int tls_setvalue(tlskey_t key, void* value)
{
#if defined(OS_WINDOWS)
	return TlsSetValue(key, value) ? 0 : GetLastError();
#else
	return pthread_setspecific(key, value);
#endif
}

static inline void* tls_getvalue(tlskey_t key)
{
#if defined(OS_WINDOWS)
	return TlsGetValue(key);
#else
	return pthread_getspecific(key);
#endif
}

#endif /* !__platform_tls_h_ */
