#if defined(OS_WINDOWS)
#include "port/serial-port.h"
#include <Windows.h>

// MSDN
// CreateFile > Remarks > Communications Resources
//	 To specify a COM port number greater than 9, use the following syntax: "\\.\COM10". 

void* serial_port_open(const char *name)
{
	HANDLE h;
	h = CreateFileA(name, 
		GENERIC_READ | GENERIC_WRITE,
		0,				// must be opened with exclusive-access
		NULL,			// default security attributes
		OPEN_EXISTING,	// must use OPEN_EXISTING
		0,				// not overlapped I/O
		NULL);			// hTemplate must be NULL for comm devices

	if(INVALID_HANDLE_VALUE == h)
		return 0;

	return (void*)h;
}

int serial_port_close(void* port)
{
	HANDLE h = (HANDLE)port;
	CloseHandle(h);
	return 0;
}

int serial_port_flush(void* port)
{
	HANDLE h = (HANDLE)port;
	FlushFileBuffers(h);
	return 0;
}

static int serial_port_baud_rate(int baudrate)
{
	switch(baudrate)
	{
	case 110: return CBR_110;
	case 300: return CBR_300;
	case 600: return CBR_600;
	case 1200: return CBR_1200;
	case 2400: return CBR_2400;
	case 4800: return CBR_4800;
	case 9600: return CBR_9600;
	case 14400: return CBR_14400;
	case 19200: return CBR_19200;
	case 56000: return CBR_56000;
	case 57600: return CBR_57600;
	case 115200: return CBR_115200;
	case 128000: return CBR_128000;
	case 256000: return CBR_256000;
	default: return -1;
	}
}

int serial_port_setattr(void* port, int baudrate, int databits, int parity, int stopbits, int flowctrl)
{
	DCB dcb;
	HANDLE h = (HANDLE)port;

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	GetCommState(h, &dcb);

	dcb.fBinary = TRUE;

	dcb.BaudRate = serial_port_baud_rate(baudrate);
	if(-1 == dcb.BaudRate)
		return -1;

	switch(parity)
	{
	case SERIAL_PARITY_NONE:
		dcb.Parity = NOPARITY;
		break;
	case SERIAL_PARITY_ODD:
		dcb.Parity = ODDPARITY;
		break;
	case SERIAL_PARITY_EVEN:
		dcb.Parity = EVENPARITY;
		break;
	case SERIAL_PARITY_MARK:
		dcb.Parity = MARKPARITY;
		break;
	case SERIAL_PARITY_SPACE:
		dcb.Parity = SPACEPARITY;
		break;
	default:
		return -1;
	}

	switch(stopbits)
	{
	case SERIAL_STOPBITS_10:
		dcb.StopBits = ONESTOPBIT;
		break;
	case SERIAL_STOPBITS_15:
		dcb.StopBits = ONE5STOPBITS;
		break;
	case SERIAL_STOPBITS_20:
		dcb.StopBits = TWOSTOPBITS;
		break;
	default:
		return -1;
	}

	switch(databits)
	{
	case SERIAL_DATABITS_5:
		dcb.ByteSize = 5;
		break;
	case SERIAL_DATABITS_6:
		dcb.ByteSize = 6;
		break;
	case SERIAL_DATABITS_7:
		dcb.ByteSize = 7;
		break;
	case SERIAL_DATABITS_8:
		dcb.ByteSize = 8;
		break;
	case SERIAL_DATABITS_16:
		dcb.ByteSize = 16;
		break;
	default:
		return -1;
	}

	// MSDN > BuildCommDCB > Remarks
	switch(flowctrl)
	{
		// Hardware flow control uses the RTS/CTS (or DTR/DSR) signals to communicate.
	case SERIAL_FLOWCTRL_HARDWARE:
		dcb.fInX = FALSE;
		dcb.fOutX = FALSE;
		dcb.fOutxCtsFlow = TRUE;
		dcb.fOutxDsrFlow = TRUE;
		dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
		dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
		break;

	case SERIAL_FLOWCTRL_SOFTWARE:
		dcb.fInX = TRUE;
		dcb.fOutX = TRUE;
		dcb.fOutxCtsFlow = FALSE;
		dcb.fOutxDsrFlow = FALSE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fRtsControl = RTS_CONTROL_ENABLE;
		break;

	case SERIAL_FLOWCTRL_DISABLE:
		dcb.fInX = FALSE;
		dcb.fOutX = FALSE;
		dcb.fOutxCtsFlow = FALSE;
		dcb.fOutxDsrFlow = FALSE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fRtsControl = RTS_CONTROL_ENABLE;

	default:
		return -1;
	}

	if(!SetCommState(h, &dcb))
		return -1;
	return 0;
}

int serial_port_write(void* port, const void* data, int bytes)
{
	DWORD n = 0;
	HANDLE h = (HANDLE)port;

	if(!WriteFile(h, data, bytes, &n, NULL))
		return -1;

	return n;
}

int serial_port_read(void* port, void* data, int bytes)
{
	DWORD n = 0;
	HANDLE h = (HANDLE)port;

	if(!ReadFile(h, data, bytes, &n, NULL))
		return -1;

	return n;
}

#endif