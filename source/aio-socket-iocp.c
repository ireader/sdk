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

static HANDLE s_iocp = INVALID_HANDLE_VALUE;
static struct OverlpappedList s_overlappeds;

static DWORD system_cpu()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

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
	assert(INVALID_HANDLE_VALUE == s_iocp);
	s_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, system_cpu());
	return NULL==s_iocp ? GetLastError() : 0;
}

static int iocp_destroy()
{
	if(INVALID_HANDLE_VALUE != s_iocp)
		CloseHandle(s_iocp);
	return 0;
}

static int iocp_connect(aio_socket_t socket, const char* ip, int port)
{
	DWORD dwBytes = 0;
	WSAOVERLAPPED overlapped;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	memset(&overlapped, 0, sizeof(overlapped));
	if(!ConnectEx(socket, &addr, sizeof(addr), NULL, 0, &dwBytes, &overlapped))
	{
		DWORD ret = WSAGetLastError();
		if(ERROR_IO_PENDING == ret)
		{
		}
		else
		{
			return ret;
		}
	}
	return 0;
}

static int iocp_disconnect(aio_socket_t socket)
{
	WSAOVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(overlapped));
	if(!DisconnectEx(socket, &overlapped, 0, 0))
	{
		DWORD ret = WSAGetLastError();
		if(ERROR_IO_PENDING == ret)
		{
		}
		else
		{
			return ret;
		}
	}
	return 0;
}

static int iocp_recv(aio_socket_t socket)
{
	DWORD dwBytes = 0;
	WSAOVERLAPPED overlapped;
	WSABUF wsa[1];
	wsa[0].buf = 0;
	wsa[0].len = 0;
	memset(&overlapped, 0, sizeof(overlapped));
	if(SOCKET_ERROR == WSARecv(socket, wsa, 1, &dwBytes, 0, &overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING == ret)
		{
		}
		else
		{
			return ret;
		}
	}

	PostQueuedCompletionStatus();
}

static int iocp_send()
{
	DWORD dwBytes = 0;
	WSAOVERLAPPED overlapped;
	WSABUF wsa[1];
	wsa[0].buf = 0;
	wsa[0].len = 0;
	memset(&overlapped, 0, sizeof(overlapped));
	if(SOCKET_ERROR == WSASend(socket, wsa, 1, &dwBytes, 0, &overlapped, NULL))
	{
		DWORD ret = WSAGetLastError();
		if(WSA_IO_PENDING == ret)
		{
		}
		else
		{
			return ret;
		}
	}
}

int iocp_process()
{
	DWORD bytes;
	ULONG completionKey;
	OVERLAPPED *pOverlapped;

	if(GetQueuedCompletionStatus(s_iocp, &bytes, &completionKey, &pOverlapped, INFINITE))
	{
		if(0 == completionKey)
		{
			// start accept
			switch(bytes)
			{
			case POST_IO_ACCEPT:
				Accept();
				break;
			case POST_IO_READ:
				break;
			case POST_IO_WRITE:
				break;
			case POST_IO_DISCONNECT:
				break;
			default:
				printf("post io unknow action\n");
			}
		}
		else
		{
			// action
		}
	}
	else
	{
		DWORD err = GetLastError();
		if(NULL==pOverlapped)
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
			.
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

	return iocp_init();
}

int aio_socket_clean()
{
	return WSACleanup();
}

int aio_socket_accept(aio_socket_t socket)
{
	WSAOVERLAPPED overlapped;
	SOCKET client;
	DWORD dwBytes = sizeof(struct sockaddr_in)+16;

	memset(&overlapped, 0, sizeof(overlapped));
	client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!AcceptEx(socket, client, buffer, 0, dwBytes, dwBytes, &dwBytes, &overlapped))
	{
		DWORD ret = WSAGetLastError();
		if(ERROR_IO_PENDING == ret)
		{
			return 0;
		}
		else
		{
			closesocket(client);
			return ret;
		}
	}

	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms737524%28v=vs.85%29.aspx
	// When the AcceptEx function returns, 
	// the socket sAcceptSocket is in the default state for a connected socket. 
	// The socket sAcceptSocket does not inherit the properties of the socket associated 
	// with sListenSocket parameter until SO_UPDATE_ACCEPT_CONTEXT is set on the socket
	setsockopt(client, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&socket, sizeof(socket));

	return 0;
}

int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param)
{
	PostQueuedCompletionStatus(s_iocp, POST_IO_CONNECT, 0, NULL);
}

int aio_socket_disconnect(aio_socket_t socket, aio_ondisconnect proc, void* param)
{
	PostQueuedCompletionStatus(s_iocp, POST_IO_DISCONNECT, 0, NULL);
}

int aio_socket_recv(aio_socket_t socket, void* buffer, int bytes, aio_onrecv proc, void* param)
{
	PostQueuedCompletionStatus(s_iocp, POST_IO_READ, 0, NULL);
}

int aio_socket_send(aio_socket_t socket, const void* buffer, int bytes, aio_onsend proc, void* param)
{
	PostQueuedCompletionStatus(s_iocp, POST_IO_WRITE, 0, NULL);
}

int aio_socket_send_v(aio_socket_t socket, const socket_bufvec_t* vec, size_t n, aio_onsend proc, void* param)
{
	PostQueuedCompletionStatus(s_iocp, POST_IO_READ_VEC, 0, NULL);
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, size_t n, aio_onrecv proc, void* param)
{
	PostQueuedCompletionStatus(s_iocp, POST_IO_WRITE_VEC, 0, NULL);
}
