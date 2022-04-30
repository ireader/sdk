#if defined(OS_MAC) || defined(OS_LINUX)
#include "port/serial-port.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

// Reference:
// http://mirror.datenwolf.net/serial/

void* serial_port_open(const char *name)
{
	int fd = 0;

	fd = open(name, O_RDWR | O_NOCTTY | O_NDELAY);
	if(-1 == fd)
		return 0;

//	fcntl(fd, F_SETFL, FNDELAY); // no block
	fcntl(fd, F_SETFL, 0); // block
	return (void*)(intptr_t)fd;
}

int serial_port_close(void* port)
{
	int fd = (int)(intptr_t)port;
	return close(fd);
}

int serial_port_flush(void* port)
{
	int fd = (int)(intptr_t)port;
	return tcflush(fd, TCIOFLUSH);
}

static int serial_port_baud_rate(int baudrate)
{
	switch(baudrate)
	{
	case 0: return B0;
	case 50: return B50;
	case 75: return B75;
	case 110: return B110;
	case 134: return B134;
	case 150: return B150;
	case 200: return B200;
	case 300: return B300;
	case 600: return B600;
	case 1200: return B1200;
	case 1800: return B1800;
	case 2400: return B2400;
	case 4800: return B4800;
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 57600: return B57600;
//	case 76800: return B76800;
	case 115200: return B115200;
	case 230400: return B230400;
	default:
		return -1;
	}
}

int serial_port_setattr(void* port, int baudrate, int databits, int parity, int stopbits, int flowctrl)
{
	int fd = (int)(intptr_t)port;
	struct termios options;

	baudrate = serial_port_baud_rate(baudrate);
	if(-1 == baudrate)
		return -1;

	tcgetattr(fd, &options);
	options.c_cflag |= CLOCAL | CREAD;
	cfsetispeed(&options, baudrate);
	cfsetospeed(&options, baudrate);

	// data bits
	options.c_cflag &= ~CSIZE; /* Mask the character size bits */
	switch(databits)
	{
	case SERIAL_DATABITS_5:
		options.c_cflag |= CS5;
		break;
	case SERIAL_DATABITS_6:
		options.c_cflag |= CS6;
		break;
	case SERIAL_DATABITS_7:
		options.c_cflag |= CS7;
		break;
	case SERIAL_DATABITS_8:
		options.c_cflag |= CS8;
		break;
	default:
		return -1;
	}

	// parity
	switch(parity)
	{
	case SERIAL_PARITY_ODD:
		options.c_cflag |= PARENB;
		options.c_cflag |= PARODD;
		options.c_iflag |= INPCK | ISTRIP;
		break;

	case SERIAL_PARITY_EVEN:
		options.c_cflag |= PARENB;
		options.c_cflag &= ~PARODD;
		options.c_iflag |= INPCK | ISTRIP;
		break;

	case SERIAL_PARITY_NONE:
		options.c_cflag &= ~PARENB;
		options.c_iflag &= ~INPCK;
		break;

	default:
		return -1;
	}

	// stop bits
	switch(stopbits)
	{
	case SERIAL_STOPBITS_10:
		options.c_cflag &= ~CSTOPB;
		break;
	case SERIAL_STOPBITS_15:
		break;
	case SERIAL_STOPBITS_20:
		options.c_cflag |= CSTOPB;
		break;
	default:
		return -1;
	}

	// flow control
	switch(flowctrl)
	{
	case SERIAL_FLOWCTRL_HARDWARE:
		options.c_cflag |= CRTSCTS;
		options.c_iflag &= ~(IXON | IXOFF | IXANY); // disable software flow control
		break;
	case SERIAL_FLOWCTRL_SOFTWARE:
		options.c_cflag &= ~CRTSCTS;
		options.c_iflag |= (IXON | IXOFF | IXANY);
		break;
	default:
		return -1;
	}

	// raw input
	options.c_lflag = ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;

	// timeout
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 10; // 0-wait forever

	return tcsetattr(fd, TCSANOW, &options);
}

int serial_port_write(void* port, const void* data, int bytes)
{
	int fd = (int)(intptr_t)port;
	return (int)write(fd, data, bytes);
}

int serial_port_read(void* port, void* data, int bytes)
{
	int fd = (int)(intptr_t)port;
	return (int)read(fd, data, bytes);
}

#endif
