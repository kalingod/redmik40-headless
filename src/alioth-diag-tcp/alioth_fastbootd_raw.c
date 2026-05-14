typedef unsigned short u16;
typedef unsigned int u32;

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SHUT_WR 1

#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_MAGIC2 672274793
#define LINUX_REBOOT_CMD_RESTART2 0xa1b2c3d4

#define NR_CLOSE 57
#define NR_READ 63
#define NR_WRITE 64
#define NR_EXIT 93
#define NR_NANOSLEEP 101
#define NR_REBOOT 142
#define NR_SOCKET 198
#define NR_BIND 200
#define NR_LISTEN 201
#define NR_SETSOCKOPT 208
#define NR_SHUTDOWN 210
#define NR_SYNC 81
#define NR_ACCEPT4 242

struct sockaddr_in {
	u16 family;
	u16 port;
	u32 addr;
	unsigned char zero[8];
};

struct timespec {
	long tv_sec;
	long tv_nsec;
};

static long sc6(long n, long a, long b, long c, long d, long e, long f)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;
	register long x8 __asm__("x8") = n;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
			 : "memory");
	return x0;
}

static long sc3(long n, long a, long b, long c)
{
	return sc6(n, a, b, c, 0, 0, 0);
}

static void sleep_seconds(long seconds)
{
	struct timespec ts;

	ts.tv_sec = seconds;
	ts.tv_nsec = 0;
	sc3(NR_NANOSLEEP, (long)&ts, 0, 0);
}

static void write_all(int fd, const char *p, long left)
{
	while (left > 0) {
		long n = sc3(NR_WRITE, fd, (long)p, left);

		if (n <= 0)
			return;
		p += n;
		left -= n;
	}
}

static long len(const char *s)
{
	long n = 0;

	while (s[n])
		n++;
	return n;
}

static int has_token(const char *buf, long n, const char *tok)
{
	long m = len(tok);

	if (m <= 0 || n < m)
		return 0;
	for (long i = 0; i <= n - m; i++) {
		long j;

		for (j = 0; j < m; j++) {
			char a = buf[i + j];
			char b = tok[j];

			if (a >= 'A' && a <= 'Z')
				a = (char)(a + 'a' - 'A');
			if (a != b)
				break;
		}
		if (j == m)
			return 1;
	}
	return 0;
}

static void reboot_bootloader(void)
{
	static const char mode[] = "bootloader";

	sc3(NR_SYNC, 0, 0, 0);
	sleep_seconds(1);
	sc6(NR_REBOOT, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
	    LINUX_REBOOT_CMD_RESTART2, (long)mode, 0, 0);
}

static void handle_client(int fd)
{
	char buf[128];
	long n;
	static const char banner[] =
		"ALIOTH_FASTBOOTD_RAW v1\n"
		"send: fastboot\\n\n";
	static const char ok[] = "rebooting to bootloader\n";
	static const char no[] = "ignored: expected fastboot or bootloader\n";

	write_all(fd, banner, sizeof(banner) - 1);
	n = sc3(NR_READ, fd, (long)buf, sizeof(buf));
	if (n > 0 && (has_token(buf, n, "fastboot") || has_token(buf, n, "bootloader"))) {
		write_all(fd, ok, sizeof(ok) - 1);
		sc3(NR_SHUTDOWN, fd, SHUT_WR, 0);
		sc3(NR_CLOSE, fd, 0, 0);
		reboot_bootloader();
		sleep_seconds(10);
		return;
	}
	write_all(fd, no, sizeof(no) - 1);
	sleep_seconds(1);
	sc3(NR_SHUTDOWN, fd, SHUT_WR, 0);
	sc3(NR_CLOSE, fd, 0, 0);
}

void _start(void)
{
	int one = 1;
	struct sockaddr_in addr;
	long srv;

	addr.family = AF_INET;
	addr.port = 0xdd09;
	addr.addr = 0;
	for (int i = 0; i < 8; i++)
		addr.zero[i] = 0;

	srv = sc3(NR_SOCKET, AF_INET, SOCK_STREAM, 0);
	if (srv < 0)
		sc3(NR_EXIT, 10, 0, 0);
	sc6(NR_SETSOCKOPT, srv, SOL_SOCKET, SO_REUSEADDR, (long)&one, sizeof(one), 0);
	if (sc3(NR_BIND, srv, (long)&addr, sizeof(addr)) < 0)
		sc3(NR_EXIT, 11, 0, 0);
	if (sc3(NR_LISTEN, srv, 4, 0) < 0)
		sc3(NR_EXIT, 12, 0, 0);

	for (;;) {
		long client = sc6(NR_ACCEPT4, srv, 0, 0, 0, 0, 0);

		if (client < 0) {
			sleep_seconds(1);
			continue;
		}
		handle_client((int)client);
	}
}
