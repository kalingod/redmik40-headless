typedef unsigned short u16;
typedef unsigned int u32;

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SHUT_WR 1

#define NR_CLOSE 57
#define NR_WRITE 64
#define NR_EXIT 93
#define NR_NANOSLEEP 101
#define NR_SOCKET 198
#define NR_BIND 200
#define NR_LISTEN 201
#define NR_SETSOCKOPT 208
#define NR_SHUTDOWN 210
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

static void write_fixed(int fd)
{
	static const char msg[] =
		"ALIOTH_RAW_DIAG_CONNECTED v1\n"
		"raw syscall tcp writer is alive\n";
	long left = (long)sizeof(msg) - 1;
	const char *p = msg;

	while (left > 0) {
		long n = sc3(NR_WRITE, fd, (long)p, left);

		if (n <= 0)
			return;
		p += n;
		left -= n;
	}
}

void _start(void)
{
	int one = 1;
	struct sockaddr_in addr;
	long srv;

	addr.family = AF_INET;
	addr.port = 0x7809;
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
		write_fixed((int)client);
		sleep_seconds(5);
		sc3(NR_SHUTDOWN, client, SHUT_WR, 0);
		sc3(NR_CLOSE, client, 0, 0);
	}
}
