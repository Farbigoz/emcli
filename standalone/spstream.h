#ifndef __SERIAL_PORT_STREAM_H__
#define __SERIAL_PORT_STREAM_H__

#include <stddef.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "paralstream.h"


class SerialPortStream : public ParallelStream {
public:
	typedef enum {
		SPEED_57600 = B57600,
		SPEED_115200 = B115200,
		SPEED_230400 = B230400,
		SPEED_460800 = B460800,
		SPEED_500000 = B500000,
		SPEED_576000 = B576000,
		SPEED_921600 = B921600,
		SPEED_1000000 = B1000000,
		SPEED_1152000 = B1152000,
		SPEED_1500000 = B1500000,
		SPEED_2000000 = B2000000,
		SPEED_2500000 = B2500000,
		SPEED_3000000 = B3000000,
		SPEED_3500000 = B3500000,
		SPEED_4000000 = B4000000,
	} TSpeed;

private:
	int fd;
	int prevTimeout;

	int initInterface(TSpeed speed) {
		struct termios tty;
		if (tcgetattr (fd, &tty) != 0)
		{
			printf("error %d from tcgetattr", errno);
			return -1;
		}

		cfsetospeed (&tty, speed);
		cfsetispeed (&tty, speed);

		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
		// disable IGNBRK for mismatched speed tests; otherwise receive break
		// as \000 chars
		tty.c_iflag &= ~IGNBRK;         // disable break processing
		tty.c_lflag = 0;                // no signaling chars, no echo,
		// no canonical processing
		tty.c_oflag = 0;                // no remapping, no delays
		tty.c_cc[VMIN]  = 0;            // read doesn't block
		tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

		tty.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | IGNCR | ICRNL); // shut off xon/xoff ctrl

		tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
		// enable reading
		tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
		tty.c_cflag |= 0; // 8n1
		tty.c_cflag &= ~CSTOPB;
		tty.c_cflag &= ~CRTSCTS;

		if (tcsetattr (fd, TCSANOW, &tty) != 0)
		{
			printf("error %d from tcsetattr", errno);
			return -1;
		}
		return 0;
	}

	void setBlocking(bool block, int timeout) {
		struct termios tty;
		memset(&tty, 0, sizeof tty);
		if (tcgetattr (fd, &tty) != 0)
		{
			printf("error %d from tggetattr", errno);
			return;
		}

		tty.c_cc[VMIN]  = block ? 1 : 0;
		tty.c_cc[VTIME] = timeout;

		if (tcsetattr (fd, TCSANOW, &tty) != 0)
			printf("error %d setting term attributes", errno);
	}

public:
	SerialPortStream(const char *device, TSpeed speed) {
		fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);

		if (fd < 0) {
			printf("Error %i from open: %s\n", errno, strerror(errno));
			return;
		}

		prevTimeout = 500;

		initInterface(speed);
		setBlocking(false, prevTimeout / 100);
	}

	~SerialPortStream() {
		if (fd >= 0)
			close(fd);
	}

	int Write(const char *p, size_t len) final {
		return write(fd, p, len);
	}

	int WriteByte(char c) final {
		return write(fd, &c, 1);
	}

	int ReadByte(char *c, size_t timeoutMs) final {
		if (timeoutMs != prevTimeout) {
			prevTimeout = timeoutMs;
			setBlocking(false, prevTimeout / 100);
		}

		return read(fd, c, 1);
	}

};



#endif /* __SERIAL_PORT_STREAM_H__ */