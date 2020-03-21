#ifndef _serial_port_h_
#define _serial_port_h_

enum ESERIAL_DATABITS
{
	SERIAL_DATABITS_5 = 0,
	SERIAL_DATABITS_6,
	SERIAL_DATABITS_7,
	SERIAL_DATABITS_8,
	SERIAL_DATABITS_16, // windows only
};

enum ESERIAL_PARITY 
{ 
	SERIAL_PARITY_NONE = 0,	// no parity
	SERIAL_PARITY_ODD,		// odd parity
	SERIAL_PARITY_EVEN,		// even parity
	SERIAL_PARITY_MARK,		// mark parity(windows only)
	SERIAL_PARITY_SPACE		// space parity(windows only)
};

enum ESERIAL_STOPBITS
{
	SERIAL_STOPBITS_10 = 0,	// 1 stop bits
	SERIAL_STOPBITS_15,		// 1.5 stop bits(windows only)
	SERIAL_STOPBITS_20		// 2 stop bits
};

enum ESERIAL_FLOWCONTROL
{
	SERIAL_FLOWCTRL_DISABLE = 0,// disable flow control
	SERIAL_FLOWCTRL_SOFTWARE,	// software flow control
	SERIAL_FLOWCTRL_HARDWARE	// hardware flow control
};

/// @param[in] name serial name, etc. /dev/ttyS0 or COM1 or \\.\COM10
/// @return NULL-error, other-ok
void* serial_port_open(const char *name);

int serial_port_close(void* port);

/// @return 0-ok, other-error
int serial_port_flush(void* port);

/// @param[in] parity 0-no parity, 1-even parity, 2-odd parity, 3-space parity, 4-mark parity
/// @param[in] stopbits 0-1 stop bits, 1-1.5 stop bits, 2-2 stop bits
/// @return 0-ok, other-error
int serial_port_setattr(void* port, int baudrate, int databits, int parity, int stopbits, int flowctrl);
//int serial_port_getattr(void* port, int *baudrate, int *databits, int *parity, int *stopbits, int *flowctrl);

int serial_port_read(void* port, void* data, int bytes);

/// @return -1-error, other-write data length
int serial_port_write(void* port, const void* data, int bytes);

#endif /* !_serial_port_h_ */
