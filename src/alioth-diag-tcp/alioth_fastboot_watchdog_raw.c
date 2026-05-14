typedef unsigned int u32;

#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_MAGIC2 672274793
#define LINUX_REBOOT_CMD_RESTART2 0xa1b2c3d4

#define NR_EXIT 93
#define NR_NANOSLEEP 101
#define NR_REBOOT 142
#define NR_SYNC 81

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
	while (seconds > 0) {
		struct timespec ts;
		long chunk = seconds > 30 ? 30 : seconds;

		ts.tv_sec = chunk;
		ts.tv_nsec = 0;
		sc3(NR_NANOSLEEP, (long)&ts, 0, 0);
		seconds -= chunk;
	}
}

static long parse_seconds(const char *s)
{
	long v = 0;

	if (!s)
		return 90;
	while (*s >= '0' && *s <= '9') {
		v = v * 10 + (*s - '0');
		s++;
	}
	if (v < 10)
		v = 10;
	if (v > 3600)
		v = 3600;
	return v;
}

void _start(void)
{
	long *sp;
	long argc;
	char **argv;
	long seconds = 90;
	static const char mode[] = "bootloader";

	__asm__ volatile("mov %0, sp" : "=r"(sp));
	argc = sp[0];
	argv = (char **)(sp + 1);
	if (argc > 1)
		seconds = parse_seconds(argv[1]);

	sleep_seconds(seconds);
	sc3(NR_SYNC, 0, 0, 0);
	sleep_seconds(1);
	sc6(NR_REBOOT, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
	    LINUX_REBOOT_CMD_RESTART2, (long)mode, 0, 0);
	sc3(NR_EXIT, 0, 0, 0);
}
