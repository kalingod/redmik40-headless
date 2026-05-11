#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifndef CRTSCTS
#define CRTSCTS 020000000000
#endif

#define HCI_COMMAND_PKT 0x01
#define HCI_EVENT_PKT 0x04
#define MAX_TLV_SEGMENT 243

enum rome_tlv_mode {
	ROME_SKIP_EVT_NONE = 0,
	ROME_SKIP_EVT_VSE = 1,
	ROME_SKIP_EVT_CC = 2,
	ROME_SKIP_EVT_VSE_CC = 3,
};

enum nvm_patch_mode {
	NVM_PATCH_KERNEL = 0,
	NVM_PATCH_H4 = 1,
};

struct blob {
	uint8_t *data;
	size_t len;
};

struct bdaddr_override {
	bool set;
	uint8_t le[6];
	char display[18];
};

static bool quiet_tlv;

static long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void hex_dump(const char *prefix, const uint8_t *buf, size_t len)
{
	size_t i;

	printf("%s len=%zu", prefix, len);
	for (i = 0; i < len; i++)
		printf(" %02x", buf[i]);
	printf("\n");
	fflush(stdout);
}

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static void format_bdaddr_display(const uint8_t le[6], char *buf, size_t len)
{
	snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
		 le[5], le[4], le[3], le[2], le[1], le[0]);
}

static int parse_bdaddr_override(const char *arg, struct bdaddr_override *out)
{
	uint8_t display[6];

	if (strlen(arg) != 17)
		return -1;

	for (size_t i = 0; i < 6; i++) {
		int hi = hex_nibble(arg[i * 3]);
		int lo = hex_nibble(arg[i * 3 + 1]);

		if (hi < 0 || lo < 0)
			return -1;
		if (i < 5 && arg[i * 3 + 2] != ':')
			return -1;

		display[i] = (uint8_t)((hi << 4) | lo);
	}

	for (size_t i = 0; i < 6; i++)
		out->le[i] = display[5 - i];
	snprintf(out->display, sizeof(out->display),
		 "%02X:%02X:%02X:%02X:%02X:%02X",
		 display[0], display[1], display[2],
		 display[3], display[4], display[5]);
	out->set = true;
	return 0;
}

static uint16_t get_le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static speed_t baud_constant(unsigned int speed)
{
	switch (speed) {
	case 2400:
		return B2400;
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
#ifdef B230400
	case 230400:
		return B230400;
#endif
#ifdef B460800
	case 460800:
		return B460800;
#endif
#ifdef B500000
	case 500000:
		return B500000;
#endif
#ifdef B921600
	case 921600:
		return B921600;
#endif
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

static int qca_baud_code(unsigned int speed)
{
	switch (speed) {
	case 115200:
		return 0;
	case 57600:
		return 1;
	case 38400:
		return 2;
	case 19200:
		return 3;
	case 9600:
		return 4;
	case 230400:
		return 5;
	case 460800:
		return 7;
	case 500000:
		return 8;
	case 921600:
		return 10;
	case 1000000:
		return 11;
	case 2000000:
		return 13;
	case 3000000:
		return 14;
	case 3200000:
		return 17;
	case 3500000:
		return 18;
	default:
		return -1;
	}
}

static const char *nvm_patch_mode_name(enum nvm_patch_mode mode)
{
	switch (mode) {
	case NVM_PATCH_KERNEL:
		return "kernel";
	case NVM_PATCH_H4:
		return "h4";
	default:
		return "unknown";
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
	printf("UART speed=%u flow=%s\n", speed, flow ? "on" : "off");
	fflush(stdout);
	return 0;
}

static int open_uart(const char *dev)
{
	int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (fd < 0)
		perror(dev);
	return fd;
}

static void close_uart(int fd)
{
	if (fd >= 0) {
		tcflush(fd, TCIOFLUSH);
		close(fd);
	}
}

static int read_byte_deadline(int fd, uint8_t *out, long long deadline)
{
	for (;;) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		long long left = deadline - now_ms();
		int ret;
		ssize_t n;

		if (left <= 0)
			return 0;

		ret = poll(&pfd, 1, left > 1000 ? 1000 : (int)left);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			return -1;
		}
		if (ret == 0)
			continue;

		n = read(fd, out, 1);
		if (n == 1)
			return 1;
		if (n < 0 && (errno == EAGAIN || errno == EINTR))
			continue;
		if (n < 0)
			perror("read");
		return -1;
	}
}

static void parse_hci_event(const uint8_t *buf, size_t len)
{
	uint8_t event;
	uint8_t plen;
	uint16_t opcode;

	if (len < 3 || buf[0] != HCI_EVENT_PKT)
		return;

	event = buf[1];
	plen = buf[2];
	printf("EVENT code=0x%02x plen=%u\n", event, plen);

	if (event == 0x0e && len >= 7) {
		opcode = get_le16(buf + 4);
		printf("  command-complete opcode=0x%04x status=0x%02x\n",
		       opcode, buf[6]);
		if (opcode == 0xfc00 && buf[6] == 0x00 && len >= 21) {
			uint8_t rtype = buf[7];
			uint8_t rlen = buf[8];
			uint32_t product = get_le32(buf + 9);
			uint16_t patch = get_le16(buf + 13);
			uint16_t rome = get_le16(buf + 15);
			uint32_t soc = get_le32(buf + 17);
			uint32_t soc_ver = (soc << 16) | (rome & 0xffff);
			uint8_t rom_file = ((soc_ver & 0x00000f00) >> 4) |
					   (soc_ver & 0x0000000f);

			printf("  qca-cc-version rtype=0x%02x rlen=%u product=0x%08x patch=0x%04x rome=0x%04x soc=0x%08x soc_ver=0x%08x rom_file=%02x\n",
			       rtype, rlen, product, patch, rome, soc, soc_ver,
			       rom_file);
			printf("  expected-wcn3990-fw qca/crbtfw%02x.tlv qca/crnv%02x.bin\n",
			       rom_file, rom_file);
		}
		if (opcode == 0x1001 && len >= 15) {
			printf("  local-version hci=0x%02x hci_rev=0x%04x lmp=0x%02x manufacturer=%u subver=0x%04x\n",
			       buf[7], get_le16(buf + 8), buf[10],
			       get_le16(buf + 11), get_le16(buf + 13));
		}
	}

	if (event == 0xff && len >= 5) {
		printf("  vendor cresp=0x%02x rtype=0x%02x\n", buf[3], buf[4]);
		if (buf[3] == 0x00 && (buf[4] == 0x02 || buf[4] == 0x19) &&
		    len >= 17) {
			uint32_t product = get_le32(buf + 5);
			uint16_t patch = get_le16(buf + 9);
			uint16_t rome = get_le16(buf + 11);
			uint32_t soc = get_le32(buf + 13);
			uint32_t soc_ver = (soc << 16) | (rome & 0xffff);
			uint8_t rom_file = ((soc_ver & 0x00000f00) >> 4) |
					   (soc_ver & 0x0000000f);

			printf("  qca-version product=0x%08x patch=0x%04x rome=0x%04x soc=0x%08x soc_ver=0x%08x rom_file=%02x\n",
			       product, patch, rome, soc, soc_ver, rom_file);
			printf("  expected-wcn3990-fw qca/crbtfw%02x.tlv qca/crnv%02x.bin\n",
			       rom_file, rom_file);
		}
	}

	fflush(stdout);
}

static int read_hci_event(int fd, int timeout_ms, const char *label)
{
	uint8_t buf[260];
	uint8_t skipped[64];
	size_t skipped_len = 0;
	long long deadline = now_ms() + timeout_ms;
	int ret;

	memset(buf, 0, sizeof(buf));

	for (;;) {
		ret = read_byte_deadline(fd, &buf[0], deadline);
		if (ret <= 0)
			goto timeout;
		if (buf[0] == HCI_EVENT_PKT)
			break;
		if (skipped_len < sizeof(skipped))
			skipped[skipped_len++] = buf[0];
	}

	if (skipped_len)
		hex_dump("RX skipped", skipped, skipped_len);

	for (size_t i = 1; i < 3; i++) {
		ret = read_byte_deadline(fd, &buf[i], deadline);
		if (ret <= 0)
			goto timeout;
	}

	for (size_t i = 0; i < buf[2]; i++) {
		ret = read_byte_deadline(fd, &buf[3 + i], deadline);
		if (ret <= 0)
			goto timeout;
	}

	hex_dump("RX event", buf, 3 + buf[2]);
	parse_hci_event(buf, 3 + buf[2]);
	return 1;

timeout:
	printf("%s: no complete HCI event within %d ms\n", label, timeout_ms);
	fflush(stdout);
	return 0;
}

static void drain_pending(int fd, int timeout_ms)
{
	uint8_t buf[256];
	size_t total = 0;
	long long deadline = now_ms() + timeout_ms;

	for (;;) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		long long left = deadline - now_ms();
		int ret;
		ssize_t n;

		if (left <= 0)
			break;
		ret = poll(&pfd, 1, left > 100 ? 100 : (int)left);
		if (ret <= 0)
			break;
		n = read(fd, buf, sizeof(buf));
		if (n > 0) {
			hex_dump("RX drain", buf, (size_t)n);
			total += (size_t)n;
		} else if (n < 0 && errno != EAGAIN && errno != EINTR) {
			perror("drain read");
			break;
		}
	}

	if (total)
		printf("drained %zu bytes\n", total);
	fflush(stdout);
}

static int write_all(int fd, const uint8_t *buf, size_t len)
{
	size_t off = 0;
	long long deadline = now_ms() + 1500;

	while (off < len) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLOUT,
		};
		long long left = deadline - now_ms();
		int ret;
		ssize_t n = write(fd, buf + off, len - off);

		if (n > 0) {
			off += (size_t)n;
			continue;
		}
		if (left <= 0) {
			fprintf(stderr, "write timeout after %zu/%zu bytes\n",
				off, len);
			return -1;
		}
		if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
			ret = poll(&pfd, 1, left > 100 ? 100 : (int)left);
			if (ret < 0 && errno != EINTR) {
				perror("write poll");
				return -1;
			}
			continue;
		}
		if (n == 0)
			continue;
		perror("write");
		return -1;
	}

	usleep(2000);
	return 0;
}

static int send_raw_byte(int fd, uint8_t byte, const char *name)
{
	hex_dump(name, &byte, 1);
	return write_all(fd, &byte, 1);
}

static const char *ibs_byte_name(uint8_t byte)
{
	switch (byte) {
	case 0xfc:
		return " WAKE_ACK";
	case 0xfd:
		return " WAKE_IND";
	case 0xfe:
		return " SLEEP_IND";
	default:
		return "";
	}
}

static int run_ibs_wake_probe(int fd, int count, int timeout_ms)
{
	int saw_any = 0;
	int saw_wake_ack = 0;

	for (int i = 0; i < count; i++) {
		long long deadline = now_ms() + timeout_ms;
		uint8_t byte;

		printf("IBS wake probe send=%d/%d timeout_ms=%d\n",
		       i + 1, count, timeout_ms);
		fflush(stdout);

		if (send_raw_byte(fd, 0xfd, "TX ibs-wake-ind") < 0)
			return -1;

		for (;;) {
			int ret = read_byte_deadline(fd, &byte, deadline);

			if (ret < 0)
				return -1;
			if (ret == 0)
				break;

			saw_any = 1;
			if (byte == 0xfc)
				saw_wake_ack = 1;
			printf("RX ibs-wake-probe byte=0x%02x%s\n",
			       byte, ibs_byte_name(byte));
			fflush(stdout);
		}

		usleep(100000);
	}

	printf("IBS wake probe result: sends=%d saw_any=%s saw_wake_ack=%s\n",
	       count, saw_any ? "yes" : "no",
	       saw_wake_ack ? "yes" : "no");
	fflush(stdout);
	return 0;
}

static int send_hci_cmd(int fd, uint16_t opcode, const uint8_t *params,
			uint8_t plen, const char *name)
{
	uint8_t buf[4 + 255];

	buf[0] = HCI_COMMAND_PKT;
	buf[1] = opcode & 0xff;
	buf[2] = opcode >> 8;
	buf[3] = plen;
	if (plen)
		memcpy(buf + 4, params, plen);

	if (name)
		hex_dump(name, buf, 4 + plen);
	return write_all(fd, buf, 4 + plen);
}

static int run_qca_version_probe(int fd)
{
	uint8_t qca_ver = 0x19;

	if (send_hci_cmd(fd, 0xfc00, &qca_ver, 1, "TX qca-version") == 0) {
		read_hci_event(fd, 2000, "qca-version primary");
		read_hci_event(fd, 300, "qca-version trailing");
	}

	return 0;
}

static int run_version_sequence(int fd, bool do_reset)
{
	run_qca_version_probe(fd);

	if (send_hci_cmd(fd, 0x1001, NULL, 0, "TX read-local-version") == 0)
		read_hci_event(fd, 1500, "read-local-version");

	if (do_reset && send_hci_cmd(fd, 0x0c03, NULL, 0, "TX hci-reset") == 0)
		read_hci_event(fd, 1500, "hci-reset");

	return 0;
}

static int send_baud_switch(int fd, unsigned int speed)
{
	int code = qca_baud_code(speed);
	uint8_t param;

	if (code < 0) {
		fprintf(stderr, "unsupported QCA baud switch speed %u\n", speed);
		return -1;
	}

	param = (uint8_t)code;
	if (send_hci_cmd(fd, 0xfc48, &param, 1, "TX qca-baud-switch") < 0)
		return -1;

	usleep(300000);
	return 0;
}

static int load_blob(const char *path, struct blob *blob)
{
	FILE *fp;
	long size;

	memset(blob, 0, sizeof(*blob));
	fp = fopen(path, "rb");
	if (!fp) {
		perror(path);
		return -1;
	}

	if (fseek(fp, 0, SEEK_END) < 0) {
		perror("fseek");
		fclose(fp);
		return -1;
	}
	size = ftell(fp);
	if (size <= 0) {
		fprintf(stderr, "%s invalid size %ld\n", path, size);
		fclose(fp);
		return -1;
	}
	rewind(fp);

	blob->data = malloc((size_t)size);
	if (!blob->data) {
		perror("malloc");
		fclose(fp);
		return -1;
	}
	blob->len = (size_t)size;

	if (fread(blob->data, 1, blob->len, fp) != blob->len) {
		fprintf(stderr, "%s short read\n", path);
		free(blob->data);
		memset(blob, 0, sizeof(*blob));
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

static void free_blob(struct blob *blob)
{
	free(blob->data);
	memset(blob, 0, sizeof(*blob));
}

static enum rome_tlv_mode inspect_and_patch_tlv(const char *path,
						struct blob *blob,
						int expected_type,
						uint8_t baud_code,
						enum nvm_patch_mode nvm_mode,
						const struct bdaddr_override *bdaddr)
{
	uint32_t type_len;
	uint8_t tlv_type;
	uint32_t payload_len;
	enum rome_tlv_mode mode = ROME_SKIP_EVT_NONE;

	if (blob->len < 4) {
		fprintf(stderr, "%s too small for TLV header\n", path);
		return mode;
	}

	type_len = get_le32(blob->data);
	tlv_type = type_len & 0xff;
	payload_len = (type_len >> 8) & 0x00ffffff;
	printf("TLV %s type=%u payload_len=%u file_len=%zu\n",
	       path, tlv_type, payload_len, blob->len);

	if (tlv_type != expected_type)
		printf("TLV warning: expected type %d but file says %u\n",
		       expected_type, tlv_type);

	if (expected_type == 1 && blob->len >= 28) {
		mode = blob->data[14];
		printf("PATCH total=%u data_len=%u format=0x%02x signature=0x%02x mode=%u product=0x%04x rom=0x%04x patch=0x%04x entry=0x%08x\n",
		       get_le32(blob->data + 4), get_le32(blob->data + 8),
		       blob->data[12], blob->data[13], mode,
		       get_le16(blob->data + 16), get_le16(blob->data + 18),
		       get_le16(blob->data + 20), get_le32(blob->data + 24));
	}

	if (expected_type == 2) {
		size_t idx = 0;
		size_t base = 4;
		size_t limit = payload_len;

		while (idx + 12 <= limit && base + idx + 12 <= blob->len) {
			uint8_t *tag = blob->data + base + idx;
			uint16_t tag_id = get_le16(tag);
			uint16_t tag_len = get_le16(tag + 2);
			uint8_t *tag_data = tag + 12;

			if (idx + 12 + tag_len > limit ||
			    base + idx + 12 + tag_len > blob->len) {
				printf("NVM tag parse stop idx=%zu tag=%u len=%u\n",
				       idx, tag_id, tag_len);
				break;
			}

			if (tag_id == 2 && tag_len >= 6) {
				char before[18];
				char after[18];

				format_bdaddr_display(tag_data, before, sizeof(before));
				printf("NVM tag2 bdaddr before bytes %02x %02x %02x %02x %02x %02x display=%s\n",
				       tag_data[0], tag_data[1], tag_data[2],
				       tag_data[3], tag_data[4], tag_data[5],
				       before);
				if (bdaddr && bdaddr->set) {
					memcpy(tag_data, bdaddr->le, 6);
					format_bdaddr_display(tag_data, after,
							      sizeof(after));
					printf("NVM tag2 bdaddr after  bytes %02x %02x %02x %02x %02x %02x display=%s source=--bdaddr\n",
					       tag_data[0], tag_data[1],
					       tag_data[2], tag_data[3],
					       tag_data[4], tag_data[5],
					       after);
				}
			} else if (tag_id == 17 && tag_len >= 3) {
				printf("NVM tag17 before %02x %02x %02x\n",
				       tag_data[0], tag_data[1], tag_data[2]);
				if (nvm_mode == NVM_PATCH_H4)
					tag_data[0] &= ~0x80;
				else
					tag_data[0] |= 0x80;
				tag_data[2] = baud_code;
				printf("NVM tag17 after  %02x %02x %02x mode=%s\n",
				       tag_data[0], tag_data[1], tag_data[2],
				       nvm_patch_mode_name(nvm_mode));
			} else if (tag_id == 27 && tag_len >= 1) {
				printf("NVM tag27 before %02x\n", tag_data[0]);
				if (nvm_mode == NVM_PATCH_H4)
					tag_data[0] &= ~0x01;
				else
					tag_data[0] |= 0x01;
				printf("NVM tag27 after  %02x mode=%s\n",
				       tag_data[0], nvm_patch_mode_name(nvm_mode));
			}

			idx += 12 + tag_len;
		}
	}

	fflush(stdout);
	return mode;
}

static int send_tlv_file(int fd, const char *path, int type, uint8_t baud_code,
			 enum nvm_patch_mode nvm_mode,
			 const struct bdaddr_override *bdaddr)
{
	struct blob blob;
	enum rome_tlv_mode mode;
	size_t off = 0;
	int idx = 0;
	int ret = -1;

	if (load_blob(path, &blob) < 0)
		return -1;

	mode = inspect_and_patch_tlv(path, &blob, type, baud_code, nvm_mode,
				     bdaddr);

	while (off < blob.len) {
		uint8_t params[2 + MAX_TLV_SEGMENT];
		size_t remain = blob.len - off;
		size_t seg = remain > MAX_TLV_SEGMENT ? MAX_TLV_SEGMENT : remain;
		size_t after = remain - seg;
		enum rome_tlv_mode seg_mode = mode;
		bool expect_event;
		char label[96];

		if (!after || seg < MAX_TLV_SEGMENT)
			seg_mode = ROME_SKIP_EVT_NONE;

		expect_event = !(seg_mode == ROME_SKIP_EVT_VSE ||
				 seg_mode == ROME_SKIP_EVT_VSE_CC);

		params[0] = 0x1e;
		params[1] = (uint8_t)seg;
		memcpy(params + 2, blob.data + off, seg);

		if (!quiet_tlv || idx < 3 || !after || idx % 64 == 0) {
			printf("TX tlv path=%s idx=%d offset=%zu size=%zu expect_event=%s mode=%u\n",
			       path, idx, off, seg,
			       expect_event ? "yes" : "no", seg_mode);
			fflush(stdout);
		}

		if (send_hci_cmd(fd, 0xfc00, params, (uint8_t)(seg + 2),
				 NULL) < 0)
			goto out;

		if (expect_event) {
			snprintf(label, sizeof(label), "tlv idx %d", idx);
			if (read_hci_event(fd, 2500, label) <= 0)
				goto out;
		}

		off += seg;
		idx++;
	}

	printf("TLV download complete path=%s segments=%d bytes=%zu\n",
	       path, idx, blob.len);
	ret = 0;

out:
	free_blob(&blob);
	return ret;
}

static void usage(const char *argv0)
{
	fprintf(stderr,
		"Usage: %s [--dev /dev/ttyHS0] [--flow|--no-flow] [--pulse] "
		"[--init-speed 115200] [--switch-oper 3000000] [--no-reset] "
		"[--quiet-tlv] [--nvm-mode kernel|h4] [--nvm-h4] "
		"[--bdaddr XX:XX:XX:XX:XX:XX] "
		"[--no-post-fw-probe] [--ibs-wake-probe] "
		"[--ibs-wake-count N] [--ibs-wake-timeout-ms N] "
		"[--patch /lib/firmware/qca/crbtfw20.tlv --nvm /lib/firmware/qca/crnv20.bin]\n",
		argv0);
}

int main(int argc, char **argv)
{
	const char *dev = "/dev/ttyHS0";
	unsigned int init_speed = 115200;
	unsigned int oper_speed = 0;
	bool flow = true;
	bool pulse = false;
	bool do_reset = true;
	bool post_fw_probe = true;
	bool ibs_wake_probe = false;
	int ibs_wake_count = 5;
	int ibs_wake_timeout_ms = 500;
	enum nvm_patch_mode nvm_mode = NVM_PATCH_KERNEL;
	struct bdaddr_override bdaddr = { 0 };
	const char *patch_path = NULL;
	const char *nvm_path = NULL;
	int fd;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--dev") && i + 1 < argc) {
			dev = argv[++i];
		} else if (!strcmp(argv[i], "--flow")) {
			flow = true;
		} else if (!strcmp(argv[i], "--no-flow")) {
			flow = false;
		} else if (!strcmp(argv[i], "--pulse")) {
			pulse = true;
		} else if (!strcmp(argv[i], "--init-speed") && i + 1 < argc) {
			init_speed = (unsigned int)strtoul(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "--switch-oper") && i + 1 < argc) {
			oper_speed = (unsigned int)strtoul(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "--no-reset")) {
			do_reset = false;
		} else if (!strcmp(argv[i], "--no-post-fw-probe")) {
			post_fw_probe = false;
		} else if (!strcmp(argv[i], "--ibs-wake-probe")) {
			ibs_wake_probe = true;
		} else if (!strcmp(argv[i], "--ibs-wake-count") && i + 1 < argc) {
			ibs_wake_count = (int)strtol(argv[++i], NULL, 0);
			if (ibs_wake_count <= 0) {
				fprintf(stderr, "--ibs-wake-count must be positive\n");
				return 2;
			}
		} else if (!strcmp(argv[i], "--ibs-wake-timeout-ms") && i + 1 < argc) {
			ibs_wake_timeout_ms = (int)strtol(argv[++i], NULL, 0);
			if (ibs_wake_timeout_ms <= 0) {
				fprintf(stderr, "--ibs-wake-timeout-ms must be positive\n");
				return 2;
			}
		} else if (!strcmp(argv[i], "--patch") && i + 1 < argc) {
			patch_path = argv[++i];
		} else if (!strcmp(argv[i], "--nvm") && i + 1 < argc) {
			nvm_path = argv[++i];
		} else if (!strcmp(argv[i], "--bdaddr") && i + 1 < argc) {
			if (parse_bdaddr_override(argv[++i], &bdaddr) < 0) {
				fprintf(stderr, "invalid --bdaddr value, expected XX:XX:XX:XX:XX:XX\n");
				return 2;
			}
		} else if (!strcmp(argv[i], "--quiet-tlv")) {
			quiet_tlv = true;
		} else if (!strcmp(argv[i], "--nvm-h4")) {
			nvm_mode = NVM_PATCH_H4;
		} else if (!strcmp(argv[i], "--nvm-mode") && i + 1 < argc) {
			const char *mode = argv[++i];

			if (!strcmp(mode, "kernel")) {
				nvm_mode = NVM_PATCH_KERNEL;
			} else if (!strcmp(mode, "h4")) {
				nvm_mode = NVM_PATCH_H4;
			} else {
				fprintf(stderr, "unsupported NVM mode: %s\n", mode);
				usage(argv[0]);
				return 2;
			}
		} else if (!strcmp(argv[i], "--help")) {
			usage(argv[0]);
			return 0;
		} else {
			usage(argv[0]);
			return 2;
		}
	}

	printf("alioth_qca_uart_probe dev=%s init_speed=%u flow=%s pulse=%s oper_speed=%u reset=%s post_fw_probe=%s ibs_wake_probe=%s ibs_wake_count=%d ibs_wake_timeout_ms=%d nvm_mode=%s bdaddr=%s patch=%s nvm=%s\n",
	       dev, init_speed, flow ? "on" : "off", pulse ? "yes" : "no",
	       oper_speed, do_reset ? "yes" : "no",
	       post_fw_probe ? "yes" : "no",
	       ibs_wake_probe ? "yes" : "no",
	       ibs_wake_count, ibs_wake_timeout_ms,
	       nvm_patch_mode_name(nvm_mode),
	       bdaddr.set ? bdaddr.display : "(none)",
	       patch_path ? patch_path : "(none)", nvm_path ? nvm_path : "(none)");
	fflush(stdout);

	if (!!patch_path != !!nvm_path) {
		fprintf(stderr, "--patch and --nvm must be used together\n");
		return 2;
	}

	fd = open_uart(dev);
	if (fd < 0)
		return 1;

	if (pulse) {
		if (set_uart(fd, 2400, false) < 0)
			return 1;
		if (send_raw_byte(fd, 0xc0, "TX wcn3990-poweroff-pulse") < 0)
			return 1;
		usleep(1000);

		if (set_uart(fd, init_speed, false) < 0)
			return 1;
		if (send_raw_byte(fd, 0xfc, "TX wcn3990-poweron-pulse") < 0)
			return 1;
		usleep(100000);

		close_uart(fd);
		fd = open_uart(dev);
		if (fd < 0)
			return 1;
	}

	if (set_uart(fd, init_speed, flow) < 0) {
		close_uart(fd);
		return 1;
	}

	drain_pending(fd, 250);
	if (patch_path)
		run_qca_version_probe(fd);
	else
		run_version_sequence(fd, oper_speed ? false : do_reset);

	if (oper_speed) {
		if (set_uart(fd, init_speed, false) < 0) {
			close_uart(fd);
			return 1;
		}
		if (send_baud_switch(fd, oper_speed) == 0) {
			if (set_uart(fd, oper_speed, flow) == 0) {
				drain_pending(fd, 250);
				if (patch_path)
					run_qca_version_probe(fd);
				else
					run_version_sequence(fd, do_reset);
			}
		}
	}

	if (patch_path && nvm_path) {
		unsigned int fw_speed = oper_speed ? oper_speed : init_speed;
		int baud_code = qca_baud_code(fw_speed);

		if (baud_code < 0) {
			fprintf(stderr, "no QCA baud code for firmware speed %u\n",
				fw_speed);
			close_uart(fd);
			return 1;
		}
		if (send_tlv_file(fd, patch_path, 1, (uint8_t)baud_code,
				  nvm_mode, NULL) < 0 ||
		    send_tlv_file(fd, nvm_path, 2, (uint8_t)baud_code,
				  nvm_mode, &bdaddr) < 0) {
			fprintf(stderr, "firmware download failed\n");
			close_uart(fd);
			return 1;
		}
		usleep(50000);
		if (ibs_wake_probe)
			run_ibs_wake_probe(fd, ibs_wake_count,
					   ibs_wake_timeout_ms);
		if (post_fw_probe) {
			drain_pending(fd, 500);
			if (do_reset && send_hci_cmd(fd, 0x0c03, NULL, 0,
						     "TX hci-reset-after-fw") == 0)
				read_hci_event(fd, 3000, "hci-reset-after-fw");
			if (send_hci_cmd(fd, 0x1001, NULL, 0,
					 "TX read-local-version-after-fw") == 0)
				read_hci_event(fd, 3000,
					       "read-local-version-after-fw");
		}
	}

	close_uart(fd);
	printf("probe done\n");
	return 0;
}
