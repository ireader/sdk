#include "aio-socket.h"
#include <assert.h>

typedef BOOL (PASCAL FAR * FAcceptEx)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
typedef VOID (PASCAL FAR * FGetAcceptExSockaddrs)(PVOID, DWORD, DWORD, DWORD, struct sockaddr **, LPINT, struct sockaddr **, LPINT);
typedef BOOL (PASCAL FAR * FConnectEx)(SOCKET, const struct sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL (PASCAL FAR * FDisconnectEx)(SOCKET, LPOVERLAPPED, DWORD, DWORD);

#define WSAID_ACCEPTEX		{0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#define WSAID_CONNECTEX		{0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
#define WSAID_DISCONNECTEX	{0x7fda2e11,0x8630,0x436f,{0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57}}

#define SO_UPDATE_ACCEPT_CONTEXT    0x700B

static FAcceptEx AcceptEx;
static FGetAcceptExSockaddrs GetAcceptExSockaddrs;
static FConnectEx ConnectEx;
static FDisconnectEx DisconnectEx;

struct aio_context
{
	LONG ref;
	LONG closed;
	int own;
	SOCKET socket;
};

struct aio_context_accept
{
	aio_onaccept proc;
	void* param;
	
	SOCKET socket;
	char buffer[(sizeof(struct sockaddr_in)+16)*2];
};

struct aio_context_connect
{
	aio_onconnect proc;
	void* param;
};

struct aio_context_recv
{
	aio_onrecv proc;
	void* param;
};

struct aio_context_send
{
	aio_onsend proc;
	void* param;
};

struct aio_context_recvfrom
{
	aio_onrecvfrom proc;
	void* param;
	struct sockaddr_in addr;
	int addrlen;
};

struct aio_context_action
{
	WSAOVERLAPPED overlapped;
	void (*action)(struct aio_context *ctx, struct aio_context_action *aio, DWORD error, DWORD bytes);

	// Anonymous union
	union
	{
		struct aio_context_connect connect;
		struct aio_context_accept accept;
		struct aio_context_send send;
		struct aio_context_recv recv;
		struct aio_context_recvfrom recvfrom;
	};

	struct aio_context *context;
	struct aio_context_action *next;
};

static int s_cpu = 0; // cpu count
static HANDLE s_iocp = 0;
static CRITICAL_SECTION s_locker;
static struct aio_context_action *s_actions = NULL;
static int s_actions_count = 0;

static int iocp_init()
{
	SOCKET sock;

	DWORD bytes = 0;
	GUID guid1 = WSAID_ACCEPTEX;
	GUID guid2 = WSAID_GETACCEPTEXSOCKADDRS;
	GUID guid3 = WSAID_CONNECTEX;
	GUID guid4 = WSAID_DISCONNECTEX;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid1, sizeof(GUID), &AcceptEx, sizeof(AcceptEx), &bytes, NULL, NULL);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid2, sizeof(GUID), &GetAcceptExSockaddrs, sizeof(GetAcceptExSockaddrs), &bytes, NULL, NULL);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid3, sizeof(GUID), &ConnectEx, sizeof(ConnectEx), &bytes, NULL, NULL);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid4, sizeof(GUID), &DisconnectEx, sizeof(DisconnectEx), &bytes, NULL, NULL);
	closesocket(sock);

	assert(AcceptEx && GetAcceptExSockaddrs && ConnectEx && DisconnectEx);
	return (AcceptEx && GetAcceptExSockaddrs && ConnectEx && DisconnectEx) ? 0 : -1;
}

static int iocp_create(int threads)
{
	SYSTEM_INFO sysinfo;
	s_cpu = threads;
	if(0 == threads)
	{
		GetSystemInfo(&sysinfo);
		s_cpu = sysinfo.dwNumberOfProcessors;
	}

	// create IOCP with n-thread
	assert(0 == s_iocp);
	s_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, s_cpu);
	if(NULL == s_iocp)
		return GetLastError();

	InitializeCriticalSection(&s_locker);
	return 0;
}

static int iocp_destroy()
{
	struct aio_context_action *action;

	if(NULL != s_iocp)
	{
		CloseHandle(s_iocp);
		s_iocp = 0;
	}

	// clear memory
	for(action = s_actions; action; action = s_actions)
	{
		--s_actions_count;
		s_actions = action->next;
		free(action);
	}
	assert(0 == s_actions_count);

	DeleteCriticalSection(&s_locker);
	return 0;
}

static int iocp_bind(SOCKET socket, ULONG_PTR key)
{
	HANDLE iocp;
	if(INVALID_HANDLE_VALUE == s_iocp)
		return -1;

	iocp = CreateIoCompletionPort((HANDLE)socket, s_iocp, key, 0);
	assert(iocp == s_iocp);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// iocp action
//////////////////////////////////////////////////////////////////////////
static void iocp_accept(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	char ip[16];
	int locallen, remotelen;
	struct sockaddr_in *local;
	struct sockaddr_in *remote;
	bytes = sizeof(struct sockaddr_in)+16;

	if(0 == error)
	{
		// http://msdn.microsoft.com/en-us/library/windows/desktop/ms737524%28v=vs.85%29.aspx
		// When the AcceptEx function returns, 
		// the socket sAcceptSocket is in the default state for a connected socket. 
		// The socket sAcceptSocket does not inherit the properties of the socket associated 
		// with sListenSocket parameter until SO_UPDATE_ACCEPT_CONTEXT is set on the socket
		setsockopt(aio->accept.socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&ctx->socket, sizeof(ctx->socket));

		local = remote = NULL;
		locallen = remotelen = 0;
		GetAcceptExSockaddrs(aio->accept.buffer, 0, bytes, bytes, (struct sockaddr **)&local, &locallen, (struct sockaddr **)&remote, &remotelen);

		strcpy(ip, inet_ntoa(remote->sin_addr));
		aio->accept.proc(aio->accept.param, 0, aio->accept.socket, ip, (int)ntohs(remote->sin_port));
	}
	else
	{
		closesocket(aio->accept.socket); // close handle
		aio->accept.proc(aio->accept.param, error, 0, NULL, 0);
	}
}

static void iocp_connect(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	ctx, bytes;
	aio->connect.proc(aio->connect.param, error);
}

static void iocp_recv(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	ctx;
	aio->recv.proc(aio->recv.param, error, bytes);
}

static void iocp_send(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	ctx;
	aio->send.proc(aio->send.param, error, bytes);
}

static void iocp_recvfrom(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	char ip[16] = {0};
	struct sockaddr_in *remote = &aio->recvfrom.addr;

	ctx;
	if(0 == error)
		strcpy(ip, inet_ntoa(remote->sin_addr));

	aio->recvfrom.proc(aio->recvfrom.param, error, bytes, ip, ntohs(remote->sin_port));
}

//////////////////////////////////////////////////////////////////////////
/// utility functions
//////////////////////////////////////////////////////////////////////////
static struct aio_context_action* util_alloc(struct aio_context *ctx)
{
	struct aio_context_action* aio = NULL;

	EnterCriticalSection(&s_locker);
	if(s_actions)
	{
		assert(s_actions_count > 0);
		aio = s_actions;
		s_actions = s_actions->next;
		--s_actions_count;
	}
	LeaveCriticalSection(&s_locker);

	if(!aio)
		aio = (struct aio_context_action*)malloc(sizeof(struct aio_context_action));

	if(aio)
	{
		memset(aio, 0, sizeof(struct aio_context_action));
		InterlockedIncrement(&ctx->ref);
		aio->context = ctx;
	}

	return aio;
}

static void util_free(struct aio_context_action* aio)
{
	if(0 == InterlockedDecrement(&aio->context->ref))
	{
		assert(1 == aio->context->closed);
		free(aio->context);
	}

	EnterCriticalSection(&s_locker);
	if(s_actions_count < s_cpu+1)
	{
		aio->next = s_actions;
		s_actions = aio;
		++s_actions_count;
		aio = NULL; // stop free
	}
	LeaveCriticalSection(&s_locker);

	if(aio)
		free(aio);
}

__forceinline int iocp_check_closed(struct aio_context_action* aio)
{
	LONG r = InterlockedCompareExchange(&aio->context->closed, 1, 1);

	if(0 != r && aio->action == iocp_accept)
		closesocket(aio->accept.socket); // close socket

	return r;
}

//////////////////////////////////////////////////////////////////////////
/// aio functions
//////////////////////////////////////////////////////////////////////////
int aio_socket_init(int threads)
{
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);

	iocp_init();
	return iocp_create(threads);
}

int aio_socket_clean(void)
{
	iocp_destroy();
	return WSACleanup();
}

int aio_socket_process(int timeout)
{
	BOOL status;
	DWORD bytes;
	ULONG completionKey;
	OVERLAPPED *pOverlapped;
	struct aio_context *ctx;
	struct aio_context_action *aio;

	status = GetQueuedCompletionStatus(s_iocp, &bytes, &completionKey, &pOverlapped, timeout);
	if(status)
	{
		assert(completionKey && pOverlapped);

		// action
		ctx = (struct aio_context*)completionKey;
		aio = (struct aio_context_action*)pOverlapped;

		if(0 == iocp_check_closed(aio))
			aio->action(ctx, aio, 0, bytes);
		util_free(aio);
	}
	else
	{
		DWORD err = GetLastError();
		if(NULL == pOverlapped)
		{
			if(WAIT_TIMEOUT == err)
			{
				return 0; // timeout
			}
			else
			{
				// exception
				assert(0);
			}
		}
		else
		{
			// io failed
			assert(completionKey);
			ctx = (struct aio_context*)completionKey;
			aio = (struct aio_context_action*)pOverlapped;
			if(0 == iocp_check_closed(aio))
				aio->action(ctx, aio, err, bytes);
			util_free(aio);
		}
	}
	return 0;
}

aio_socket_t aio_socket_create(socket_t socket, int own)
{
	struct aio_context *ctx = NULL;
	ctx = (struct aio_context*)malloc(sizeof(struct aio_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct aio_context));
	ctx->socket = socket;
	ctx->own = own;
	ctx->ref = 1;
	ctx->closed = 0;

	if(0 != iocp_bind(ctx->socket, (ULONG_PTR)ctx))
	{
		free(ctx);
		return NULL;
	}

	return ctx;
}

int aio_socket_destroy(aio_socket_t socket)
{
	struct aio_context *ctx = (struct aio_context*)socket;
	InterlockedExchange(&ctx->closed, 1);

	if(ctx->own)
		closesocket(ctx->socket);

	if(0 == InterlockedDecrement(&ctx->ref))
		free(ctx);
	return 0;
}

int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param)
{
	struct aio_context *ctx = (struct aio_context*)socket;
	DWORD dwBytes = sizeof(struct sockaddr_in)+16;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_accept;
	aio->accept.proc = proc;
	aio->accept.param = param;
	aio->accept.socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(INVALID_SOCKET == aio->accept.socket)
	{
		closesocket(aio->accept.socket);
		util_free(aio);
		return WSAGetLastError();
	}

	if(!AcceptEx(ctx->socket, aio->accept.socket, aio->accept.buffer, 0, dwBytes, dwBytes, &dwBytes, &aio->overlapped))
	{
		DWORD ret = WSAGetLastError();
		if(ERROR_IO_PENDING != ret)
		{
			closesocket(aio->accept.socket);
			util_free(aio);
			return ret;
		}
	}
	return 0;
}

int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param)
{
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);
	addr.sin_addr.s_addr = inet_addr(ip);

	aio = util_alloc(ctx);
	aio->action = iocp_connect;
	aio->connect.proc = proc;
	aio->connect.param = param;

	if(!ConnectEx(ctx->socket, (const struct sockaddr *)&addr, sizeof(addr), NULL, 0, NULL, &aio->overlapped))
	{
		DWORD ret = WSAGetLastError();
		if(ERROR_IO_PENDING != ret)
		{
			util_free(aio);
			return ret;
		}
	}
	return 0;
}

int aio_socket_recv(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecv proc, void* param)
{
	socket_bufvec_t vec[1];
	vec[0].buf = buffer;
	vec[0].len = bytes;
	return aio_socket_recv_v(socket, vec, 1, proc, param);
}

int aio_socket_send(aio_socket_t socket, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	socket_bufvec_t vec[1];
	vec[0].buf = (CHAR FAR*)buffer;
	vec[0].len = bytes;
	return aio_socket_send_v(socket, vec, 1, proc, param);
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param)
{
	DWORD flags = 0;
	DWORD dwBytes = 0;
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_recv;
	aio->recv.proc = proc;
	aio->recv.param = param;

	if(SOCKET_ERROR == WSARecv(ctx->socket, vec, n, &dwBytes, &flags, &aio->overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING != ret)
		{
			util_free(aio);
			return ret;
		}
	}
	return 0;
}

int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	DWORD dwBytes = 0;
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_send;
	aio->send.proc = proc;
	aio->send.param = param;

	if(SOCKET_ERROR == WSASend(ctx->socket, vec, n, &dwBytes, 0, &aio->overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING != ret)
		{
			util_free(aio);
			return ret;
		}
	}
	return 0;
}

int aio_socket_recvfrom(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecvfrom proc, void* param)
{
	socket_bufvec_t vec[1];
	vec[0].buf = buffer;
	vec[0].len = bytes;
	return aio_socket_recvfrom_v(socket, vec, 1, proc, param);
}

int aio_socket_sendto(aio_socket_t socket, const char* ip, int port, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	socket_bufvec_t vec[1];
	vec[0].buf = (CHAR FAR*)buffer;
	vec[0].len = bytes;
	return aio_socket_sendto_v(socket, ip, port, vec, 1, proc, param);
}

int aio_socket_recvfrom_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvfrom proc, void* param)
{
	DWORD flags = 0;
	DWORD dwBytes = 0;
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_recvfrom;
	aio->recvfrom.proc = proc;
	aio->recvfrom.param = param;
	aio->recvfrom.addrlen = sizeof(aio->recvfrom.addr);

	if(SOCKET_ERROR == WSARecvFrom(ctx->socket, vec, (DWORD)n, &dwBytes, &flags, (struct sockaddr *)&aio->recvfrom.addr, &aio->recvfrom.addrlen, &aio->overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING != ret)
		{
			util_free(aio);
			return ret;
		}
	}
	return 0;
}

int aio_socket_sendto_v(aio_socket_t socket, const char* ip, int port, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	DWORD dwBytes = sizeof(struct sockaddr_in);
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);
	addr.sin_addr.s_addr = inet_addr(ip);

	aio = util_alloc(ctx);
	aio->action = iocp_send;
	aio->send.proc = proc;
	aio->send.param = param;

	if(SOCKET_ERROR == WSASendTo(ctx->socket, vec, (DWORD)n, &dwBytes, 0, (const struct sockaddr *)&addr, (int)dwBytes, &aio->overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING != ret)
		{
			util_free(aio);
			return ret;
		}
	}
	return 0;
}
