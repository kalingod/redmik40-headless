#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#ifndef SYSLOG_ACTION_READ_ALL
#define SYSLOG_ACTION_READ_ALL 3
#endif
#ifndef SYSLOG_ACTION_SIZE_BUFFER
#define SYSLOG_ACTION_SIZE_BUFFER 10
#endif

#define MAX_PACKET 1100

static int udp_fd = -1;
static int kmsg_fd = -1;
static int pmsg_fd = -1;
static struct sockaddr_in udp_addr;
static const char *tag = "alioth";
static unsigned long seqno;

static void write_all(int fd, const char *buf, size_t len)
{
	while (len > 0) {
		ssize_t n = write(fd, buf, len);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return;
		}
		if (n == 0)
			return;
		buf += n;
		len -= (size_t)n;
	}
}

static void emit_raw(const char *level, const char *msg)
{
	char packet[MAX_PACKET + 256];
	int n;

	n = snprintf(packet, sizeof(packet),
		     "ALIOTHLOG seq=%lu tag=%s level=%s %s\n",
		     ++seqno, tag, level, msg);
	if (n < 0)
		return;
	if (n >= (int)sizeof(packet))
		n = (int)sizeof(packet) - 1;

	if (udp_fd >= 0)
		sendto(udp_fd, packet, (size_t)n, 0,
		       (struct sockaddr *)&udp_addr, sizeof(udp_addr));
	if (kmsg_fd >= 0) {
		write_all(kmsg_fd, "alioth-logd: ", 13);
		write_all(kmsg_fd, msg, strlen(msg));
		write_all(kmsg_fd, "\n", 1);
	}
	if (pmsg_fd >= 0) {
		write_all(pmsg_fd, packet, (size_t)n);
	}
}

static void emitf(const char *level, const char *fmt, ...)
{
	char msg[MAX_PACKET];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n >= (int)sizeof(msg))
		msg[sizeof(msg) - 1] = '\0';
	emit_raw(level, msg);
}

static void emit_data_chunk(const char *label, const char *data, size_t len)
{
	size_t off = 0;

	if (len == 0) {
		emitf("data", "%s [empty]", label);
		return;
	}
	while (off < len) {
		char msg[MAX_PACKET];
		size_t room;
		size_t n;
		int prefix;

		prefix = snprintf(msg, sizeof(msg), "%s offset=%zu data=", label, off);
		if (prefix < 0 || prefix >= (int)sizeof(msg))
			return;
		room = sizeof(msg) - (size_t)prefix - 1;
		n = len - off;
		if (n > room)
			n = room;
		memcpy(msg + prefix, data + off, n);
		for (size_t i = 0; i < n; i++) {
			char *c = msg + prefix + i;

			if (*c == '\0')
				*c = ' ';
			else if (*c == '\n' || *c == '\r' || *c == '\t')
				*c = ' ';
			else if ((unsigned char)*c < 32)
				*c = '.';
		}
		msg[prefix + n] = '\0';
		emit_raw("data", msg);
		off += n;
	}
}

static void emit_file(const char *label, const char *path, size_t limit)
{
	int fd;
	char *buf;
	ssize_t n;
	size_t cap = limit ? limit : 65536;

	if (cap > 262144)
		cap = 262144;
	emitf("info", "file label=%s path=%s", label, path);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		emitf("warn", "open_failed label=%s path=%s errno=%d %s",
		      label, path, errno, strerror(errno));
		return;
	}
	buf = calloc(1, cap + 1);
	if (!buf) {
		emitf("warn", "calloc_failed label=%s size=%zu", label, cap);
		close(fd);
		return;
	}
	n = read(fd, buf, cap);
	if (n < 0) {
		emitf("warn", "read_failed label=%s path=%s errno=%d %s",
		      label, path, errno, strerror(errno));
	} else {
		emit_data_chunk(label, buf, (size_t)n);
	}
	free(buf);
	close(fd);
}

static void emit_dir_files(const char *label, const char *path, size_t per_file_limit)
{
	DIR *dir;
	struct dirent *de;
	int count = 0;

	emitf("info", "dir label=%s path=%s", label, path);
	dir = opendir(path);
	if (!dir) {
		emitf("warn", "opendir_failed label=%s path=%s errno=%d %s",
		      label, path, errno, strerror(errno));
		return;
	}
	while ((de = readdir(dir)) != NULL) {
		char full[512];
		char item_label[256];

		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
		snprintf(item_label, sizeof(item_label), "%s/%s", label, de->d_name);
		emit_file(item_label, full, per_file_limit);
		count++;
	}
	closedir(dir);
	emitf("info", "dir_done label=%s count=%d", label, count);
}

static void emit_kmsg_tail(void)
{
	int size;
	char *buf;

	size = klogctl(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
	if (size <= 0) {
		emitf("warn", "klog_size_failed errno=%d %s", errno, strerror(errno));
		return;
	}
	if (size > 262144)
		size = 262144;
	buf = calloc(1, (size_t)size + 1);
	if (!buf) {
		emitf("warn", "klog_calloc_failed size=%d", size);
		return;
	}
	size = klogctl(SYSLOG_ACTION_READ_ALL, buf, size);
	if (size <= 0) {
		emitf("warn", "klog_read_failed errno=%d %s", errno, strerror(errno));
		free(buf);
		return;
	}
	if (size > 49152)
		emit_data_chunk("kmsg_tail", buf + size - 49152, 49152);
	else
		emit_data_chunk("kmsg_tail", buf, (size_t)size);
	free(buf);
}

static void emit_snapshot(const char *reason)
{
	struct utsname u;

	emitf("info", "snapshot_begin reason=%s pid=%ld", reason, (long)getpid());
	if (uname(&u) == 0) {
		emitf("info", "uname sys=%s node=%s release=%s version=%s machine=%s",
		      u.sysname, u.nodename, u.release, u.version, u.machine);
	}
	mkdir("/sys/fs/pstore", 0755);
	if (mount("pstore", "/sys/fs/pstore", "pstore", 0, "") == 0)
		emit_raw("info", "mounted pstore at /sys/fs/pstore");
	else
		emitf("info", "mount_pstore_result errno=%d %s", errno, strerror(errno));
	emit_file("cmdline", "/proc/cmdline", 8192);
	emit_file("version", "/proc/version", 8192);
	emit_file("uptime", "/proc/uptime", 4096);
	emit_file("boot_id", "/proc/sys/kernel/random/boot_id", 4096);
	emit_file("pid1_cmdline", "/proc/1/cmdline", 4096);
	emit_file("mounts", "/proc/mounts", 32768);
	emit_file("ramoops_mem_address", "/sys/module/ramoops/parameters/mem_address", 4096);
	emit_file("ramoops_mem_size", "/sys/module/ramoops/parameters/mem_size", 4096);
	emit_file("ramoops_record_size", "/sys/module/ramoops/parameters/record_size", 4096);
	emit_dir_files("pstore", "/sys/fs/pstore", 8192);
	emit_kmsg_tail();
	emit_raw("info", "snapshot_end");
}

int main(int argc, char **argv)
{
	const char *host = "172.16.42.1";
	int port = 5514;
	int seconds = 180;

	if (argc > 1 && argv[1][0])
		host = argv[1];
	if (argc > 2 && argv[2][0])
		port = atoi(argv[2]);
	if (argc > 3 && argv[3][0])
		tag = argv[3];
	if (argc > 4 && argv[4][0])
		seconds = atoi(argv[4]);
	if (seconds < 10)
		seconds = 10;
	if (seconds > 3600)
		seconds = 3600;

	udp_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (udp_fd >= 0) {
		memset(&udp_addr, 0, sizeof(udp_addr));
		udp_addr.sin_family = AF_INET;
		udp_addr.sin_port = htons((uint16_t)port);
		inet_pton(AF_INET, host, &udp_addr.sin_addr);
	}
	kmsg_fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	pmsg_fd = open("/dev/pmsg0", O_WRONLY | O_CLOEXEC);

	emitf("info", "udp_logd_start host=%s port=%d tag=%s seconds=%d kmsg=%d pmsg=%d",
	      host, port, tag, seconds, kmsg_fd, pmsg_fd);
	emit_snapshot("start");
	for (int i = 0; i < seconds; i += 2) {
		emitf("heartbeat", "t=%d pid=%ld", i, (long)getpid());
		if (i == 20 || i == 60 || i == 120)
			emit_snapshot("interval");
		sleep(2);
	}
	emit_snapshot("final");
	emit_raw("info", "udp_logd_exit");
	return 0;
}
