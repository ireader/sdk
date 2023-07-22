#if defined(OS_WINDOWS)
#include "aio-socket.h"
#include <WS2tcpip.h>
#include <stdlib.h>
#include <assert.h>

// aio_socket_process(socket)
// 1. socket_setrecvtimeout(socket, xxx) failed, don't callback
// 2. socket_shutdown(SD_SEND) failed, don't callback
// 3. socket_shutdown(SD_RECEIVE) failed, don't callback
// 4. socket_close ok, callback read and write

// https://msdn.microsoft.com/en-us/library/windows/desktop/ms737582%28v=vs.85%29.aspx
// MSDN > closesocket function > Remarks
// Any pending overlapped send and receive operations ( WSASend/ WSASendTo/ WSARecv/ WSARecvFrom with an overlapped socket) 
// issued by any thread in this process are also canceled. Any event, completion routine, or completion port action 
// specified for these overlapped operations is performed. The pending overlapped operations fail with the error status WSA_OPERATION_ABORTED.

typedef BOOL (PASCAL FAR * FAcceptEx)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
typedef VOID (PASCAL FAR * FGetAcceptExSockaddrs)(PVOID, DWORD, DWORD, DWORD, struct sockaddr **, LPINT, struct sockaddr **, LPINT);
typedef BOOL (PASCAL FAR * FConnectEx)(SOCKET, const struct sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL (PASCAL FAR * FDisconnectEx)(SOCKET, LPOVERLAPPED, DWORD, DWORD);
typedef INT  (PASCAL FAR * FWSARECVMSG)(SOCKET, LPWSAMSG, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
//typedef INT	 (PASCAL FAR*  FWSASENDMSG)(SOCKET, LPWSAMSG, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

#define WSAID_ACCEPTEX		{0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#define WSAID_CONNECTEX		{0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
#define WSAID_DISCONNECTEX	{0x7fda2e11,0x8630,0x436f,{0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57}}
#define WSAID_WSARECVMSG	{0xf689d7c8,0x6f1f,0x436b,{0x8a,0x53,0xe5,0x4f,0xe3,0x51,0xc3,0x22}}
#define WSAID_WSASENDMSG	{0xa441e712,0x754f,0x43ca,{0x84,0xa7,0x0d,0xee,0x44,0xcf,0x60,0x6d}}

#define SO_UPDATE_ACCEPT_CONTEXT    0x700B
#define SO_UPDATE_CONNECT_CONTEXT   0x7010

static FAcceptEx AcceptEx;
static FGetAcceptExSockaddrs GetAcceptExSockaddrs;
static FConnectEx ConnectEx;
static FDisconnectEx DisconnectEx;
static FWSARECVMSG WSARecvMsg;
//static FWSASENDMSG WSASendMsg;

enum { AIO_READ = 0x01, AIO_WRITE = 0x02, };

struct aio_context
{
	volatile LONG ref;
	//volatile LONG closed;
	volatile LONG flags;

	int own;
	SOCKET socket;

	aio_ondestroy ondestroy;
	void* param;
};

struct aio_context_accept
{
	aio_onaccept proc;
	void* param;
	
	SOCKET socket;
	char buffer[sizeof(struct sockaddr_storage)*2];
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
	socklen_t addrlen;
	struct sockaddr_storage addr;
};

struct aio_context_recvmsg
{
	aio_onrecvmsg proc;
	void* param;

	WSAMSG wsamsg;
	char control[64];
	struct sockaddr_storage peer;
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
		struct aio_context_recvmsg recvmsg;
	};

	struct aio_context *context;
	struct aio_context_action *next;
};

static int s_cpu = 0; // cpu count
static HANDLE s_iocp = 0;
static CRITICAL_SECTION s_locker;
static struct aio_context_action *s_actions = NULL; // TODO: lock-free queue
static int s_actions_count = 0;

static int iocp_init()
{
	SOCKET sock;

	DWORD bytes = 0;
	GUID guid1 = WSAID_ACCEPTEX;
	GUID guid2 = WSAID_GETACCEPTEXSOCKADDRS;
	GUID guid3 = WSAID_CONNECTEX;
	GUID guid4 = WSAID_DISCONNECTEX;
	GUID guid5 = WSAID_WSARECVMSG;
	//GUID guid6 = WSAID_WSASENDMSG;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid1, sizeof(GUID), &AcceptEx, sizeof(AcceptEx), &bytes, NULL, NULL);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid2, sizeof(GUID), &GetAcceptExSockaddrs, sizeof(GetAcceptExSockaddrs), &bytes, NULL, NULL);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid3, sizeof(GUID), &ConnectEx, sizeof(ConnectEx), &bytes, NULL, NULL);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid4, sizeof(GUID), &DisconnectEx, sizeof(DisconnectEx), &bytes, NULL, NULL);
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid5, sizeof(GUID), &WSARecvMsg, sizeof(WSARecvMsg), &bytes, NULL, NULL);
	//WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid6, sizeof(GUID), &WSASendMsg, sizeof(WSASendMsg), &bytes, NULL, NULL);
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

#if (WINVER >= 0x0403)
	InitializeCriticalSectionAndSpinCount(&s_locker, 0x00000400); // spin-lock
#else
	InitializeCriticalSection(&s_locker);
#endif
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
	int locallen, remotelen;
	struct sockaddr *local;
	struct sockaddr *remote;
	assert(0 != (AIO_READ & InterlockedAnd(&ctx->flags, ~AIO_READ)));
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
		GetAcceptExSockaddrs(aio->accept.buffer, 0, sizeof(aio->accept.buffer)/2, sizeof(aio->accept.buffer)/2, &local, &locallen, &remote, &remotelen);
		aio->accept.proc(aio->accept.param, 0, aio->accept.socket, remote, remotelen);
		//aio->accept.proc(aio->accept.param, 0, aio->accept.socket, ip, (int)ntohs(remote->sin_port));
	}
	else
	{
		closesocket(aio->accept.socket); // close handle
		aio->accept.proc(aio->accept.param, error, 0, NULL, 0);
	}
}

static void iocp_connect(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	(void)bytes;
	assert(0 != (AIO_WRITE & InterlockedAnd(&ctx->flags, ~AIO_WRITE)));
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms737606%28v=vs.85%29.aspx
	// When the ConnectEx function returns TRUE, the socket s is in the default state for a connected socket. 
	// The socket s does not enable previously set properties or options until SO_UPDATE_CONNECT_CONTEXT is 
	// set on the socket. Use the setsockopt function to set the SO_UPDATE_CONNECT_CONTEXT option.
	setsockopt(ctx->socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0); // fix WSAENOTCONN

	aio->connect.proc(aio->connect.param, error);
}

static void iocp_recv(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	assert(0 != (AIO_READ & InterlockedAnd(&ctx->flags, ~AIO_READ)));
	aio->recv.proc(aio->recv.param, error, bytes);
}

static void iocp_send(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	assert(0 != (AIO_WRITE & InterlockedAnd(&ctx->flags, ~AIO_WRITE)));
	aio->send.proc(aio->send.param, error, bytes);
}

static void iocp_recvfrom(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	assert(0 != (AIO_READ & InterlockedAnd(&ctx->flags, ~AIO_READ)));
	aio->recvfrom.proc(aio->recvfrom.param, error, bytes, (struct sockaddr*)&aio->recvfrom.addr, aio->recvfrom.addrlen);
}

static void iocp_recvmsg(struct aio_context* ctx, struct aio_context_action* aio, DWORD error, DWORD bytes)
{
	WSAMSG* wsamsg;
	WSACMSGHDR* cmsg;
	struct in_pktinfo* pktinfo;
	struct in6_pktinfo* pktinfo6;
	struct sockaddr_storage local;
	socklen_t locallen;

	assert(0 != (AIO_READ & InterlockedAnd(&ctx->flags, ~AIO_READ)));

	locallen = 0;
	wsamsg = &aio->recvmsg.wsamsg;
	memset(&local, 0, sizeof(local));
	for (cmsg = WSA_CMSG_FIRSTHDR(wsamsg); 0 == error && !!cmsg; cmsg = WSA_CMSG_NXTHDR(wsamsg, cmsg))
	{
		if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO)
		{
			pktinfo = (struct in_pktinfo*)WSA_CMSG_DATA(cmsg);
			locallen = sizeof(struct sockaddr_in);
			((struct sockaddr_in*)&local)->sin_family = AF_INET;
			memcpy(&((struct sockaddr_in*)&local)->sin_addr, &pktinfo->ipi_addr, sizeof(pktinfo->ipi_addr));
			break;
		}
		else if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
		{
			pktinfo6 = (struct in6_pktinfo*)WSA_CMSG_DATA(cmsg);
			locallen = sizeof(struct sockaddr_in6);
			((struct sockaddr_in6*)&local)->sin6_family = AF_INET6;
			memcpy(&((struct sockaddr_in6*)&local)->sin6_addr, &pktinfo6->ipi6_addr, sizeof(pktinfo6->ipi6_addr));
			break;
		}
	}

	aio->recvmsg.proc(aio->recvmsg.param, error, bytes, (struct sockaddr*)&aio->recvmsg.peer, wsamsg->namelen, (struct sockaddr*)&local, locallen);
}

//////////////////////////////////////////////////////////////////////////
/// utility functions
//////////////////////////////////////////////////////////////////////////
static int aio_socket_release(struct aio_context *ctx)
{
	LONG ref;
	ref = InterlockedDecrement(&ctx->ref);
	if (0 == ref)
	{
		assert(0 == ctx->flags);
		//assert(1 == ctx->closed);
		if (ctx->ondestroy)
			ctx->ondestroy(ctx->param);

#if defined(DEBUG) || defined(_DEBUG)
		memset(ctx, 0xCC, sizeof(*ctx));
#endif
		free(ctx);
	}

	assert(ref >= 0);
	return ref;
}

static struct aio_context_action* util_alloc(struct aio_context *ctx)
{
	struct aio_context_action* aio = NULL;

	// lock-free dequeue
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
	aio_socket_release(aio->context);

	// lock-free enqueue
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

//__forceinline int iocp_check_closed(struct aio_context_action* aio)
//{
//	LONG r = InterlockedCompareExchange(&aio->context->closed, 1, 1);
//
//	if(0 != r && aio->action == iocp_accept)
//		closesocket(aio->accept.socket); // close socket(create in aio_socket_accept)
//
//	return r;
//}

static inline int aio_socket_result(struct aio_context_action *aio, int flag)
{
	DWORD ret = WSAGetLastError();
	if (WSA_IO_PENDING != ret)
	{
		assert(0 != (flag & InterlockedAnd(&aio->context->flags, ~flag)));
		util_free(aio);
		return ret;
	}
	return 0;
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
	DWORD bytes;
	ULONG_PTR completionKey;
	OVERLAPPED *pOverlapped;
	struct aio_context *ctx;
	struct aio_context_action *aio;

	if(GetQueuedCompletionStatus(s_iocp, &bytes, &completionKey, &pOverlapped, timeout))
	{
		assert(completionKey && pOverlapped);

		// action
		ctx = (struct aio_context*)completionKey;
		aio = (struct aio_context_action*)pOverlapped;

//		if(0 == iocp_check_closed(aio))
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
				assert(0); // exception
			}
		}
		else
		{
			// io failed
			assert(completionKey);
			ctx = (struct aio_context*)completionKey;
			aio = (struct aio_context_action*)pOverlapped;
//			if(0 == iocp_check_closed(aio))
				aio->action(ctx, aio, err, bytes);
			util_free(aio);
		}
	}
	return 1;
}

aio_socket_t aio_socket_create(socket_t socket, int own)
{
	struct aio_context *ctx = NULL;
	ctx = (struct aio_context*)calloc(1, sizeof(struct aio_context));
	if(!ctx)
		return NULL;

	ctx->socket = socket;
	ctx->own = own;
	ctx->ref = 1;
	ctx->flags = 0;
//	ctx->closed = 0;

	if(0 != iocp_bind(ctx->socket, (ULONG_PTR)ctx))
	{
		free(ctx);
		return NULL;
	}

	return ctx;
}

int aio_socket_destroy(aio_socket_t socket, aio_ondestroy ondestroy, void* param)
{
	struct aio_context *ctx = (struct aio_context*)socket;
//	InterlockedExchange(&ctx->closed, 1);
	ctx->ondestroy = ondestroy;
	ctx->param = param;

	if(ctx->own)
	{
		closesocket(ctx->socket);
		ctx->socket = INVALID_SOCKET;
	}

	aio_socket_release(ctx);
	return 0;
}

int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param)
{
	int ret;
	DWORD dwBytes = 0;
	WSAPROTOCOL_INFOW pi;
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;
	
	ret = sizeof(pi);
	if (0 != getsockopt(ctx->socket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&pi, &ret))
		return WSAGetLastError();

	aio = util_alloc(ctx);
	aio->action = iocp_accept;
	aio->accept.proc = proc;
	aio->accept.param = param;
	aio->accept.socket = WSASocket(pi.iAddressFamily, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(INVALID_SOCKET == aio->accept.socket)
	{
		closesocket(aio->accept.socket);
		util_free(aio);
		return WSAGetLastError();
	}

	dwBytes = sizeof(aio->accept.buffer) / 2;
	assert(0 == (AIO_READ & InterlockedOr(&ctx->flags, AIO_READ)));
	if (!AcceptEx(ctx->socket, aio->accept.socket, aio->accept.buffer, 0, dwBytes, dwBytes, &dwBytes, &aio->overlapped))
	{
		ret = aio_socket_result(aio, AIO_READ);
		if(0 != ret)
		{
			closesocket(aio->accept.socket);
			return ret;
		}
	}
	return 0;
}

int aio_socket_connect(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, aio_onconnect proc, void* param)
{
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_connect;
	aio->connect.proc = proc;
	aio->connect.param = param;

	assert(0 == (AIO_WRITE & InterlockedOr(&ctx->flags, AIO_WRITE)));
	if (!ConnectEx(ctx->socket, addr, addrlen, NULL, 0, NULL, &aio->overlapped))
	{
		return aio_socket_result(aio, AIO_WRITE);
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
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_recv;
	aio->recv.proc = proc;
	aio->recv.param = param;

	assert(0 == (AIO_READ & InterlockedOr(&ctx->flags, AIO_READ)));
	if(SOCKET_ERROR == WSARecv(ctx->socket, vec, n, NULL/*&dwBytes*/, &flags, &aio->overlapped, NULL))
	{
		return aio_socket_result(aio, AIO_READ);
	}
	return 0;
}

int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_send;
	aio->send.proc = proc;
	aio->send.param = param;

	assert(0 == (AIO_WRITE & InterlockedOr(&ctx->flags, AIO_WRITE)));
	if(SOCKET_ERROR == WSASend(ctx->socket, vec, n, NULL/*&dwBytes*/, 0, &aio->overlapped, NULL))
	{
		return aio_socket_result(aio, AIO_WRITE);
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

int aio_socket_sendto(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	socket_bufvec_t vec[1];
	vec[0].buf = (CHAR FAR*)buffer;
	vec[0].len = bytes;
	return aio_socket_sendto_v(socket, addr, addrlen, vec, 1, proc, param);
}

int aio_socket_recvfrom_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvfrom proc, void* param)
{
	DWORD flags = 0;
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;

	aio = util_alloc(ctx);
	aio->action = iocp_recvfrom;
	aio->recvfrom.proc = proc;
	aio->recvfrom.param = param;
	aio->recvfrom.addrlen = sizeof(aio->recvfrom.addr);

	assert(0 == (AIO_READ & InterlockedOr(&ctx->flags, AIO_READ)));
	if(SOCKET_ERROR == WSARecvFrom(ctx->socket, vec, (DWORD)n, NULL/*&dwBytes*/, &flags, (struct sockaddr *)&aio->recvfrom.addr, &aio->recvfrom.addrlen, &aio->overlapped, NULL))
	{
		return aio_socket_result(aio, AIO_READ);
	}
	return 0;
}

int aio_socket_sendto_v(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	struct aio_context *ctx = (struct aio_context*)socket;
	struct aio_context_action *aio;
	
	aio = util_alloc(ctx);
	aio->action = iocp_send;
	aio->send.proc = proc;
	aio->send.param = param;

	assert(0 == (AIO_WRITE & InterlockedOr(&ctx->flags, AIO_WRITE)));
	if(SOCKET_ERROR == WSASendTo(ctx->socket, vec, (DWORD)n, NULL/*&dwBytes*/, 0, addr, addrlen, &aio->overlapped, NULL))
	{
		return aio_socket_result(aio, AIO_WRITE);
	}
	return 0;
}

int aio_socket_recvmsg(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecvmsg proc, void* param)
{
	socket_bufvec_t vec[1];
	vec[0].buf = buffer;
	vec[0].len = bytes;
	return aio_socket_recvmsg_v(socket, vec, 1, proc, param);
}

int aio_socket_recvmsg_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvmsg proc, void* param)
{
	DWORD bytes = 0;
	DWORD flags = 0;
	struct aio_context* ctx = (struct aio_context*)socket;
	struct aio_context_action* aio;
	WSAMSG *wsamsg;

	aio = util_alloc(ctx);
	aio->action = iocp_recvmsg;
	aio->recvmsg.proc = proc;
	aio->recvmsg.param = param;

	wsamsg = &aio->recvmsg.wsamsg;
	memset(wsamsg, 0, sizeof(*wsamsg));
	wsamsg->name = (struct sockaddr*)&aio->recvmsg.peer;
	wsamsg->namelen = sizeof(aio->recvmsg.peer);
	wsamsg->lpBuffers = vec;
	wsamsg->dwBufferCount = n;
	wsamsg->Control.buf = aio->recvmsg.control;
	wsamsg->Control.len = sizeof(aio->recvmsg.control);
	wsamsg->dwFlags = flags;

	assert(0 == (AIO_READ & InterlockedOr(&ctx->flags, AIO_READ)));
	if (SOCKET_ERROR == WSARecvMsg(ctx->socket, wsamsg, &bytes, &aio->overlapped, NULL))
	{
		return aio_socket_result(aio, AIO_READ);
	}
	return 0;
}

int aio_socket_sendmsg(aio_socket_t socket, const struct sockaddr* peer, socklen_t peerlen, const struct sockaddr* local, socklen_t locallen, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	socket_bufvec_t vec[1];
	vec[0].buf = (CHAR FAR*)buffer;
	vec[0].len = bytes;
	return aio_socket_sendmsg_v(socket, peer, peerlen, local, locallen, vec, 1, proc, param);
}

int aio_socket_sendmsg_v(aio_socket_t socket, const struct sockaddr* peer, socklen_t peerlen, const struct sockaddr* local, socklen_t locallen, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	DWORD bytes = 0;
	DWORD flags = 0;
	WSAMSG wsamsg;
	WSACMSGHDR* cmsg;
	char control[64];
	struct in_pktinfo* pktinfo;
	struct in6_pktinfo* pktinfo6;
	struct aio_context* ctx = (struct aio_context*)socket;
	struct aio_context_action* aio;

	aio = util_alloc(ctx);
	aio->action = iocp_send;
	aio->send.proc = proc;
	aio->send.param = param;

	memset(control, 0, sizeof(control));
	memset(&wsamsg, 0, sizeof(wsamsg));
	wsamsg.name = (LPSOCKADDR)peer;
	wsamsg.namelen = peerlen;
	wsamsg.lpBuffers = (LPWSABUF)vec;
	wsamsg.dwBufferCount = n;
	wsamsg.Control.buf = control;
	wsamsg.Control.len = sizeof(control);
	wsamsg.dwFlags = 0;

	cmsg = WSA_CMSG_FIRSTHDR(&wsamsg);
	if (AF_INET == local->sa_family && locallen >= sizeof(struct sockaddr_in))
	{
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in_pktinfo));
		pktinfo = (struct in_pktinfo*)WSA_CMSG_DATA(cmsg);
		memset(pktinfo, 0, sizeof(struct in_pktinfo));
		memcpy(&pktinfo->ipi_addr, &((struct sockaddr_in*)local)->sin_addr, sizeof(pktinfo->ipi_addr));
		wsamsg.Control.len = WSA_CMSG_SPACE(sizeof(struct in_pktinfo));
	}
	else if (AF_INET6 == local->sa_family && locallen >= sizeof(struct sockaddr_in6))
	{
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;
		cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in6_pktinfo));
		pktinfo6 = (struct in6_pktinfo*)WSA_CMSG_DATA(cmsg);
		memset(pktinfo6, 0, sizeof(struct in6_pktinfo));
		memcpy(&pktinfo6->ipi6_addr, &((struct sockaddr_in6*)local)->sin6_addr, sizeof(pktinfo6->ipi6_addr));
		wsamsg.Control.len = WSA_CMSG_SPACE(sizeof(struct in6_pktinfo));
	}
	else
	{
		assert(0);
		return -1;
	}

	assert(0 == (AIO_READ & InterlockedOr(&ctx->flags, AIO_READ)));
	if (SOCKET_ERROR == WSASendMsg(ctx->socket, &wsamsg, flags, &bytes, &aio->overlapped, NULL))
	{
		return aio_socket_result(aio, AIO_READ);
	}
	return 0;
}
#endif
