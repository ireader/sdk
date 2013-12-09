#include "aio-socket.h"
#include "thread-pool.h"
#include <Windows.h>
#include <WinSock2.h>
#include <assert.h>

typedef BOOL (PASCAL FAR * FAcceptEx)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
typedef VOID (PASCAL FAR * FGetAcceptExSockaddrs)(PVOID, DWORD, DWORD, DWORD, struct sockaddr **, LPINT, struct sockaddr **, LPINT);
typedef BOOL (PASCAL FAR * FConnectEx)(SOCKET, const struct sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL (PASCAL FAR * FDisconnectEx)(SOCKET, LPOVERLAPPED, DWORD, DWORD);

#define WSAID_ACCEPTEX		{0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#define WSAID_CONNECTEX		{0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
#define WSAID_DISCONNECTEX	{0x7fda2e11,0x8630,0x436f,{0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57}}

static FAcceptEx AcceptEx;
static FGetAcceptExSockaddrs GetAcceptExSockaddrs;
static FConnectEx ConnectEx;
static FDisconnectEx DisconnectEx;

enum {
	POST_IO_MSG = 1,
	POST_IO_ACCEPT,
	POST_IO_CONNECT,
	POST_IO_DISCONNECT,
	POST_IO_READ,
	POST_IO_WRITE,
	POST_IO_READ_VEC,
	POST_IO_WRITE_VEC,
};

struct aio_context_accept
{
	WSAOVERLAPPED overlapped;
	aio_onaccept proc;
	void* param;
	
	SOCKET socket;
	char buffer[(sizeof(sockaddr_in)+16)*2];
};

struct aio_context_connect
{
	WSAOVERLAPPED overlapped;
	aio_onconnect proc;
	void* param;
};

struct aio_context_recv
{
	WSAOVERLAPPED overlapped;
	aio_onrecv proc;
	void* param;
};

struct aio_context_send
{
	WSAOVERLAPPED overlapped;
	aio_onsend proc;
	void* param;
};

struct aio_context
{
	int own;
	SOCKET socket;
	aio_context_accept accept;
	aio_context_connect connect;
	aio_context_recv recv;
	aio_context_send send;
};

static int s_processors = 0; // cpu count
static HANDLE s_iocp = INVALID_HANDLE_VALUE;

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

static int iocp_create()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	s_processors = sysinfo.dwNumberOfProcessors;

	assert(INVALID_HANDLE_VALUE == s_iocp);
	s_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, s_processors);
	return NULL==s_iocp ? GetLastError() : 0;
}

static int iocp_destroy()
{
	int i;
	if(INVALID_HANDLE_VALUE != s_iocp)
	{
		for(i = 0; i < s_processors; i++)
			PostQueuedCompletionStatus(s_iocp, 0, 0, NULL); // notify to exit
		CloseHandle(s_iocp);
	}
	return 0;
}

static int iocp_bind(SOCKET socket, ULONG_PTR key)
{
	HANDLE iocp;
	if(INVALID_HANDLE_VALUE == s_iocp)
		return -1;

	iocp = CreateIoCompletionPort(socket, s_iocp, key, 0);
	assert(iocp == s_iocp);
	return 0;
}

static int STDCALL iocp_process()
{
	DWORD bytes;
	ULONG completionKey;
	OVERLAPPED *pOverlapped;
	aio_context *aio;

	if(GetQueuedCompletionStatus(s_iocp, &bytes, &completionKey, &pOverlapped, INFINITE))
	{
		if(0 == completionKey)
		{
			return 0;
		}
		else
		{
			// action
			aio = (aio_context*)completionKey;
		}
	}
	else
	{
		DWORD err = GetLastError();
		if(NULL==*pOverlapped)
		{
			if(WAIT_TIMEOUT == err)
			{
				// timeout
				assert(false);
			}
			else
			{
				// exception
			}
		}
		else
		{
			// io failed
		}
	}

	return 0;
}

int aio_socket_init()
{
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);

	iocp_init();
	return iocp_create();
}

int aio_socket_clean()
{
	iocp_destroy();
	return WSACleanup();
}

aio_socket_t aio_socket_create(socket_t socket, int own)
{
	aio_context *aio = NULL;
	aio = (aio_context*)malloc(sizeof(aio_context));
	if(!aio)
		return NULL;

	memset(aio, 0, sizeof(aio_context));
	aio->socket = socket;
	aio->own = own;

	if(0 != iocp_bind(aio->socket, aio))
	{
		free(aio);
		return NULL;
	}

	return aio;
}

int aio_socket_close(aio_socket_t socket)
{
	aio_context *aio = (aio_context*)socket;
	if(aio->own)
		closesocket(aio->socket);
	free(aio);
	return 0;
}

int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param)
{
	aio_context *aio = (aio_context*)socket;
	DWORD dwBytes = sizeof(struct sockaddr_in)+16;
	struct sockaddr_in local;
	struct sockaddr_in remote;
	char ip[16];

	aio->accept.proc = proc;
	aio->accept.param = param;
	aio->accept.socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(!AcceptEx(socket, aio->accept.socket, aio->accept.buffer, 0, dwBytes, dwBytes, &dwBytes, &aio->accept.overlapped))
	{
		DWORD ret = WSAGetLastError();
		if(ERROR_IO_PENDING == ret)
		{
			return 0;
		}
		else
		{
			closesocket(aio->accept.socket);
			return ret;
		}
	}

	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms737524%28v=vs.85%29.aspx
	// When the AcceptEx function returns, 
	// the socket sAcceptSocket is in the default state for a connected socket. 
	// The socket sAcceptSocket does not inherit the properties of the socket associated 
	// with sListenSocket parameter until SO_UPDATE_ACCEPT_CONTEXT is set on the socket
	setsockopt(aio->accept.socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&socket, sizeof(socket));

	dwBytes = sizeof(struct sockaddr_in)+16;
	GetAcceptExSockaddrs(aio->accept.buffer, 0, dwBytes, dwBytes, &local, &dwBytes, &remote, &dwBytes);

	sprintf(ip, "%d.%d.%d.%d", addr.sin_addr.s_net, addr.sin_addr.s_host, addr.sin_addr.s_lh, addr.sin_addr.s_impno);
	proc(param, 0, aio->accept.socket, ip, ntohs(remote.sin_port));
	return 0;
}

int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param)
{
	aio_context *aio = (aio_context*)socket;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	aio->connect.proc = proc;
	aio->connect.param = param;
	if(!ConnectEx(aio->socket, addr, sizeof(addr), NULL, 0, NULL, &aio->connect.overlapped))
	{
		DWORD ret = WSAGetLastError();
		if(ERROR_IO_PENDING == ret)
		{
			return 0;
		}
		else
		{
			return ret;
		}
	}

	proc(param, 0);
	return 0;
}

int aio_socket_recv(aio_socket_t socket, void* buffer, int bytes, aio_onrecv proc, void* param)
{
	DWORD dwBytes;
	WSABUF wsa[1];
	aio_context *aio;

	wsa[0].buf = buffer;
	wsa[0].len = bytes;

	aio = (aio_context*)socket;
	aio->recv.proc = proc;
	aio->recv.param = param;

	if(SOCKET_ERROR == WSARecv(socket, wsa, 1, &dwBytes, 0, &aio->recv.overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING == ret)
		{
			return 0;
		}
		else
		{
			return ret;
		}
	}

	proc(param, 0, dwBytes);
	return 0;
}

int aio_socket_send(aio_socket_t socket, const void* buffer, int bytes, aio_onsend proc, void* param)
{
	DWORD dwBytes;
	WSABUF wsa[1];
	aio_context *aio;

	wsa[0].buf = buffer;
	wsa[0].len = bytes;

	aio = (aio_context*)socket;
	aio->send.proc = proc;
	aio->send.param = param;

	if(SOCKET_ERROR == WSASend(socket, wsa, 1, &dwBytes, 0, &aio->send.overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING == ret)
		{
			return 0;
		}
		else
		{
			return ret;
		}
	}
	
	proc(param, 0, dwBytes);
	return 0;
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, size_t n, aio_onrecv proc, void* param)
{
	DWORD dwBytes = 0;
	aio_context *aio = (aio_context*)socket;

	aio->recv.proc = proc;
	aio->recv.param = param;

	if(SOCKET_ERROR == WSARecv(socket, vec, n, &dwBytes, 0, &aio->recv.overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING == ret)
		{
			return 0;
		}
		else
		{
			return ret;
		}
	}

	proc(param, 0, dwBytes);
	return 0;
}

int aio_socket_send_v(aio_socket_t socket, const socket_bufvec_t* vec, size_t n, aio_onsend proc, void* param)
{
	DWORD dwBytes = 0;
	aio_context *aio = (aio_context*)socket;

	aio->send.proc = proc;
	aio->send.param = param;

	if(SOCKET_ERROR == WSASend(socket, vec, n, &dwBytes, 0, &aio->send.overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING == ret)
		{
			return 0;
		}
		else
		{
			return ret;
		}
	}

	proc(param, 0, dwBytes);
	return 0;
}
