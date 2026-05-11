#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifndef CRTSCTS
#define CRTSCTS 020000000000
#endif

#ifndef N_HCI
#define N_HCI 15
#endif

#ifndef HCIUARTSETPROTO
#define HCIUARTSETPROTO _IOW('U', 200, int)
#endif

#ifndef HCIUARTGETPROTO
#define HCIUARTGETPROTO _IOR('U', 201, int)
#endif

#define HCI_UART_QCA 8

static volatile sig_atomic_t stop_requested;

static void on_signal(int signo)
{
	(void)signo;
	stop_requested = 1;
}

static speed_t baud_constant(unsigned int speed)
{
	switch (speed) {
	case 115200:
		return B115200;
#ifdef B1000000
	case 1000000:
		return B1000000;
#endif
#ifdef B2000000
	case 2000000:
		return B2000000;
#endif
#ifdef B3000000
	case 3000000:
		return B3000000;
#endif
#ifdef B3200000
	case 3200000:
		return B3200000;
#endif
#ifdef B3500000
	case 3500000:
		return B3500000;
#endif
	default:
		return 0;
	}
}

static int set_uart(int fd, unsigned int speed, bool flow)
{
	struct termios tio;
	speed_t baud = baud_constant(speed);
	int modem;

	if (!baud) {
		fprintf(stderr, "unsupported baud rate %u\n", speed);
		return -1;
	}
	if (tcgetattr(fd, &tio) < 0) {
		perror("tcgetattr");
		return -1;
	}

	cfmakeraw(&tio);
	tio.c_cflag |= CLOCAL | CREAD;
	if (flow)
		tio.c_cflag |= CRTSCTS;
	else
		tio.c_cflag &= ~CRTSCTS;
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 0;

	if (cfsetispeed(&tio, baud) < 0 || cfsetospeed(&tio, baud) < 0) {
		perror("cfset*speed");
		return -1;
	}
	if (tcsetattr(fd, TCSANOW, &tio) < 0) {
		perror("tcsetattr");
		return -1;
	}

	if (ioctl(fd, TIOCMGET, &modem) == 0) {
		modem |= TIOCM_RTS | TIOCM_DTR;
		ioctl(fd, TIOCMSET, &modem);
	}

	tcflush(fd, TCIOFLUSH);
	return 0;
}

static void usage(const char *argv0)
{
	fprintf(stderr,
		"Usage: %s [--dev /dev/ttyHS0] [--speed 3000000] [--flow|--no-flow]\n",
		argv0);
}

int main(int argc, char **argv)
{
	const char *dev = "/dev/ttyHS0";
	unsigned int speed = 3000000;
	bool flow = true;
	int fd;
	int ldisc = N_HCI;
	int proto = HCI_UART_QCA;
	int reported = -1;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--dev") && i + 1 < argc) {
			dev = argv[++i];
		} else if (!strcmp(argv[i], "--speed") && i + 1 < argc) {
			speed = (unsigned int)strtoul(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "--flow")) {
			flow = true;
		} else if (!strcmp(argv[i], "--no-flow")) {
			flow = false;
		} else if (!strcmp(argv[i], "--help")) {
			usage(argv[0]);
			return 0;
		} else {
			usage(argv[0]);
			return 2;
		}
	}

	fd = open(dev, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror(dev);
		return 1;
	}

	if (set_uart(fd, speed, flow) < 0)
		return 1;

	if (ioctl(fd, TIOCSETD, &ldisc) < 0) {
		perror("TIOCSETD N_HCI");
		return 1;
	}
	if (ioctl(fd, HCIUARTSETPROTO, proto) < 0) {
		perror("HCIUARTSETPROTO QCA");
		return 1;
	}
	if (ioctl(fd, HCIUARTGETPROTO, &reported) == 0)
		printf("attached %s speed=%u flow=%s proto=%d\n",
		       dev, speed, flow ? "on" : "off", reported);
	else
		printf("attached %s speed=%u flow=%s proto=QCA\n",
		       dev, speed, flow ? "on" : "off");
	fflush(stdout);

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	while (!stop_requested)
		pause();

	printf("detaching %s\n", dev);
	fflush(stdout);
	close(fd);
	return 0;
}
