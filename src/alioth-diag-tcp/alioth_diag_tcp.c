#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifndef SYSLOG_ACTION_READ_ALL
#define SYSLOG_ACTION_READ_ALL 3
#endif
#ifndef SYSLOG_ACTION_SIZE_BUFFER
#define SYSLOG_ACTION_SIZE_BUFFER 10
#endif

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

static void out(int fd, const char *fmt, ...)
{
	va_list ap;
	char buf[2048];
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if ((size_t)n >= sizeof(buf)) {
		write_all(fd, buf, sizeof(buf) - 1);
		write_all(fd, "\n[diag line truncated]\n", 23);
		return;
	}
	write_all(fd, buf, (size_t)n);
}

static void send_file(int fd, const char *label, const char *path, size_t limit)
{
	int in;
	char buf[4096];
	ssize_t n;
	size_t sent = 0;

	out(fd, "--- %s: %s ---\n", label, path);
	in = open(path, O_RDONLY | O_CLOEXEC);
	if (in < 0) {
		out(fd, "open failed: %s\n", strerror(errno));
		return;
	}
	while ((n = read(in, buf, sizeof(buf))) > 0) {
		if (limit && sent + (size_t)n > limit)
			n = (ssize_t)(limit - sent);
		for (ssize_t i = 0; i < n; i++) {
			if (buf[i] == '\0')
				buf[i] = ' ';
		}
		write_all(fd, buf, (size_t)n);
		sent += (size_t)n;
		if (limit && sent >= limit) {
			out(fd, "\n[truncated at %zu bytes]\n", limit);
			break;
		}
	}
	if (sent == 0)
		out(fd, "[empty]\n");
	out(fd, "\n");
	close(in);
}

static void list_dir(int fd, const char *label, const char *path)
{
	DIR *dir;
	struct dirent *de;
	struct stat st;
	char full[512];

	out(fd, "--- %s: %s ---\n", label, path);
	dir = opendir(path);
	if (!dir) {
		out(fd, "opendir failed: %s\n", strerror(errno));
		return;
	}
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
		if (stat(full, &st) == 0)
			out(fd, "%s %lld bytes\n", de->d_name, (long long)st.st_size);
		else
			out(fd, "%s stat failed: %s\n", de->d_name, strerror(errno));
	}
	closedir(dir);
}

static void send_dir_files(int fd, const char *label, const char *path, size_t per_file_limit)
{
	DIR *dir;
	struct dirent *de;
	char full[512];

	list_dir(fd, label, path);
	dir = opendir(path);
	if (!dir)
		return;
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
		send_file(fd, de->d_name, full, per_file_limit);
	}
	closedir(dir);
}

static void send_uname(int fd)
{
	struct utsname u;

	if (uname(&u) == 0) {
		out(fd, "uname: %s %s %s %s %s\n",
		    u.sysname, u.nodename, u.release, u.version, u.machine);
	}
}

static void send_kmsg_tail(int fd)
{
	int size;
	char *buf;

	out(fd, "--- kmsg tail ---\n");
	size = klogctl(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
	if (size <= 0) {
		out(fd, "klog size failed: %s\n", strerror(errno));
		return;
	}
	if (size > 262144)
		size = 262144;
	buf = calloc(1, (size_t)size + 1);
	if (!buf) {
		out(fd, "calloc failed\n");
		return;
	}
	size = klogctl(SYSLOG_ACTION_READ_ALL, buf, size);
	if (size <= 0) {
		out(fd, "klog read failed: %s\n", strerror(errno));
		free(buf);
		return;
	}
	if (size > 32768)
		write_all(fd, buf + size - 32768, 32768);
	else
		write_all(fd, buf, (size_t)size);
	out(fd, "\n");
	free(buf);
}

static void send_report(int fd)
{
	write_all(fd, "ALIOTH_DIAG_CONNECTED v2\n", 25);
	out(fd, "=== alioth diag tcp v2 ===\n");
	send_uname(fd);
	send_file(fd, "cmdline", "/proc/cmdline", 0);
	send_file(fd, "version", "/proc/version", 0);
	send_file(fd, "uptime", "/proc/uptime", 0);
	send_file(fd, "boot_id", "/proc/sys/kernel/random/boot_id", 0);
	send_file(fd, "pid1 cmdline", "/proc/1/cmdline", 4096);
	send_file(fd, "mounts", "/proc/mounts", 32768);
	send_dir_files(fd, "ramoops parameters", "/sys/module/ramoops/parameters", 4096);
	send_dir_files(fd, "pstore", "/sys/fs/pstore", 8192);
	send_file(fd, "iomem", "/proc/iomem", 32768);
	send_kmsg_tail(fd);
	out(fd, "=== end ===\n");
}

static int listen_socket(int port)
{
	int fd, one = 1;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((uint16_t)port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		return -1;
	if (listen(fd, 8) < 0)
		return -1;
	return fd;
}

static void configure_client(int fd)
{
	struct timeval tv;

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

int main(int argc, char **argv)
{
	int port = 2424;
	int srv;

	if (argc > 1)
		port = atoi(argv[1]);
	signal(SIGPIPE, SIG_IGN);
	srv = listen_socket(port);
	if (srv < 0) {
		perror("listen");
		return 1;
	}
	for (;;) {
		int client = accept4(srv, NULL, NULL, SOCK_CLOEXEC);
		if (client < 0) {
			if (errno == EINTR)
				continue;
			sleep(1);
			continue;
		}
		configure_client(client);
		send_report(client);
		shutdown(client, SHUT_WR);
		close(client);
	}
}
