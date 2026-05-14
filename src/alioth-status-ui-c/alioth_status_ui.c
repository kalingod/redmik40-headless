#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef ALIOTH_GPU_RENDERER
#include <vulkan/vulkan.h>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif

#include "alioth-panel/panel_paths.h"

#define DRM_CONNECTED 1
#define DRM_FORMAT_XRGB8888 0x34325258u

struct drm_mode_card_res {
	uint64_t fb_id_ptr;
	uint64_t crtc_id_ptr;
	uint64_t connector_id_ptr;
	uint64_t encoder_id_ptr;
	uint32_t count_fbs;
	uint32_t count_crtcs;
	uint32_t count_connectors;
	uint32_t count_encoders;
	uint32_t min_width;
	uint32_t max_width;
	uint32_t min_height;
	uint32_t max_height;
};

struct drm_mode_modeinfo {
	uint32_t clock;
	uint16_t hdisplay;
	uint16_t hsync_start;
	uint16_t hsync_end;
	uint16_t htotal;
	uint16_t hskew;
	uint16_t vdisplay;
	uint16_t vsync_start;
	uint16_t vsync_end;
	uint16_t vtotal;
	uint16_t vscan;
	uint32_t vrefresh;
	uint32_t flags;
	uint32_t type;
	char name[32];
};

struct drm_mode_get_connector {
	uint64_t encoders_ptr;
	uint64_t modes_ptr;
	uint64_t props_ptr;
	uint64_t prop_values_ptr;
	uint32_t count_modes;
	uint32_t count_props;
	uint32_t count_encoders;
	uint32_t encoder_id;
	uint32_t connector_id;
	uint32_t connector_type;
	uint32_t connector_type_id;
	uint32_t connection;
	uint32_t mm_width;
	uint32_t mm_height;
	uint32_t subpixel;
	uint32_t pad;
};

struct drm_mode_get_encoder {
	uint32_t encoder_id;
	uint32_t encoder_type;
	uint32_t crtc_id;
	uint32_t possible_crtcs;
	uint32_t possible_clones;
};

struct drm_mode_create_dumb {
	uint32_t height;
	uint32_t width;
	uint32_t bpp;
	uint32_t flags;
	uint32_t handle;
	uint32_t pitch;
	uint64_t size;
};

struct drm_mode_map_dumb {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

struct drm_mode_destroy_dumb {
	uint32_t handle;
};

struct drm_mode_fb_cmd2 {
	uint32_t fb_id;
	uint32_t width;
	uint32_t height;
	uint32_t pixel_format;
	uint32_t flags;
	uint32_t handles[4];
	uint32_t pitches[4];
	uint32_t offsets[4];
	uint64_t modifier[4];
};

struct drm_mode_crtc {
	uint64_t set_connectors_ptr;
	uint32_t count_connectors;
	uint32_t crtc_id;
	uint32_t fb_id;
	uint32_t x;
	uint32_t y;
	uint32_t gamma_size;
	uint32_t mode_valid;
	struct drm_mode_modeinfo mode;
};

struct drm_clip_rect {
	uint16_t x1;
	uint16_t y1;
	uint16_t x2;
	uint16_t y2;
};

struct drm_mode_fb_dirty_cmd {
	uint32_t fb_id;
	uint32_t flags;
	uint32_t color;
	uint32_t num_clips;
	uint64_t clips_ptr;
};

struct drm_prime_handle {
	uint32_t handle;
	uint32_t flags;
	int32_t fd;
};

struct drm_mode_crtc_page_flip {
	uint32_t crtc_id;
	uint32_t fb_id;
	uint32_t flags;
	uint32_t reserved;
	uint64_t user_data;
};

#define DRM_MODE_PAGE_FLIP_EVENT 0x01
#define DRM_IOCTL_PRIME_HANDLE_TO_FD _IOWR('d', 0x2d, struct drm_prime_handle)
#define DRM_IOCTL_MODE_GETRESOURCES _IOWR('d', 0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_SETCRTC _IOWR('d', 0xA2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETENCODER _IOWR('d', 0xA6, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETCONNECTOR _IOWR('d', 0xA7, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_RMFB _IOWR('d', 0xAF, uint32_t)
#define DRM_IOCTL_MODE_PAGE_FLIP _IOWR('d', 0xB0, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_DIRTYFB _IOWR('d', 0xB1, struct drm_mode_fb_dirty_cmd)
#define DRM_IOCTL_MODE_CREATE_DUMB _IOWR('d', 0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB _IOWR('d', 0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB _IOWR('d', 0xB4, struct drm_mode_destroy_dumb)
#define DRM_IOCTL_MODE_ADDFB2 _IOWR('d', 0xB8, struct drm_mode_fb_cmd2)

struct drm_state {
	int fd;
	uint32_t connector_id;
	uint32_t crtc_id;
	struct drm_mode_modeinfo mode;
	int width;
	int height;
	int pitch;
	uint32_t handle[2];
	uint32_t fb_id[2];
	size_t size;
	uint8_t *map[2];
#ifdef ALIOTH_GPU_RENDERER
	int prime_fd[2];
#endif
	int front;
};

struct wifi_saved_network {
	char ssid[WIFI_SSID_MAX];
	char psk[WIFI_PSK_MAX];
};

#ifdef ALIOTH_GPU_RENDERER
struct gpu_vertex {
	float pos[2];
	float uv[2];
	float color[3];
};

struct gpu_rect {
	int x;
	int y;
	int w;
	int h;
	uint32_t color;
};

struct gpu_glyph {
	float x0;
	float y0;
	float x1;
	float y1;
	float u0;
	float v0;
	float u1;
	float v1;
	uint32_t color;
	uint8_t scale;
};

enum gpu_draw_op_type {
	GPU_OP_RECT = 1,
	GPU_OP_TEXT = 2,
};

struct gpu_draw_op {
	uint8_t type;
	uint8_t scale;
	uint32_t first;
	uint32_t count;
};

struct gpu_font_atlas {
	int scale;
	int pixel_height;
	int width;
	int height;
	int ascent;
	stbtt_bakedchar chars[GPU_FONT_COUNT];
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler;
	VkDescriptorSet descriptor_set;
	bool ready;
};

struct gpu_state;
#endif

struct canvas {
	int width;
	int height;
	int pitch;
	uint8_t *data;
#ifdef ALIOTH_GPU_RENDERER
	bool gpu_record;
	bool gpu_overflow;
	struct gpu_state *gpu;
	struct gpu_rect *rects;
	size_t rect_count;
	size_t rect_cap;
	struct gpu_glyph *glyphs;
	size_t glyph_count;
	size_t glyph_cap;
	struct gpu_draw_op *ops;
	size_t op_count;
	size_t op_cap;
#endif
};

#ifdef ALIOTH_GPU_RENDERER
struct gpu_imported_image {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFramebuffer framebuffer;
	VkImageLayout layout;
	bool initialized;
};

struct gpu_state {
	VkInstance instance;
	VkPhysicalDevice phys;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family;
	VkCommandPool command_pool;
	PFN_vkGetMemoryFdPropertiesKHR get_memory_fd_props;
	VkRenderPass render_pass;
	VkShaderModule vert_shader;
	VkShaderModule frag_shader;
	VkShaderModule text_frag_shader;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorSetLayout text_descriptor_layout;
	VkDescriptorPool text_descriptor_pool;
	VkPipelineLayout text_pipeline_layout;
	VkPipeline text_pipeline;
	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_memory;
	VkDeviceSize vertex_capacity;
	VkBuffer text_vertex_buffer;
	VkDeviceMemory text_vertex_memory;
	VkDeviceSize text_vertex_capacity;
	uint8_t *font_data;
	size_t font_size;
	struct gpu_font_atlas font[GPU_FONT_MAX_SCALE + 1];
	bool font_ready;
	struct gpu_imported_image image[2];
};
#endif

#include "alioth-panel/app_types.h"

static enum display_mode g_display_mode = DISPLAY_NORMAL;
static bool g_shutdown_confirm = false;
static int64_t g_shutdown_deadline_ms = 0;
static enum ui_screen g_ui_screen = UI_STATUS;
static enum app_page g_app_page = PAGE_MONITOR;
static enum power_action g_power_action = POWER_BACK;
static int g_power_menu_index = 0;
static int g_panel_refresh_hz = 0;
static char g_panel_mode_name[48] = "";
static int g_frame_draw_ms = 0;
static int g_frame_gpu_ms = 0;
static int g_frame_present_ms = 0;
static int g_frame_total_ms = 0;
static struct wifi_ui_state g_wifi;
static int64_t g_gpu_test_started_ms = 0;
static char g_gpu_message[160];
static char g_cpu_governor_message[128];
static int g_screen_width = 1080;
static int g_screen_height = 2400;

static struct gpu_frame_metrics g_gpu_metrics;
static struct gpu_page_cache g_gpu_page_cache;

struct services_page_cache {
	int64_t last_update_ms;
	int failed_count;
	char failed_value[32];
	char failed_detail[160];
	char service_lines[8][128];
	char audio_state[160];
	char wifi_state[160];
	char panel_metrics[160];
	char log_tail[5][160];
};

static struct services_page_cache g_services_page_cache;

static int64_t monotonic_ms(void);
static const char *page_name(enum app_page page);

static void log_msg(const char *fmt, ...)
{
	FILE *f = fopen("/tmp/alioth-status-ui-c.log", "a");
	va_list ap;
	va_start(ap, fmt);
	if (f) {
		vfprintf(f, fmt, ap);
		fputc('\n', f);
		fclose(f);
	}
	va_end(ap);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}

static char *trim(char *s)
{
	size_t n;
	while (*s && isspace((unsigned char)*s))
		s++;
	n = strlen(s);
	while (n > 0 && isspace((unsigned char)s[n - 1]))
		s[--n] = '\0';
	return s;
}

static bool read_file(const char *path, char *buf, size_t len)
{
	int fd;
	ssize_t n;

	if (len == 0)
		return false;
	buf[0] = '\0';
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	n = read(fd, buf, len - 1);
	close(fd);
	if (n <= 0)
		return false;
	buf[n] = '\0';
	char *p = trim(buf);
	memmove(buf, p, strlen(p) + 1);
	return true;
}

static bool file_exists_mode(const char *path, mode_t mode)
{
	struct stat st;

	if (stat(path, &st) != 0)
		return false;
	if (mode == 0)
		return true;
	return (st.st_mode & S_IFMT) == mode;
}

static const char *display_mode_name(enum display_mode mode)
{
	switch (mode) {
	case DISPLAY_LOW:
		return "low";
	case DISPLAY_NIGHT:
		return "night";
	case DISPLAY_LAMP:
		return "lamp";
	case DISPLAY_OFF:
		return "off";
	case DISPLAY_NORMAL:
	default:
		return "normal";
	}
}

static const char *power_action_name(enum power_action action)
{
	switch (action) {
	case POWER_BACK:
		return "BACK";
	case POWER_OFF:
		return "POWER OFF";
	case POWER_REBOOT:
		return "REBOOT";
	case POWER_FASTBOOT:
		return "FASTBOOT";
	case POWER_ACTION_COUNT:
	default:
		return "UNKNOWN";
	}
}

static enum display_mode parse_display_mode(const char *s)
{
	if (strcmp(s, "low") == 0)
		return DISPLAY_LOW;
	if (strcmp(s, "night") == 0)
		return DISPLAY_NIGHT;
	if (strcmp(s, "lamp") == 0 || strcmp(s, "nightlight") == 0)
		return DISPLAY_LAMP;
	if (strcmp(s, "off") == 0 || strcmp(s, "black") == 0)
		return DISPLAY_OFF;
	return DISPLAY_NORMAL;
}

static void write_display_mode(enum display_mode mode)
{
	int fd = open("/run/lele-display-mode", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	const char *name = display_mode_name(mode);

	if (fd < 0)
		return;
	if (write(fd, name, strlen(name)) < 0)
		log_msg("write display mode failed: %s", strerror(errno));
	if (write(fd, "\n", 1) < 0)
		log_msg("write display mode newline failed: %s", strerror(errno));
	close(fd);
}

static void sync_display_mode_from_file(struct input_state *in)
{
	char mode[64];
	enum display_mode parsed;

	if (!read_file("/run/lele-display-mode", mode, sizeof(mode)))
		return;
	parsed = parse_display_mode(mode);
	in->mode = parsed;
	g_display_mode = parsed;
	if (parsed != DISPLAY_OFF)
		in->previous_lit_mode = parsed;
}

static uint32_t apply_display_color(uint32_t color)
{
	int r = (int)((color >> 16) & 0xff);
	int g = (int)((color >> 8) & 0xff);
	int b = (int)(color & 0xff);
	double rf = 1.0, gf = 1.0, bf = 1.0;

	switch (g_display_mode) {
	case DISPLAY_OFF:
		return 0x00000000;
	case DISPLAY_LOW:
		rf = gf = bf = 0.32;
		break;
	case DISPLAY_NIGHT:
		rf = 0.28;
		gf = 0.15;
		bf = 0.06;
		break;
	case DISPLAY_LAMP:
		break;
	case DISPLAY_NORMAL:
	default:
		break;
	}

	r = (int)(r * rf);
	g = (int)(g * gf);
	b = (int)(b * bf);
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	return (uint32_t)((r << 16) | (g << 8) | b);
}

static void short_copy(char *dst, size_t dst_len, const char *src, size_t max_chars)
{
	size_t n;

	if (dst_len == 0)
		return;
	n = strlen(src);
	if (n > max_chars)
		n = max_chars;
	if (n >= dst_len)
		n = dst_len - 1;
	memcpy(dst, src, n);
	dst[n] = '\0';
}

static void glyph_for(char ch, uint8_t out[7])
{
	static const uint8_t q[7] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
	const uint8_t *g = q;

	ch = (char)toupper((unsigned char)ch);
	switch (ch) {
	case ' ': { static const uint8_t v[7] = {0,0,0,0,0,0,0}; g = v; break; }
	case '?': g = q; break;
	case '!': { static const uint8_t v[7] = {0x04,0x04,0x04,0x04,0x04,0,0x04}; g = v; break; }
	case '"': { static const uint8_t v[7] = {0x0a,0x0a,0x0a,0,0,0,0}; g = v; break; }
	case '\'': { static const uint8_t v[7] = {0x04,0x04,0x08,0,0,0,0}; g = v; break; }
	case '#': { static const uint8_t v[7] = {0x0a,0x0a,0x1f,0x0a,0x1f,0x0a,0x0a}; g = v; break; }
	case '$': { static const uint8_t v[7] = {0x04,0x0f,0x14,0x0e,0x05,0x1e,0x04}; g = v; break; }
	case '@': { static const uint8_t v[7] = {0x0e,0x11,0x17,0x15,0x17,0x10,0x0f}; g = v; break; }
	case '&': { static const uint8_t v[7] = {0x0c,0x12,0x14,0x08,0x15,0x12,0x0d}; g = v; break; }
	case '*': { static const uint8_t v[7] = {0,0x15,0x0e,0x1f,0x0e,0x15,0}; g = v; break; }
	case ',': { static const uint8_t v[7] = {0,0,0,0,0x0c,0x04,0x08}; g = v; break; }
	case ';': { static const uint8_t v[7] = {0,0x04,0x04,0,0x04,0x04,0x08}; g = v; break; }
	case '-': { static const uint8_t v[7] = {0,0,0,0x1f,0,0,0}; g = v; break; }
	case '+': { static const uint8_t v[7] = {0,0x04,0x04,0x1f,0x04,0x04,0}; g = v; break; }
	case '/': { static const uint8_t v[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10}; g = v; break; }
	case '\\': { static const uint8_t v[7] = {0x10,0x08,0x08,0x04,0x02,0x02,0x01}; g = v; break; }
	case '|': { static const uint8_t v[7] = {0x04,0x04,0x04,0,0x04,0x04,0x04}; g = v; break; }
	case ':': { static const uint8_t v[7] = {0,0x04,0x04,0,0x04,0x04,0}; g = v; break; }
	case '.': { static const uint8_t v[7] = {0,0,0,0,0,0x0c,0x0c}; g = v; break; }
	case '=': { static const uint8_t v[7] = {0,0x1f,0,0x1f,0,0,0}; g = v; break; }
	case '%': { static const uint8_t v[7] = {0x19,0x19,0x02,0x04,0x08,0x13,0x13}; g = v; break; }
	case '_': { static const uint8_t v[7] = {0,0,0,0,0,0,0x1f}; g = v; break; }
	case '(': { static const uint8_t v[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02}; g = v; break; }
	case ')': { static const uint8_t v[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08}; g = v; break; }
	case '[': { static const uint8_t v[7] = {0x0e,0x08,0x08,0x08,0x08,0x08,0x0e}; g = v; break; }
	case ']': { static const uint8_t v[7] = {0x0e,0x02,0x02,0x02,0x02,0x02,0x0e}; g = v; break; }
	case '{': { static const uint8_t v[7] = {0x02,0x04,0x04,0x18,0x04,0x04,0x02}; g = v; break; }
	case '}': { static const uint8_t v[7] = {0x08,0x04,0x04,0x03,0x04,0x04,0x08}; g = v; break; }
	case '<': { static const uint8_t v[7] = {0x02,0x04,0x08,0x10,0x08,0x04,0x02}; g = v; break; }
	case '>': { static const uint8_t v[7] = {0x08,0x04,0x02,0x01,0x02,0x04,0x08}; g = v; break; }
	case '~': { static const uint8_t v[7] = {0,0,0x08,0x15,0x02,0,0}; g = v; break; }
	case '`': { static const uint8_t v[7] = {0x08,0x04,0x02,0,0,0,0}; g = v; break; }
	case '0': { static const uint8_t v[7] = {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}; g = v; break; }
	case '1': { static const uint8_t v[7] = {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}; g = v; break; }
	case '2': { static const uint8_t v[7] = {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}; g = v; break; }
	case '3': { static const uint8_t v[7] = {0x1f,0x02,0x04,0x02,0x01,0x11,0x0e}; g = v; break; }
	case '4': { static const uint8_t v[7] = {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}; g = v; break; }
	case '5': { static const uint8_t v[7] = {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e}; g = v; break; }
	case '6': { static const uint8_t v[7] = {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}; g = v; break; }
	case '7': { static const uint8_t v[7] = {0x1f,0x01,0x02,0x04,0x08,0x08,0x08}; g = v; break; }
	case '8': { static const uint8_t v[7] = {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}; g = v; break; }
	case '9': { static const uint8_t v[7] = {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c}; g = v; break; }
	case 'A': { static const uint8_t v[7] = {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}; g = v; break; }
	case 'B': { static const uint8_t v[7] = {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}; g = v; break; }
	case 'C': { static const uint8_t v[7] = {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}; g = v; break; }
	case 'D': { static const uint8_t v[7] = {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}; g = v; break; }
	case 'E': { static const uint8_t v[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}; g = v; break; }
	case 'F': { static const uint8_t v[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}; g = v; break; }
	case 'G': { static const uint8_t v[7] = {0x0e,0x11,0x10,0x17,0x11,0x11,0x0f}; g = v; break; }
	case 'H': { static const uint8_t v[7] = {0x11,0x11,0x11,0x1f,0x11,0x11,0x11}; g = v; break; }
	case 'I': { static const uint8_t v[7] = {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}; g = v; break; }
	case 'J': { static const uint8_t v[7] = {0x01,0x01,0x01,0x01,0x11,0x11,0x0e}; g = v; break; }
	case 'K': { static const uint8_t v[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; g = v; break; }
	case 'L': { static const uint8_t v[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1f}; g = v; break; }
	case 'M': { static const uint8_t v[7] = {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}; g = v; break; }
	case 'N': { static const uint8_t v[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; g = v; break; }
	case 'O': { static const uint8_t v[7] = {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}; g = v; break; }
	case 'P': { static const uint8_t v[7] = {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}; g = v; break; }
	case 'Q': { static const uint8_t v[7] = {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}; g = v; break; }
	case 'R': { static const uint8_t v[7] = {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}; g = v; break; }
	case 'S': { static const uint8_t v[7] = {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}; g = v; break; }
	case 'T': { static const uint8_t v[7] = {0x1f,0x04,0x04,0x04,0x04,0x04,0x04}; g = v; break; }
	case 'U': { static const uint8_t v[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}; g = v; break; }
	case 'V': { static const uint8_t v[7] = {0x11,0x11,0x11,0x11,0x11,0x0a,0x04}; g = v; break; }
	case 'W': { static const uint8_t v[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0a}; g = v; break; }
	case 'X': { static const uint8_t v[7] = {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11}; g = v; break; }
	case 'Y': { static const uint8_t v[7] = {0x11,0x11,0x0a,0x04,0x04,0x04,0x04}; g = v; break; }
	case 'Z': { static const uint8_t v[7] = {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}; g = v; break; }
	}
	memcpy(out, g, 7);
}

#ifdef ALIOTH_GPU_RENDERER
static void gpu_canvas_reset(struct canvas *c)
{
	c->rect_count = 0;
	c->glyph_count = 0;
	c->op_count = 0;
	c->gpu_overflow = false;
}

static bool gpu_add_op(struct canvas *c, enum gpu_draw_op_type type, uint8_t scale,
	size_t first, size_t count)
{
	struct gpu_draw_op *op;

	if (count == 0)
		return true;
	if (c->op_count >= c->op_cap || first > UINT32_MAX || count > UINT32_MAX) {
		c->gpu_overflow = true;
		return false;
	}
	op = &c->ops[c->op_count++];
	op->type = (uint8_t)type;
	op->scale = scale;
	op->first = (uint32_t)first;
	op->count = (uint32_t)count;
	return true;
}

static bool gpu_record_text(struct canvas *c, int x, int y, const char *s, int scale,
	uint32_t color)
{
	struct gpu_font_atlas *font;
	float fx, line_top;
	size_t first, count = 0;

	if (!c->gpu || !c->gpu->font_ready)
		return false;
	if (scale < 1)
		scale = 1;
	if (scale > GPU_FONT_MAX_SCALE)
		scale = GPU_FONT_MAX_SCALE;
	font = &c->gpu->font[scale];
	if (!font->ready)
		return false;

	color = apply_display_color(color);
	fx = (float)x;
	line_top = (float)y;
	first = c->glyph_count;
	for (; *s; s++) {
		unsigned char ch = (unsigned char)*s;
		stbtt_aligned_quad q;
		float baseline;
		struct gpu_glyph *glyph;

		if (*s == '\n') {
			line_top += 8.0f * (float)scale;
			fx = (float)x;
			continue;
		}
		if (ch < GPU_FONT_FIRST || ch >= GPU_FONT_FIRST + GPU_FONT_COUNT)
			ch = '?';
		baseline = line_top + (float)font->ascent;
		stbtt_GetBakedQuad(font->chars, font->width, font->height,
			ch - GPU_FONT_FIRST, &fx, &baseline, &q, 1);
		if (q.x1 <= q.x0 || q.y1 <= q.y0)
			continue;
		if (c->glyph_count >= c->glyph_cap) {
			c->gpu_overflow = true;
			break;
		}
		glyph = &c->glyphs[c->glyph_count++];
		glyph->x0 = q.x0;
		glyph->y0 = q.y0;
		glyph->x1 = q.x1;
		glyph->y1 = q.y1;
		glyph->u0 = q.s0;
		glyph->v0 = q.t0;
		glyph->u1 = q.s1;
		glyph->v1 = q.t1;
		glyph->color = color;
		glyph->scale = (uint8_t)scale;
		count++;
	}
	return gpu_add_op(c, GPU_OP_TEXT, (uint8_t)scale, first, count);
}
#endif

static void rect(struct canvas *c, int x, int y, int w, int h, uint32_t color)
{
	int xx, yy;
	color = apply_display_color(color);
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > c->width) w = c->width - x;
	if (y + h > c->height) h = c->height - y;
	if (w <= 0 || h <= 0)
		return;
#ifdef ALIOTH_GPU_RENDERER
	if (c->gpu_record) {
		if (c->rect_count < c->rect_cap) {
			size_t first = c->rect_count;
			struct gpu_rect *r = &c->rects[c->rect_count++];
			r->x = x;
			r->y = y;
			r->w = w;
			r->h = h;
			r->color = color;
			gpu_add_op(c, GPU_OP_RECT, 0, first, 1);
		} else {
			c->gpu_overflow = true;
		}
		return;
	}
#endif
	for (yy = y; yy < y + h; yy++) {
		uint32_t *row = (uint32_t *)(void *)(c->data + yy * c->pitch + x * 4);
		for (xx = 0; xx < w; xx++)
			row[xx] = color;
	}
}

static void text(struct canvas *c, int x, int y, const char *s, int scale, uint32_t color)
{
	int ox = x;
#ifdef ALIOTH_GPU_RENDERER
	if (c->gpu_record && gpu_record_text(c, x, y, s, scale, color))
		return;
#endif
	for (; *s; s++) {
		uint8_t glyph[7];
		int gy, gx;
		if (*s == '\n') {
			y += 8 * scale;
			x = ox;
			continue;
		}
		glyph_for(*s, glyph);
		for (gy = 0; gy < 7; gy++) {
			for (gx = 0; gx < 5; gx++) {
				if (glyph[gy] & (1u << (4 - gx)))
					rect(c, x + gx * scale, y + gy * scale, scale, scale, color);
			}
		}
		x += 6 * scale;
	}
}

static int text_width(struct canvas *c, const char *s, int scale)
{
	int fallback = 0;
#ifdef ALIOTH_GPU_RENDERER
	if (c->gpu_record && c->gpu && c->gpu->font_ready) {
		struct gpu_font_atlas *font;
		float fx = 0.0f, line_width = 0.0f, max_width = 0.0f;
		float baseline;

		if (scale < 1)
			scale = 1;
		if (scale > GPU_FONT_MAX_SCALE)
			scale = GPU_FONT_MAX_SCALE;
		font = &c->gpu->font[scale];
		if (font->ready) {
			for (; *s; s++) {
				unsigned char ch = (unsigned char)*s;
				stbtt_aligned_quad q;
				if (*s == '\n') {
					if (line_width > max_width)
						max_width = line_width;
					fx = 0.0f;
					line_width = 0.0f;
					continue;
				}
				if (ch < GPU_FONT_FIRST || ch >= GPU_FONT_FIRST + GPU_FONT_COUNT)
					ch = '?';
				baseline = (float)font->ascent;
				stbtt_GetBakedQuad(font->chars, font->width, font->height,
					ch - GPU_FONT_FIRST, &fx, &baseline, &q, 1);
				line_width = fx;
			}
			if (line_width > max_width)
				max_width = line_width;
			return (int)(max_width + 0.5f);
		}
	}
#else
	(void)c;
#endif
	for (; *s; s++) {
		if (*s == '\n') {
			if (fallback < 0)
				fallback = 0;
			continue;
		}
		fallback += 6 * scale;
	}
	return fallback;
}

static void bar(struct canvas *c, int x, int y, int w, int h, double pct, uint32_t bg, uint32_t fg)
{
	if (pct < 0) pct = 0;
	if (pct > 1) pct = 1;
	rect(c, x, y, w, h, bg);
	rect(c, x, y, (int)(w * pct), h, fg);
}

static bool write_all_fd(int fd, const void *buf, size_t len)
{
	const uint8_t *p = buf;

	while (len > 0) {
		ssize_t n = write(fd, p, len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		if (n == 0)
			return false;
		p += n;
		len -= (size_t)n;
	}
	return true;
}

static void put_le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
}

static void put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff);
	p[3] = (uint8_t)((v >> 24) & 0xff);
}

static bool write_canvas_bmp(struct canvas *c, const char *path)
{
	int fd;
	uint8_t header[54];
	uint8_t *row = NULL;
	int row_stride = ((c->width * 3 + 3) / 4) * 4;
	uint32_t file_size = 54u + (uint32_t)row_stride * (uint32_t)c->height;
	int y, x;
	bool ok = false;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0)
		return false;
	row = calloc(1, (size_t)row_stride);
	if (!row)
		goto out;

	memset(header, 0, sizeof(header));
	header[0] = 'B';
	header[1] = 'M';
	put_le32(header + 2, file_size);
	put_le32(header + 10, 54);
	put_le32(header + 14, 40);
	put_le32(header + 18, (uint32_t)c->width);
	put_le32(header + 22, (uint32_t)c->height);
	put_le16(header + 26, 1);
	put_le16(header + 28, 24);
	put_le32(header + 34, (uint32_t)row_stride * (uint32_t)c->height);
	if (!write_all_fd(fd, header, sizeof(header)))
		goto out;

	for (y = c->height - 1; y >= 0; y--) {
		uint32_t *src = (uint32_t *)(void *)(c->data + y * c->pitch);
		memset(row, 0, (size_t)row_stride);
		for (x = 0; x < c->width; x++) {
			uint32_t pix = src[x];
			row[x * 3 + 0] = (uint8_t)(pix & 0xff);
			row[x * 3 + 1] = (uint8_t)((pix >> 8) & 0xff);
			row[x * 3 + 2] = (uint8_t)((pix >> 16) & 0xff);
		}
		if (!write_all_fd(fd, row, (size_t)row_stride))
			goto out;
	}
	ok = true;
out:
	free(row);
	close(fd);
	return ok;
}

static void maybe_write_screenshot(struct canvas *c)
{
	int info_fd;
	char info[256];
	int n;

	if (access(SCREENSHOT_REQUEST_PATH, F_OK) != 0)
		return;
	unlink(SCREENSHOT_REQUEST_PATH);
	if (!write_canvas_bmp(c, SCREENSHOT_BMP_PATH)) {
		log_msg("screenshot write failed: %s", strerror(errno));
		return;
	}
	info_fd = open(SCREENSHOT_INFO_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (info_fd >= 0) {
		n = snprintf(info, sizeof(info),
			"path=%s\nwidth=%d\nheight=%d\npage=%s\nscreen=%d\nwritten_ms=%lld\n",
			SCREENSHOT_BMP_PATH, c->width, c->height, page_name(g_app_page),
			(int)g_ui_screen, (long long)monotonic_ms());
		if (n > 0)
			write_all_fd(info_fd, info, (size_t)n);
		close(info_fd);
	}
	log_msg("screenshot written %s", SCREENSHOT_BMP_PATH);
}

static uint32_t first_possible_crtc(uint32_t mask, uint32_t *crtcs, uint32_t count)
{
	uint32_t i;
	for (i = 0; i < count; i++) {
		if (mask & (1u << i))
			return crtcs[i];
	}
	return 0;
}

static int drm_mode_refresh_hz(const struct drm_mode_modeinfo *mode)
{
	uint64_t denom;

	if (mode->vrefresh > 0)
		return (int)mode->vrefresh;
	if (!mode->clock || !mode->htotal || !mode->vtotal)
		return 0;
	denom = (uint64_t)mode->htotal * (uint64_t)mode->vtotal;
	if (mode->vscan > 1)
		denom *= mode->vscan;
	if (denom == 0)
		return 0;
	return (int)(((uint64_t)mode->clock * 1000u + denom / 2u) / denom);
}

static int choose_display_mode(const struct drm_mode_modeinfo *modes, uint32_t count)
{
	uint32_t i;
	int best = 0;
	uint32_t best_area = 0;
	int best_refresh = 0;

	for (i = 0; i < count; i++) {
		uint32_t area = (uint32_t)modes[i].hdisplay * (uint32_t)modes[i].vdisplay;
		int refresh = drm_mode_refresh_hz(&modes[i]);

		if (area > best_area || (area == best_area && refresh > best_refresh)) {
			best = (int)i;
			best_area = area;
			best_refresh = refresh;
		}
	}
	return best;
}

static int open_card(void)
{
	int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd >= 0)
		return fd;
	return open("/dev/graphics/card0", O_RDWR | O_CLOEXEC);
}

static bool setup_drm(struct drm_state *d)
{
	struct drm_mode_card_res res;
	uint32_t *connectors = NULL, *crtcs = NULL, *encoders = NULL;
	struct drm_mode_get_connector chosen;
	struct drm_mode_modeinfo *modes = NULL;
	uint32_t *encoder_ids = NULL;
	uint32_t chosen_crtc = 0;
	uint32_t i;

	memset(d, 0, sizeof(*d));
#ifdef ALIOTH_GPU_RENDERER
	d->prime_fd[0] = -1;
	d->prime_fd[1] = -1;
#endif
	d->fd = open_card();
	if (d->fd < 0) {
		log_msg("open drm card failed: %s", strerror(errno));
		return false;
	}

	memset(&res, 0, sizeof(res));
	if (ioctl(d->fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		log_msg("GETRESOURCES phase1 failed: %s", strerror(errno));
		return false;
	}
	if (!res.count_connectors || !res.count_crtcs) {
		log_msg("missing drm resources connectors=%u crtcs=%u", res.count_connectors, res.count_crtcs);
		return false;
	}

	connectors = calloc(res.count_connectors, sizeof(uint32_t));
	crtcs = calloc(res.count_crtcs, sizeof(uint32_t));
	encoders = calloc(res.count_encoders ? res.count_encoders : 1, sizeof(uint32_t));
	if (!connectors || !crtcs || !encoders)
		return false;

	res.connector_id_ptr = (uint64_t)(uintptr_t)connectors;
	res.crtc_id_ptr = (uint64_t)(uintptr_t)crtcs;
	res.encoder_id_ptr = (uint64_t)(uintptr_t)encoders;
	if (ioctl(d->fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		log_msg("GETRESOURCES phase2 failed: %s", strerror(errno));
		goto fail;
	}

	memset(&chosen, 0, sizeof(chosen));
	for (i = 0; i < res.count_connectors; i++) {
		struct drm_mode_get_connector c;
		uint32_t *props = NULL;
		uint64_t *prop_values = NULL;

		memset(&c, 0, sizeof(c));
		c.connector_id = connectors[i];
		if (ioctl(d->fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0)
			continue;
		if (c.connection != DRM_CONNECTED || c.count_modes == 0)
			continue;

		modes = calloc(c.count_modes, sizeof(*modes));
		encoder_ids = calloc(c.count_encoders ? c.count_encoders : 1, sizeof(uint32_t));
		props = calloc(c.count_props ? c.count_props : 1, sizeof(uint32_t));
		prop_values = calloc(c.count_props ? c.count_props : 1, sizeof(uint64_t));
		if (!modes || !encoder_ids || !props || !prop_values) {
			free(props);
			free(prop_values);
			goto fail;
		}

		c.modes_ptr = (uint64_t)(uintptr_t)modes;
		c.encoders_ptr = (uint64_t)(uintptr_t)encoder_ids;
		c.props_ptr = (uint64_t)(uintptr_t)props;
		c.prop_values_ptr = (uint64_t)(uintptr_t)prop_values;
		if (ioctl(d->fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) == 0) {
			chosen = c;
			free(props);
			free(prop_values);
			break;
		}
		free(props);
		free(prop_values);
		free(modes);
		free(encoder_ids);
		modes = NULL;
		encoder_ids = NULL;
	}
	if (!chosen.connector_id || !modes) {
		log_msg("no connected drm connector with modes");
		goto fail;
	}

	if (chosen.encoder_id) {
		struct drm_mode_get_encoder enc;
		memset(&enc, 0, sizeof(enc));
		enc.encoder_id = chosen.encoder_id;
		if (ioctl(d->fd, DRM_IOCTL_MODE_GETENCODER, &enc) == 0) {
			chosen_crtc = enc.crtc_id;
			if (!chosen_crtc)
				chosen_crtc = first_possible_crtc(enc.possible_crtcs, crtcs, res.count_crtcs);
		}
	}
	if (!chosen_crtc) {
		for (i = 0; i < chosen.count_encoders; i++) {
			struct drm_mode_get_encoder enc;
			memset(&enc, 0, sizeof(enc));
			enc.encoder_id = encoder_ids[i];
			if (ioctl(d->fd, DRM_IOCTL_MODE_GETENCODER, &enc) == 0) {
				chosen_crtc = enc.crtc_id;
				if (!chosen_crtc)
					chosen_crtc = first_possible_crtc(enc.possible_crtcs, crtcs, res.count_crtcs);
				if (chosen_crtc)
					break;
			}
		}
	}
	if (!chosen_crtc && res.count_crtcs)
		chosen_crtc = crtcs[0];
	if (!chosen_crtc) {
		log_msg("no usable drm crtc");
		goto fail;
	}

	d->connector_id = chosen.connector_id;
	d->crtc_id = chosen_crtc;
	d->mode = modes[choose_display_mode(modes, chosen.count_modes)];
	d->width = d->mode.hdisplay;
	d->height = d->mode.vdisplay;
	g_panel_refresh_hz = drm_mode_refresh_hz(&d->mode);
	snprintf(g_panel_mode_name, sizeof(g_panel_mode_name), "%ux%u@%dHz",
		d->mode.hdisplay, d->mode.vdisplay, g_panel_refresh_hz);

	for (i = 0; i < 2; i++) {
		struct drm_mode_create_dumb dumb;
		struct drm_mode_fb_cmd2 fb;
		struct drm_mode_map_dumb map;

		memset(&dumb, 0, sizeof(dumb));
		dumb.width = d->width;
		dumb.height = d->height;
		dumb.bpp = 32;
		if (ioctl(d->fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb) < 0) {
			log_msg("CREATE_DUMB[%u] failed: %s", i, strerror(errno));
			goto fail;
		}

		d->handle[i] = dumb.handle;
		if (i == 0) {
			d->pitch = dumb.pitch;
			d->size = dumb.size;
		} else if (d->pitch != (int)dumb.pitch || d->size != dumb.size) {
			log_msg("dumb buffer layout mismatch");
			goto fail;
		}

		memset(&fb, 0, sizeof(fb));
		fb.width = d->width;
		fb.height = d->height;
		fb.pixel_format = DRM_FORMAT_XRGB8888;
		fb.handles[0] = d->handle[i];
		fb.pitches[0] = d->pitch;
		if (ioctl(d->fd, DRM_IOCTL_MODE_ADDFB2, &fb) < 0) {
			log_msg("ADDFB2[%u] failed: %s", i, strerror(errno));
			goto fail;
		}
		d->fb_id[i] = fb.fb_id;

#ifdef ALIOTH_GPU_RENDERER
		{
			struct drm_prime_handle prime;
			memset(&prime, 0, sizeof(prime));
			prime.handle = d->handle[i];
			prime.flags = O_CLOEXEC;
			prime.fd = -1;
			if (ioctl(d->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) < 0) {
				log_msg("PRIME_HANDLE_TO_FD[%u] failed: %s", i, strerror(errno));
				goto fail;
			}
			d->prime_fd[i] = prime.fd;
		}
#endif

		memset(&map, 0, sizeof(map));
		map.handle = d->handle[i];
		if (ioctl(d->fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
			log_msg("MAP_DUMB[%u] failed: %s", i, strerror(errno));
			goto fail;
		}
		d->map[i] = mmap(NULL, d->size, PROT_READ | PROT_WRITE, MAP_SHARED,
			d->fd, (off_t)map.offset);
		if (d->map[i] == MAP_FAILED) {
			log_msg("mmap dumb[%u] failed: %s", i, strerror(errno));
			d->map[i] = NULL;
			goto fail;
		}
	}
	d->front = 0;

	log_msg("display ready %dx%d@%d pitch=%d connector=%u crtc=%u mode=%s",
		d->width, d->height, g_panel_refresh_hz, d->pitch, d->connector_id,
		d->crtc_id, d->mode.name);
	free(connectors);
	free(crtcs);
	free(encoders);
	free(modes);
	free(encoder_ids);
	return true;

fail:
	free(connectors);
	free(crtcs);
	free(encoders);
	free(modes);
	free(encoder_ids);
	return false;
}

static bool modeset_fb(struct drm_state *d, int index)
{
	struct drm_mode_crtc crtc;
	uint32_t connector = d->connector_id;

	memset(&crtc, 0, sizeof(crtc));
	crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&connector;
	crtc.count_connectors = 1;
	crtc.crtc_id = d->crtc_id;
	crtc.fb_id = d->fb_id[index];
	crtc.mode_valid = 1;
	crtc.mode = d->mode;
	if (ioctl(d->fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
		log_msg("SETCRTC fb[%d] failed: %s", index, strerror(errno));
		return false;
	}
	return true;
}

static void wait_page_flip_event(struct drm_state *d)
{
	struct pollfd pfd;
	char buf[256];
	int rc;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = d->fd;
	pfd.events = POLLIN;

	do {
		rc = poll(&pfd, 1, 2000);
	} while (rc < 0 && errno == EINTR);

	if (rc < 0) {
		log_msg("PAGE_FLIP event wait failed: %s", strerror(errno));
		return;
	}
	if (rc == 0) {
		log_msg("PAGE_FLIP event wait timed out");
		return;
	}
	if (!(pfd.revents & POLLIN))
		return;

	while (read(d->fd, buf, sizeof(buf)) < 0 && errno == EINTR)
		;
}

static bool present_fb(struct drm_state *d, int index)
{
	struct drm_mode_crtc_page_flip flip;

	if (index == d->front)
		return true;

	memset(&flip, 0, sizeof(flip));
	flip.crtc_id = d->crtc_id;
	flip.fb_id = d->fb_id[index];
	flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
	if (ioctl(d->fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip) < 0) {
		log_msg("PAGE_FLIP fb[%d] failed: %s; falling back SETCRTC", index, strerror(errno));
		if (!modeset_fb(d, index))
			return false;
		d->front = index;
		return true;
	}

	wait_page_flip_event(d);
	d->front = index;
	return true;
}

static void dirty_fb(struct drm_state *d, int index)
{
	struct drm_clip_rect clip = {0, 0, (uint16_t)d->width, (uint16_t)d->height};
	struct drm_mode_fb_dirty_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.fb_id = d->fb_id[index];
	cmd.num_clips = 1;
	cmd.clips_ptr = (uint64_t)(uintptr_t)&clip;
	(void)ioctl(d->fd, DRM_IOCTL_MODE_DIRTYFB, &cmd);
}

static void dirty(struct drm_state *d)
{
	dirty_fb(d, d->front);
}

#ifdef ALIOTH_GPU_RENDERER
static int gpu_vk_ok(VkResult result, const char *label)
{
	if (result == VK_SUCCESS)
		return 0;
	log_msg("%s failed: VkResult=%d", label, result);
	return -1;
}

static bool gpu_has_extension(const VkExtensionProperties *exts, uint32_t count, const char *name)
{
	uint32_t i;
	for (i = 0; i < count; i++) {
		if (strcmp(exts[i].extensionName, name) == 0)
			return true;
	}
	return false;
}

static int gpu_find_memory_type_any(VkPhysicalDevice phys, uint32_t type_bits, uint32_t *type_index)
{
	VkPhysicalDeviceMemoryProperties props;
	uint32_t i;

	vkGetPhysicalDeviceMemoryProperties(phys, &props);
	for (i = 0; i < props.memoryTypeCount; i++) {
		if (type_bits & (1u << i)) {
			*type_index = i;
			return 0;
		}
	}
	return -1;
}

static int gpu_find_memory_type_flags(VkPhysicalDevice phys, uint32_t type_bits,
	VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred, uint32_t *type_index)
{
	VkPhysicalDeviceMemoryProperties props;
	int fallback = -1;
	uint32_t i;

	vkGetPhysicalDeviceMemoryProperties(phys, &props);
	for (i = 0; i < props.memoryTypeCount; i++) {
		VkMemoryPropertyFlags flags = props.memoryTypes[i].propertyFlags;
		if (!(type_bits & (1u << i)))
			continue;
		if ((flags & required) != required)
			continue;
		if ((flags & preferred) == preferred) {
			*type_index = i;
			return 0;
		}
		if (fallback < 0)
			fallback = (int)i;
	}
	if (fallback >= 0) {
		*type_index = (uint32_t)fallback;
		return 0;
	}
	return -1;
}

static bool gpu_read_spv(const char *path, uint32_t **words, size_t *word_count)
{
	int fd;
	struct stat st;
	uint8_t *buf = NULL;
	size_t len;

	*words = NULL;
	*word_count = 0;
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		log_msg("open shader %s failed: %s", path, strerror(errno));
		return false;
	}
	if (fstat(fd, &st) != 0 || st.st_size <= 0 || (st.st_size % 4) != 0) {
		log_msg("bad shader size %s", path);
		close(fd);
		return false;
	}
	len = (size_t)st.st_size;
	buf = malloc(len);
	if (!buf) {
		close(fd);
		return false;
	}
	size_t off = 0;
	while (off < len) {
		ssize_t n = read(fd, buf + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			free(buf);
			close(fd);
			return false;
		}
		if (n == 0)
			break;
		off += (size_t)n;
	}
	close(fd);
	if (off != len) {
		free(buf);
		return false;
	}
	*words = (uint32_t *)(void *)buf;
	*word_count = len / 4;
	return true;
}

static bool gpu_read_binary(const char *path, uint8_t **data, size_t *size)
{
	int fd;
	struct stat st;
	uint8_t *buf = NULL;
	size_t len, off = 0;

	*data = NULL;
	*size = 0;
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	if (fstat(fd, &st) != 0 || st.st_size <= 0) {
		close(fd);
		return false;
	}
	len = (size_t)st.st_size;
	buf = malloc(len);
	if (!buf) {
		close(fd);
		return false;
	}
	while (off < len) {
		ssize_t n = read(fd, buf + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			free(buf);
			close(fd);
			return false;
		}
		if (n == 0)
			break;
		off += (size_t)n;
	}
	close(fd);
	if (off != len) {
		free(buf);
		return false;
	}
	*data = buf;
	*size = len;
	return true;
}

static float gpu_ndc_x(int x, int width)
{
	return ((float)x * 2.0f / (float)width) - 1.0f;
}

static float gpu_ndc_y(int y, int height)
{
	return ((float)y * 2.0f / (float)height) - 1.0f;
}

static float gpu_ndc_xf(float x, int width)
{
	return (x * 2.0f / (float)width) - 1.0f;
}

static float gpu_ndc_yf(float y, int height)
{
	return (y * 2.0f / (float)height) - 1.0f;
}

static void gpu_set_vertex_uv(struct gpu_vertex *v, float x, float y, int width, int height,
	float u, float t, uint32_t color)
{
	v->pos[0] = gpu_ndc_xf(x, width);
	v->pos[1] = gpu_ndc_yf(y, height);
	v->uv[0] = u;
	v->uv[1] = t;
	v->color[0] = (float)((color >> 16) & 0xff) / 255.0f;
	v->color[1] = (float)((color >> 8) & 0xff) / 255.0f;
	v->color[2] = (float)(color & 0xff) / 255.0f;
}

static void gpu_set_vertex(struct gpu_vertex *v, int x, int y, int width, int height, uint32_t color)
{
	gpu_set_vertex_uv(v, (float)x, (float)y, width, height, 0.0f, 0.0f, color);
}

static int gpu_create_buffer(struct gpu_state *g, VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred, VkBuffer *buffer,
	VkDeviceMemory *memory)
{
	VkBufferCreateInfo buffer_info;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo alloc;
	uint32_t memory_type = UINT32_MAX;

	memset(&buffer_info, 0, sizeof(buffer_info));
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (gpu_vk_ok(vkCreateBuffer(g->device, &buffer_info, NULL, buffer), "vkCreateBuffer") != 0)
		return -1;
	vkGetBufferMemoryRequirements(g->device, *buffer, &req);
	if (gpu_find_memory_type_flags(g->phys, req.memoryTypeBits, required, preferred, &memory_type) != 0) {
		log_msg("no memory type for gpu buffer bits=0x%x", req.memoryTypeBits);
		return -1;
	}
	memset(&alloc, 0, sizeof(alloc));
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = req.size;
	alloc.memoryTypeIndex = memory_type;
	if (gpu_vk_ok(vkAllocateMemory(g->device, &alloc, NULL, memory), "vkAllocateMemory(buffer)") != 0)
		return -1;
	if (gpu_vk_ok(vkBindBufferMemory(g->device, *buffer, *memory, 0), "vkBindBufferMemory") != 0)
		return -1;
	return 0;
}

static int gpu_init_vulkan(struct gpu_state *g)
{
	const char *required_exts[] = {
		"VK_KHR_external_memory",
		"VK_KHR_external_memory_fd",
		"VK_KHR_dedicated_allocation",
		"VK_KHR_bind_memory2",
		"VK_EXT_external_memory_dma_buf",
	};
	VkApplicationInfo app;
	VkInstanceCreateInfo instance_info;
	uint32_t phys_count = 0, queue_count = 0, ext_count = 0;
	VkPhysicalDevice *phys_list = NULL;
	VkQueueFamilyProperties *queues = NULL;
	VkExtensionProperties *exts = NULL;
	VkDeviceQueueCreateInfo queue_info;
	VkDeviceCreateInfo device_info;
	VkCommandPoolCreateInfo pool_info;
	float priority = 1.0f;
	uint32_t i;
	int rc = -1;

	memset(g, 0, sizeof(*g));
	g->queue_family = UINT32_MAX;
	memset(&app, 0, sizeof(app));
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "alioth-status-ui-gpu-full";
	app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app.pEngineName = "none";
	app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	app.apiVersion = VK_API_VERSION_1_1;
	memset(&instance_info, 0, sizeof(instance_info));
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pApplicationInfo = &app;
	if (gpu_vk_ok(vkCreateInstance(&instance_info, NULL, &g->instance), "vkCreateInstance") != 0)
		return -1;
	if (gpu_vk_ok(vkEnumeratePhysicalDevices(g->instance, &phys_count, NULL),
		"vkEnumeratePhysicalDevices(count)") != 0 || phys_count == 0)
		return -1;
	phys_list = calloc(phys_count, sizeof(*phys_list));
	if (!phys_list)
		return -1;
	if (gpu_vk_ok(vkEnumeratePhysicalDevices(g->instance, &phys_count, phys_list),
		"vkEnumeratePhysicalDevices(list)") != 0)
		goto out;
	g->phys = phys_list[0];
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(g->phys, &props);
		log_msg("gpu panel physical_device=%s api=%u.%u.%u",
			props.deviceName, VK_VERSION_MAJOR(props.apiVersion),
			VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
	}
	vkGetPhysicalDeviceQueueFamilyProperties(g->phys, &queue_count, NULL);
	queues = calloc(queue_count ? queue_count : 1, sizeof(*queues));
	if (!queues)
		goto out;
	vkGetPhysicalDeviceQueueFamilyProperties(g->phys, &queue_count, queues);
	for (i = 0; i < queue_count; i++) {
		if (queues[i].queueCount > 0 && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			g->queue_family = i;
			break;
		}
	}
	if (g->queue_family == UINT32_MAX) {
		log_msg("gpu panel no graphics queue");
		goto out;
	}
	if (gpu_vk_ok(vkEnumerateDeviceExtensionProperties(g->phys, NULL, &ext_count, NULL),
		"vkEnumerateDeviceExtensionProperties(count)") != 0)
		goto out;
	exts = calloc(ext_count ? ext_count : 1, sizeof(*exts));
	if (!exts)
		goto out;
	if (gpu_vk_ok(vkEnumerateDeviceExtensionProperties(g->phys, NULL, &ext_count, exts),
		"vkEnumerateDeviceExtensionProperties(list)") != 0)
		goto out;
	for (i = 0; i < sizeof(required_exts) / sizeof(required_exts[0]); i++) {
		if (!gpu_has_extension(exts, ext_count, required_exts[i])) {
			log_msg("gpu panel missing extension %s", required_exts[i]);
			goto out;
		}
	}
	memset(&queue_info, 0, sizeof(queue_info));
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.queueFamilyIndex = g->queue_family;
	queue_info.queueCount = 1;
	queue_info.pQueuePriorities = &priority;
	memset(&device_info, 0, sizeof(device_info));
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos = &queue_info;
	device_info.enabledExtensionCount = sizeof(required_exts) / sizeof(required_exts[0]);
	device_info.ppEnabledExtensionNames = required_exts;
	if (gpu_vk_ok(vkCreateDevice(g->phys, &device_info, NULL, &g->device), "vkCreateDevice") != 0)
		goto out;
	vkGetDeviceQueue(g->device, g->queue_family, 0, &g->queue);
	g->get_memory_fd_props = (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(g->device,
		"vkGetMemoryFdPropertiesKHR");
	if (!g->get_memory_fd_props) {
		log_msg("gpu panel missing vkGetMemoryFdPropertiesKHR");
		goto out;
	}
	memset(&pool_info, 0, sizeof(pool_info));
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	pool_info.queueFamilyIndex = g->queue_family;
	if (gpu_vk_ok(vkCreateCommandPool(g->device, &pool_info, NULL, &g->command_pool),
		"vkCreateCommandPool") != 0)
		goto out;
	rc = 0;
out:
	free(phys_list);
	free(queues);
	free(exts);
	return rc;
}

static int gpu_create_pipeline(struct gpu_state *g, uint32_t width, uint32_t height)
{
	uint32_t *vert_spv = NULL, *frag_spv = NULL, *text_frag_spv = NULL;
	size_t vert_words = 0, frag_words = 0, text_frag_words = 0;
	VkAttachmentDescription attachment;
	VkAttachmentReference color_ref;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[2];
	VkRenderPassCreateInfo render_pass_info;
	VkShaderModuleCreateInfo vert_info, frag_info, text_frag_info;
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription vertex_binding;
	VkVertexInputAttributeDescription vertex_attrs[3];
	VkPipelineVertexInputStateCreateInfo vertex_input;
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineColorBlendAttachmentState blend_attachment;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineLayoutCreateInfo pipeline_layout_info;
	VkDescriptorSetLayoutBinding text_binding;
	VkDescriptorSetLayoutCreateInfo text_layout_info;
	VkGraphicsPipelineCreateInfo pipeline_info;
	int rc = -1;

	if (!gpu_read_spv(GPU_PANEL_VERT_SPV, &vert_spv, &vert_words) ||
		!gpu_read_spv(GPU_PANEL_FRAG_SPV, &frag_spv, &frag_words) ||
		!gpu_read_spv(GPU_PANEL_TEXT_FRAG_SPV, &text_frag_spv, &text_frag_words))
		goto out;
	memset(&attachment, 0, sizeof(attachment));
	attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	memset(&color_ref, 0, sizeof(color_ref));
	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	memset(&subpass, 0, sizeof(subpass));
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	memset(deps, 0, sizeof(deps));
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_HOST_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[0].srcAccessMask = VK_ACCESS_HOST_READ_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	memset(&render_pass_info, 0, sizeof(render_pass_info));
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 2;
	render_pass_info.pDependencies = deps;
	if (gpu_vk_ok(vkCreateRenderPass(g->device, &render_pass_info, NULL, &g->render_pass),
		"vkCreateRenderPass") != 0)
		goto out;
	memset(&vert_info, 0, sizeof(vert_info));
	vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vert_info.codeSize = vert_words * sizeof(uint32_t);
	vert_info.pCode = vert_spv;
	memset(&frag_info, 0, sizeof(frag_info));
	frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	frag_info.codeSize = frag_words * sizeof(uint32_t);
	frag_info.pCode = frag_spv;
	memset(&text_frag_info, 0, sizeof(text_frag_info));
	text_frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	text_frag_info.codeSize = text_frag_words * sizeof(uint32_t);
	text_frag_info.pCode = text_frag_spv;
	if (gpu_vk_ok(vkCreateShaderModule(g->device, &vert_info, NULL, &g->vert_shader),
		"vkCreateShaderModule(vert)") != 0)
		goto out;
	if (gpu_vk_ok(vkCreateShaderModule(g->device, &frag_info, NULL, &g->frag_shader),
		"vkCreateShaderModule(frag)") != 0)
		goto out;
	if (gpu_vk_ok(vkCreateShaderModule(g->device, &text_frag_info, NULL, &g->text_frag_shader),
		"vkCreateShaderModule(text_frag)") != 0)
		goto out;
	memset(stages, 0, sizeof(stages));
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = g->vert_shader;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = g->frag_shader;
	stages[1].pName = "main";
	memset(&vertex_binding, 0, sizeof(vertex_binding));
	vertex_binding.binding = 0;
	vertex_binding.stride = sizeof(struct gpu_vertex);
	vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	memset(vertex_attrs, 0, sizeof(vertex_attrs));
	vertex_attrs[0].location = 0;
	vertex_attrs[0].binding = 0;
	vertex_attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_attrs[0].offset = offsetof(struct gpu_vertex, pos);
	vertex_attrs[1].location = 1;
	vertex_attrs[1].binding = 0;
	vertex_attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_attrs[1].offset = offsetof(struct gpu_vertex, uv);
	vertex_attrs[2].location = 2;
	vertex_attrs[2].binding = 0;
	vertex_attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertex_attrs[2].offset = offsetof(struct gpu_vertex, color);
	memset(&vertex_input, 0, sizeof(vertex_input));
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &vertex_binding;
	vertex_input.vertexAttributeDescriptionCount = 3;
	vertex_input.pVertexAttributeDescriptions = vertex_attrs;
	memset(&input_assembly, 0, sizeof(input_assembly));
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	memset(&viewport, 0, sizeof(viewport));
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.maxDepth = 1.0f;
	memset(&scissor, 0, sizeof(scissor));
	scissor.extent.width = width;
	scissor.extent.height = height;
	memset(&viewport_state, 0, sizeof(viewport_state));
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;
	memset(&raster, 0, sizeof(raster));
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth = 1.0f;
	memset(&multisample, 0, sizeof(multisample));
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	memset(&blend_attachment, 0, sizeof(blend_attachment));
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	memset(&blend, 0, sizeof(blend));
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_attachment;
	memset(&pipeline_layout_info, 0, sizeof(pipeline_layout_info));
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (gpu_vk_ok(vkCreatePipelineLayout(g->device, &pipeline_layout_info, NULL,
		&g->pipeline_layout), "vkCreatePipelineLayout") != 0)
		goto out;
	memset(&text_binding, 0, sizeof(text_binding));
	text_binding.binding = 0;
	text_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	text_binding.descriptorCount = 1;
	text_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	memset(&text_layout_info, 0, sizeof(text_layout_info));
	text_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	text_layout_info.bindingCount = 1;
	text_layout_info.pBindings = &text_binding;
	if (gpu_vk_ok(vkCreateDescriptorSetLayout(g->device, &text_layout_info, NULL,
		&g->text_descriptor_layout), "vkCreateDescriptorSetLayout(text)") != 0)
		goto out;
	memset(&pipeline_layout_info, 0, sizeof(pipeline_layout_info));
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &g->text_descriptor_layout;
	if (gpu_vk_ok(vkCreatePipelineLayout(g->device, &pipeline_layout_info, NULL,
		&g->text_pipeline_layout), "vkCreatePipelineLayout(text)") != 0)
		goto out;
	memset(&pipeline_info, 0, sizeof(pipeline_info));
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = stages;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &raster;
	pipeline_info.pMultisampleState = &multisample;
	pipeline_info.pColorBlendState = &blend;
	pipeline_info.layout = g->pipeline_layout;
	pipeline_info.renderPass = g->render_pass;
	if (gpu_vk_ok(vkCreateGraphicsPipelines(g->device, VK_NULL_HANDLE, 1,
		&pipeline_info, NULL, &g->pipeline), "vkCreateGraphicsPipelines") != 0)
		goto out;
	stages[1].module = g->text_frag_shader;
	blend_attachment.blendEnable = VK_TRUE;
	blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	pipeline_info.layout = g->text_pipeline_layout;
	if (gpu_vk_ok(vkCreateGraphicsPipelines(g->device, VK_NULL_HANDLE, 1,
		&pipeline_info, NULL, &g->text_pipeline), "vkCreateGraphicsPipelines(text)") != 0)
		goto out;
	rc = 0;
out:
	free(vert_spv);
	free(frag_spv);
	free(text_frag_spv);
	return rc;
}

static int gpu_create_image_memory(struct gpu_state *g, VkImage image,
	VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred, VkDeviceMemory *memory)
{
	VkMemoryRequirements req;
	VkMemoryAllocateInfo alloc;
	uint32_t memory_type = UINT32_MAX;

	vkGetImageMemoryRequirements(g->device, image, &req);
	if (gpu_find_memory_type_flags(g->phys, req.memoryTypeBits, required, preferred,
		&memory_type) != 0) {
		log_msg("no memory type for image bits=0x%x", req.memoryTypeBits);
		return -1;
	}
	memset(&alloc, 0, sizeof(alloc));
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = req.size;
	alloc.memoryTypeIndex = memory_type;
	if (gpu_vk_ok(vkAllocateMemory(g->device, &alloc, NULL, memory),
		"vkAllocateMemory(image)") != 0)
		return -1;
	if (gpu_vk_ok(vkBindImageMemory(g->device, image, *memory, 0),
		"vkBindImageMemory(image)") != 0)
		return -1;
	return 0;
}

static int gpu_upload_font_bitmap(struct gpu_state *g, struct gpu_font_atlas *font,
	const uint8_t *bitmap, size_t bitmap_size)
{
	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory staging_memory = VK_NULL_HANDLE;
	VkImageCreateInfo image_info;
	VkImageViewCreateInfo view_info;
	VkSamplerCreateInfo sampler_info;
	VkCommandBufferAllocateInfo cmd_alloc;
	VkCommandBufferBeginInfo begin_info;
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	VkImageSubresourceRange range;
	VkImageMemoryBarrier barrier;
	VkBufferImageCopy copy;
	VkSubmitInfo submit;
	void *mapped = NULL;
	int rc = -1;

	if (gpu_create_buffer(g, (VkDeviceSize)bitmap_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_memory) != 0)
		return -1;
	if (gpu_vk_ok(vkMapMemory(g->device, staging_memory, 0, (VkDeviceSize)bitmap_size, 0,
		&mapped), "vkMapMemory(font staging)") != 0)
		goto out;
	memcpy(mapped, bitmap, bitmap_size);
	vkUnmapMemory(g->device, staging_memory);
	mapped = NULL;

	memset(&image_info, 0, sizeof(image_info));
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = VK_FORMAT_R8_UNORM;
	image_info.extent.width = (uint32_t)font->width;
	image_info.extent.height = (uint32_t)font->height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (gpu_vk_ok(vkCreateImage(g->device, &image_info, NULL, &font->image),
		"vkCreateImage(font)") != 0)
		goto out;
	if (gpu_create_image_memory(g, font->image, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&font->memory) != 0)
		goto out;

	memset(&cmd_alloc, 0, sizeof(cmd_alloc));
	cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_alloc.commandPool = g->command_pool;
	cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_alloc.commandBufferCount = 1;
	if (gpu_vk_ok(vkAllocateCommandBuffers(g->device, &cmd_alloc, &command_buffer),
		"vkAllocateCommandBuffers(font)") != 0)
		goto out;
	memset(&begin_info, 0, sizeof(begin_info));
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (gpu_vk_ok(vkBeginCommandBuffer(command_buffer, &begin_info),
		"vkBeginCommandBuffer(font)") != 0)
		goto out;
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = 1;
	range.layerCount = 1;
	memset(&barrier, 0, sizeof(barrier));
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = font->image;
	barrier.subresourceRange = range;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
	memset(&copy, 0, sizeof(copy));
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent.width = (uint32_t)font->width;
	copy.imageExtent.height = (uint32_t)font->height;
	copy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(command_buffer, staging, font->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
	if (gpu_vk_ok(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(font)") != 0)
		goto out;
	memset(&submit, 0, sizeof(submit));
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &command_buffer;
	if (gpu_vk_ok(vkQueueSubmit(g->queue, 1, &submit, VK_NULL_HANDLE),
		"vkQueueSubmit(font)") != 0)
		goto out;
	if (gpu_vk_ok(vkQueueWaitIdle(g->queue), "vkQueueWaitIdle(font)") != 0)
		goto out;

	memset(&view_info, 0, sizeof(view_info));
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = font->image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = VK_FORMAT_R8_UNORM;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	if (gpu_vk_ok(vkCreateImageView(g->device, &view_info, NULL, &font->view),
		"vkCreateImageView(font)") != 0)
		goto out;
	memset(&sampler_info, 0, sizeof(sampler_info));
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.maxLod = 0.0f;
	if (gpu_vk_ok(vkCreateSampler(g->device, &sampler_info, NULL, &font->sampler),
		"vkCreateSampler(font)") != 0)
		goto out;
	rc = 0;
out:
	if (command_buffer != VK_NULL_HANDLE)
		vkFreeCommandBuffers(g->device, g->command_pool, 1, &command_buffer);
	if (staging != VK_NULL_HANDLE)
		vkDestroyBuffer(g->device, staging, NULL);
	if (staging_memory != VK_NULL_HANDLE)
		vkFreeMemory(g->device, staging_memory, NULL);
	return rc;
}

static int gpu_build_font_atlas(struct gpu_state *g, stbtt_fontinfo *font_info,
	int font_offset, int scale)
{
	struct gpu_font_atlas *font = &g->font[scale];
	uint8_t *bitmap = NULL;
	int ascent, descent, line_gap;
	float sf;
	int bake_rc;
	size_t bitmap_size;
	VkDescriptorSetAllocateInfo set_alloc;
	VkDescriptorImageInfo image_desc;
	VkWriteDescriptorSet write_desc;
	int rc = -1;

	memset(font, 0, sizeof(*font));
	font->scale = scale;
	font->pixel_height = scale * 9;
	font->width = scale <= 4 ? 512 : 1024;
	font->height = scale <= 5 ? 512 : 1024;
	bitmap_size = (size_t)font->width * (size_t)font->height;
	bitmap = calloc(1, bitmap_size);
	if (!bitmap)
		return -1;
	bake_rc = stbtt_BakeFontBitmap(g->font_data, font_offset, (float)font->pixel_height,
		bitmap, font->width, font->height, GPU_FONT_FIRST, GPU_FONT_COUNT, font->chars);
	if (bake_rc <= 0) {
		log_msg("gpu font bake failed scale=%d rc=%d", scale, bake_rc);
		goto out;
	}
	stbtt_GetFontVMetrics(font_info, &ascent, &descent, &line_gap);
	(void)descent;
	(void)line_gap;
	sf = stbtt_ScaleForPixelHeight(font_info, (float)font->pixel_height);
	font->ascent = (int)((float)ascent * sf + 0.5f);
	if (font->ascent <= 0)
		font->ascent = font->pixel_height;
	if (gpu_upload_font_bitmap(g, font, bitmap, bitmap_size) != 0)
		goto out;
	memset(&set_alloc, 0, sizeof(set_alloc));
	set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set_alloc.descriptorPool = g->text_descriptor_pool;
	set_alloc.descriptorSetCount = 1;
	set_alloc.pSetLayouts = &g->text_descriptor_layout;
	if (gpu_vk_ok(vkAllocateDescriptorSets(g->device, &set_alloc, &font->descriptor_set),
		"vkAllocateDescriptorSets(font)") != 0)
		goto out;
	memset(&image_desc, 0, sizeof(image_desc));
	image_desc.sampler = font->sampler;
	image_desc.imageView = font->view;
	image_desc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	memset(&write_desc, 0, sizeof(write_desc));
	write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_desc.dstSet = font->descriptor_set;
	write_desc.dstBinding = 0;
	write_desc.descriptorCount = 1;
	write_desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write_desc.pImageInfo = &image_desc;
	vkUpdateDescriptorSets(g->device, 1, &write_desc, 0, NULL);
	font->ready = true;
	rc = 0;
out:
	free(bitmap);
	return rc;
}

static int gpu_init_fonts(struct gpu_state *g)
{
	static const char *font_paths[] = {
		"/system/fonts/Roboto-Regular.ttf",
		"/system/fonts/MiSansVF.ttf",
		"/system/fonts/DroidSansMono.ttf",
	};
	VkDescriptorPoolSize pool_size;
	VkDescriptorPoolCreateInfo pool_info;
	stbtt_fontinfo font_info;
	const char *chosen = NULL;
	int font_offset;
	int scale;
	size_t i;

	for (i = 0; i < sizeof(font_paths) / sizeof(font_paths[0]); i++) {
		if (gpu_read_binary(font_paths[i], &g->font_data, &g->font_size)) {
			chosen = font_paths[i];
			break;
		}
	}
	if (!chosen) {
		log_msg("gpu font atlas unavailable: no Android font file");
		return -1;
	}
	font_offset = stbtt_GetFontOffsetForIndex(g->font_data, 0);
	if (font_offset < 0)
		font_offset = 0;
	if (!stbtt_InitFont(&font_info, g->font_data, font_offset)) {
		log_msg("gpu font atlas unavailable: bad font %s", chosen);
		return -1;
	}
	memset(&pool_size, 0, sizeof(pool_size));
	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = GPU_FONT_MAX_SCALE;
	memset(&pool_info, 0, sizeof(pool_info));
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = GPU_FONT_MAX_SCALE;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	if (gpu_vk_ok(vkCreateDescriptorPool(g->device, &pool_info, NULL,
		&g->text_descriptor_pool), "vkCreateDescriptorPool(text)") != 0)
		return -1;
	for (scale = 1; scale <= GPU_FONT_MAX_SCALE; scale++) {
		if (gpu_build_font_atlas(g, &font_info, font_offset, scale) != 0)
			return -1;
	}
	g->font_ready = true;
	log_msg("gpu font atlas ready path=%s scales=1..%d pixel=scale*9 renderer=stb_truetype",
		chosen, GPU_FONT_MAX_SCALE);
	return 0;
}

static int gpu_import_drm_buffer(struct gpu_state *g, struct drm_state *d, int index)
{
	struct gpu_imported_image *img = &g->image[index];
	VkExternalMemoryImageCreateInfo external_image;
	VkImageCreateInfo image_info;
	VkMemoryRequirements req;
	VkMemoryFdPropertiesKHR fd_props;
	uint32_t memory_type = UINT32_MAX;
	int import_fd;
	VkMemoryDedicatedAllocateInfo dedicated;
	VkImportMemoryFdInfoKHR import_info;
	VkMemoryAllocateInfo alloc;
	VkImageViewCreateInfo view_info;
	VkFramebufferCreateInfo fb_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	uint32_t combined_bits;

	memset(img, 0, sizeof(*img));
	img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	memset(&external_image, 0, sizeof(external_image));
	external_image.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	external_image.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
	memset(&image_info, 0, sizeof(image_info));
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = &external_image;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = VK_FORMAT_B8G8R8A8_UNORM;
	image_info.extent.width = (uint32_t)d->width;
	image_info.extent.height = (uint32_t)d->height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_LINEAR;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (gpu_vk_ok(vkCreateImage(g->device, &image_info, NULL, &img->image),
		"vkCreateImage(imported)") != 0)
		return -1;
	memset(&subresource, 0, sizeof(subresource));
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkGetImageSubresourceLayout(g->device, img->image, &subresource, &layout);
	if (layout.offset != 0 || layout.rowPitch != (uint64_t)d->pitch || layout.size > d->size) {
		log_msg("gpu imported layout mismatch rowPitch=%llu pitch=%d size=%llu dumb=%zu",
			(unsigned long long)layout.rowPitch, d->pitch,
			(unsigned long long)layout.size, d->size);
		return -1;
	}
	vkGetImageMemoryRequirements(g->device, img->image, &req);
	memset(&fd_props, 0, sizeof(fd_props));
	fd_props.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
	if (gpu_vk_ok(g->get_memory_fd_props(g->device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
		d->prime_fd[index], &fd_props), "vkGetMemoryFdPropertiesKHR") != 0)
		return -1;
	combined_bits = req.memoryTypeBits & fd_props.memoryTypeBits;
	if (req.size > d->size || gpu_find_memory_type_any(g->phys, combined_bits, &memory_type) != 0) {
		log_msg("gpu imported no compatible memory type req=0x%x fd=0x%x combined=0x%x",
			req.memoryTypeBits, fd_props.memoryTypeBits, combined_bits);
		return -1;
	}
	import_fd = dup(d->prime_fd[index]);
	if (import_fd < 0) {
		log_msg("dup prime fd failed: %s", strerror(errno));
		return -1;
	}
	memset(&dedicated, 0, sizeof(dedicated));
	dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	dedicated.image = img->image;
	memset(&import_info, 0, sizeof(import_info));
	import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
	import_info.pNext = &dedicated;
	import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
	import_info.fd = import_fd;
	memset(&alloc, 0, sizeof(alloc));
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.pNext = &import_info;
	alloc.allocationSize = req.size;
	alloc.memoryTypeIndex = memory_type;
	if (gpu_vk_ok(vkAllocateMemory(g->device, &alloc, NULL, &img->memory),
		"vkAllocateMemory(import)") != 0) {
		close(import_fd);
		return -1;
	}
	import_fd = -1;
	if (gpu_vk_ok(vkBindImageMemory(g->device, img->image, img->memory, 0),
		"vkBindImageMemory(import)") != 0)
		return -1;
	memset(&view_info, 0, sizeof(view_info));
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = img->image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = VK_FORMAT_B8G8R8A8_UNORM;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	if (gpu_vk_ok(vkCreateImageView(g->device, &view_info, NULL, &img->view),
		"vkCreateImageView(import)") != 0)
		return -1;
	memset(&fb_info, 0, sizeof(fb_info));
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.renderPass = g->render_pass;
	fb_info.attachmentCount = 1;
	fb_info.pAttachments = &img->view;
	fb_info.width = (uint32_t)d->width;
	fb_info.height = (uint32_t)d->height;
	fb_info.layers = 1;
	if (gpu_vk_ok(vkCreateFramebuffer(g->device, &fb_info, NULL, &img->framebuffer),
		"vkCreateFramebuffer") != 0)
		return -1;
	log_msg("gpu imported fb[%d]=%u memory_type=%u", index, d->fb_id[index], memory_type);
	return 0;
}

static int gpu_init_panel(struct gpu_state *g, struct drm_state *d, size_t max_rects,
	size_t max_glyphs)
{
	VkDeviceSize vertex_size = (VkDeviceSize)max_rects * 6u * sizeof(struct gpu_vertex);
	VkDeviceSize text_vertex_size = (VkDeviceSize)max_glyphs * 6u * sizeof(struct gpu_vertex);

	if (gpu_init_vulkan(g) != 0)
		return -1;
	if (gpu_create_pipeline(g, (uint32_t)d->width, (uint32_t)d->height) != 0)
		return -1;
	if (gpu_init_fonts(g) != 0)
		log_msg("gpu font atlas unavailable; text will use pixel fallback");
	if (gpu_create_buffer(g, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &g->vertex_buffer, &g->vertex_memory) != 0)
		return -1;
	g->vertex_capacity = vertex_size / sizeof(struct gpu_vertex);
	if (gpu_create_buffer(g, text_vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &g->text_vertex_buffer,
		&g->text_vertex_memory) != 0)
		return -1;
	g->text_vertex_capacity = text_vertex_size / sizeof(struct gpu_vertex);
	if (gpu_import_drm_buffer(g, d, 0) != 0 || gpu_import_drm_buffer(g, d, 1) != 0)
		return -1;
	log_msg("gpu panel ready vertex_capacity=%llu text_vertex_capacity=%llu font_atlas=%s",
		(unsigned long long)g->vertex_capacity,
		(unsigned long long)g->text_vertex_capacity,
		g->font_ready ? "ready" : "fallback");
	return 0;
}

static int gpu_upload_vertices(struct gpu_state *g, struct canvas *c, uint32_t *vertex_count)
{
	struct gpu_vertex *vertices = NULL;
	size_t i;
	uint32_t out = 0;
	VkDeviceSize size;

	if (c->rect_count * 6u > g->vertex_capacity) {
		log_msg("gpu vertex overflow rects=%zu capacity=%llu", c->rect_count,
			(unsigned long long)g->vertex_capacity);
		return -1;
	}
	size = (VkDeviceSize)c->rect_count * 6u * sizeof(struct gpu_vertex);
	if (size == 0) {
		*vertex_count = 0;
		return 0;
	}
	if (gpu_vk_ok(vkMapMemory(g->device, g->vertex_memory, 0, size, 0, (void **)&vertices),
		"vkMapMemory(gpu vertices)") != 0)
		return -1;
	for (i = 0; i < c->rect_count; i++) {
		const struct gpu_rect *r = &c->rects[i];
		int x0 = r->x;
		int y0 = r->y;
		int x1 = r->x + r->w;
		int y1 = r->y + r->h;
		gpu_set_vertex(&vertices[out++], x0, y0, c->width, c->height, r->color);
		gpu_set_vertex(&vertices[out++], x1, y0, c->width, c->height, r->color);
		gpu_set_vertex(&vertices[out++], x0, y1, c->width, c->height, r->color);
		gpu_set_vertex(&vertices[out++], x1, y0, c->width, c->height, r->color);
		gpu_set_vertex(&vertices[out++], x1, y1, c->width, c->height, r->color);
		gpu_set_vertex(&vertices[out++], x0, y1, c->width, c->height, r->color);
	}
	vkUnmapMemory(g->device, g->vertex_memory);
	*vertex_count = out;
	return 0;
}

static int gpu_upload_text_vertices(struct gpu_state *g, struct canvas *c, uint32_t *vertex_count)
{
	struct gpu_vertex *vertices = NULL;
	size_t i;
	uint32_t out = 0;
	VkDeviceSize size;

	if (c->glyph_count * 6u > g->text_vertex_capacity) {
		log_msg("gpu text vertex overflow glyphs=%zu capacity=%llu", c->glyph_count,
			(unsigned long long)g->text_vertex_capacity);
		return -1;
	}
	size = (VkDeviceSize)c->glyph_count * 6u * sizeof(struct gpu_vertex);
	if (size == 0) {
		*vertex_count = 0;
		return 0;
	}
	if (gpu_vk_ok(vkMapMemory(g->device, g->text_vertex_memory, 0, size, 0,
		(void **)&vertices), "vkMapMemory(gpu text vertices)") != 0)
		return -1;
	for (i = 0; i < c->glyph_count; i++) {
		const struct gpu_glyph *glyph = &c->glyphs[i];
		gpu_set_vertex_uv(&vertices[out++], glyph->x0, glyph->y0, c->width, c->height,
			glyph->u0, glyph->v0, glyph->color);
		gpu_set_vertex_uv(&vertices[out++], glyph->x1, glyph->y0, c->width, c->height,
			glyph->u1, glyph->v0, glyph->color);
		gpu_set_vertex_uv(&vertices[out++], glyph->x0, glyph->y1, c->width, c->height,
			glyph->u0, glyph->v1, glyph->color);
		gpu_set_vertex_uv(&vertices[out++], glyph->x1, glyph->y0, c->width, c->height,
			glyph->u1, glyph->v0, glyph->color);
		gpu_set_vertex_uv(&vertices[out++], glyph->x1, glyph->y1, c->width, c->height,
			glyph->u1, glyph->v1, glyph->color);
		gpu_set_vertex_uv(&vertices[out++], glyph->x0, glyph->y1, c->width, c->height,
			glyph->u0, glyph->v1, glyph->color);
	}
	vkUnmapMemory(g->device, g->text_vertex_memory);
	*vertex_count = out;
	return 0;
}

static int gpu_render_canvas(struct gpu_state *g, int image_index, struct canvas *c)
{
	struct gpu_imported_image *img = &g->image[image_index];
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo cmd_alloc;
	VkCommandBufferBeginInfo begin_info;
	VkImageSubresourceRange range;
	VkImageMemoryBarrier to_color, to_host;
	VkClearValue clear;
	VkRenderPassBeginInfo render_begin;
	VkDeviceSize vertex_offset = 0;
	VkSubmitInfo submit;
	uint32_t vertex_count = 0;
	uint32_t text_vertex_count = 0;
	size_t op_i;

	if (c->gpu_overflow) {
		log_msg("gpu canvas overflow rects=%zu glyphs=%zu ops=%zu", c->rect_count,
			c->glyph_count, c->op_count);
		return -1;
	}
	if (gpu_upload_vertices(g, c, &vertex_count) != 0)
		return -1;
	if (gpu_upload_text_vertices(g, c, &text_vertex_count) != 0)
		return -1;
	memset(&cmd_alloc, 0, sizeof(cmd_alloc));
	cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_alloc.commandPool = g->command_pool;
	cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_alloc.commandBufferCount = 1;
	if (gpu_vk_ok(vkAllocateCommandBuffers(g->device, &cmd_alloc, &command_buffer),
		"vkAllocateCommandBuffers(gpu render)") != 0)
		return -1;
	memset(&begin_info, 0, sizeof(begin_info));
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (gpu_vk_ok(vkBeginCommandBuffer(command_buffer, &begin_info),
		"vkBeginCommandBuffer(gpu render)") != 0)
		return -1;
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = 1;
	range.layerCount = 1;
	memset(&to_color, 0, sizeof(to_color));
	to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	to_color.srcAccessMask = img->initialized ? VK_ACCESS_HOST_READ_BIT : 0;
	to_color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	to_color.oldLayout = img->layout;
	to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_color.image = img->image;
	to_color.subresourceRange = range;
	vkCmdPipelineBarrier(command_buffer,
		img->initialized ? VK_PIPELINE_STAGE_HOST_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &to_color);
	memset(&clear, 0, sizeof(clear));
	clear.color.float32[3] = 1.0f;
	memset(&render_begin, 0, sizeof(render_begin));
	render_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_begin.renderPass = g->render_pass;
	render_begin.framebuffer = img->framebuffer;
	render_begin.renderArea.extent.width = (uint32_t)c->width;
	render_begin.renderArea.extent.height = (uint32_t)c->height;
	render_begin.clearValueCount = 1;
	render_begin.pClearValues = &clear;
	vkCmdBeginRenderPass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
	for (op_i = 0; op_i < c->op_count; ) {
		struct gpu_draw_op *op = &c->ops[op_i];
		if (op->type == GPU_OP_RECT) {
			uint32_t first = op->first;
			uint32_t count = 0;
			while (op_i < c->op_count && c->ops[op_i].type == GPU_OP_RECT) {
				count += c->ops[op_i].count;
				op_i++;
			}
			if (count > 0 && vertex_count > 0) {
				vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					g->pipeline);
				vkCmdBindVertexBuffers(command_buffer, 0, 1, &g->vertex_buffer,
					&vertex_offset);
				vkCmdDraw(command_buffer, count * 6u, 1, first * 6u, 0);
			}
			continue;
		}
		if (op->type == GPU_OP_TEXT) {
			uint8_t scale = op->scale;
			uint32_t first = op->first;
			uint32_t count = 0;
			while (op_i < c->op_count && c->ops[op_i].type == GPU_OP_TEXT &&
				c->ops[op_i].scale == scale) {
				count += c->ops[op_i].count;
				op_i++;
			}
			if (scale <= GPU_FONT_MAX_SCALE && g->font[scale].ready && count > 0 &&
				text_vertex_count > 0) {
				vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					g->text_pipeline);
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					g->text_pipeline_layout, 0, 1, &g->font[scale].descriptor_set,
					0, NULL);
				vkCmdBindVertexBuffers(command_buffer, 0, 1, &g->text_vertex_buffer,
					&vertex_offset);
				vkCmdDraw(command_buffer, count * 6u, 1, first * 6u, 0);
			}
			continue;
		}
		op_i++;
	}
	vkCmdEndRenderPass(command_buffer);
	memset(&to_host, 0, sizeof(to_host));
	to_host.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	to_host.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	to_host.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	to_host.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	to_host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_host.image = img->image;
	to_host.subresourceRange = range;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &to_host);
	if (gpu_vk_ok(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(gpu render)") != 0)
		return -1;
	memset(&submit, 0, sizeof(submit));
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &command_buffer;
	if (gpu_vk_ok(vkQueueSubmit(g->queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit(gpu render)") != 0)
		return -1;
	if (gpu_vk_ok(vkQueueWaitIdle(g->queue), "vkQueueWaitIdle(gpu render)") != 0)
		return -1;
	vkFreeCommandBuffers(g->device, g->command_pool, 1, &command_buffer);
	img->layout = VK_IMAGE_LAYOUT_GENERAL;
	img->initialized = true;
	return 0;
}
#endif

static void boot_mode_line(char *out, size_t len)
{
	char buf[256];
	if (read_file("/etc/alioth-init-mode", buf, sizeof(buf))) {
		snprintf(out, len, "%s", buf);
		return;
	}
	if (read_file("/proc/1/cmdline", buf, sizeof(buf)) && strstr(buf, "alioth-switch-init")) {
		snprintf(out, len, "switchroot");
		return;
	}
	snprintf(out, len, "mininitramfs");
}

static void uptime_line(char *out, size_t len)
{
	char buf[128];
	double secf = 0;
	int sec;
	if (!read_file("/proc/uptime", buf, sizeof(buf)) || sscanf(buf, "%lf", &secf) != 1) {
		snprintf(out, len, "unknown");
		return;
	}
	sec = (int)secf;
	snprintf(out, len, "%02d:%02d:%02d", sec / 3600, (sec / 60) % 60, sec % 60);
}

static void mount_line(const char *target, char *out, size_t len)
{
	FILE *f = fopen("/proc/mounts", "r");
	char src[160], tgt[160], type[64], rest[512];

	snprintf(out, len, "not mounted");
	if (!f)
		return;
	while (fscanf(f, "%159s %159s %63s %511s %*d %*d\n", src, tgt, type, rest) == 4) {
		if (strcmp(tgt, target) == 0) {
			size_t sl = strlen(src);
			if (sl > 25)
				snprintf(out, len, "...%s %s mounted", src + sl - 22, type);
			else
				snprintf(out, len, "%s %s mounted", src, type);
			break;
		}
	}
	fclose(f);
}

static void rootfs_line(char *out, size_t len)
{
	char mnt[256];
	mount_line("/mnt/arch", mnt, sizeof(mnt));
	if (strcmp(mnt, "not mounted") != 0) {
		snprintf(out, len, "%s", mnt);
		return;
	}
	mount_line("/", out, len);
}

static void unquote_value(char *s)
{
	size_t n = strlen(s);

	if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') || (s[0] == '\'' && s[n - 1] == '\''))) {
		memmove(s, s + 1, n - 2);
		s[n - 2] = '\0';
	}
}

static void drm_line(char *out, size_t len)
{
	char status[64] = "", enabled[64] = "", dpms[64] = "";
	read_file("/sys/class/drm/card0-DSI-1/status", status, sizeof(status));
	read_file("/sys/class/drm/card0-DSI-1/enabled", enabled, sizeof(enabled));
	read_file("/sys/class/drm/card0-DSI-1/dpms", dpms, sizeof(dpms));
	snprintf(out, len, "%s %s %s",
		status[0] ? status : "unknown",
		enabled[0] ? enabled : "",
		dpms[0] ? dpms : "");
}

static bool iface_ipv4(const char *ifname, char *out, size_t len)
{
	struct ifreq ifr;
	const struct sockaddr_in *sin;
	int fd;
	bool ok = false;

	if (len == 0)
		return false;
	out[0] = '\0';
	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return false;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
		sin = (const struct sockaddr_in *)(const void *)&ifr.ifr_addr;
		ok = inet_ntop(AF_INET, &sin->sin_addr, out, len) != NULL;
	}
	close(fd);
	return ok;
}

static bool read_kv(const char *path, const char *key, char *out, size_t len)
{
	FILE *f;
	char line[256];
	size_t key_len = strlen(key);

	if (len == 0)
		return false;
	out[0] = '\0';
	f = fopen(path, "r");
	if (!f)
		return false;
	while (fgets(line, sizeof(line), f)) {
		char *value;
		if (strncmp(line, key, key_len) != 0 || line[key_len] != '=')
			continue;
		value = trim(line + key_len + 1);
		short_copy(out, len, value, len - 1);
		fclose(f);
		return out[0] != '\0';
	}
	fclose(f);
	return false;
}

static void os_line(char *out, size_t len)
{
	char pretty[128] = "", pid1[64] = "";

	if (read_kv("/etc/os-release", "PRETTY_NAME", pretty, sizeof(pretty))) {
		unquote_value(pretty);
	} else if (!read_file("/etc/lele-rootfs-id", pretty, sizeof(pretty))) {
		snprintf(pretty, sizeof(pretty), "unknown userspace");
	}
	read_file("/proc/1/comm", pid1, sizeof(pid1));
	if (strcmp(pid1, "systemd") == 0)
		snprintf(out, len, "%s systemd", pretty);
	else if (pid1[0])
		snprintf(out, len, "%s pid1 %s", pretty, pid1);
	else
		snprintf(out, len, "%s", pretty);
}

static void distro_title_line(char *out, size_t len)
{
	char pretty[128] = "";

	if (read_kv("/etc/os-release", "PRETTY_NAME", pretty, sizeof(pretty))) {
		unquote_value(pretty);
		if (strstr(pretty, "Ubuntu 24.04")) {
			snprintf(out, len, "UBUNTU 24.04");
			return;
		}
		if (strstr(pretty, "Ubuntu")) {
			snprintf(out, len, "UBUNTU LINUX");
			return;
		}
		if (strstr(pretty, "CentOS")) {
			snprintf(out, len, "CENTOS LINUX");
			return;
		}
		short_copy(out, len, pretty, 18);
		return;
	}
	snprintf(out, len, "LELE LINUX");
}

static void iface_line(const char *ifname, int port, char *out, size_t len)
{
	char path[128], state[64], ip[INET_ADDRSTRLEN];

	snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ifname);
	if (!read_file(path, state, sizeof(state))) {
		snprintf(out, len, "%s missing", ifname);
		return;
	}
	if (!iface_ipv4(ifname, ip, sizeof(ip)))
		snprintf(ip, sizeof(ip), "noip");
	if (port > 0)
		snprintf(out, len, "%s %s %s:%d", ifname, state, ip, port);
	else
		snprintf(out, len, "%s %s %s", ifname, state, ip);
}

static void ssid_line(char *out, size_t len)
{
	char ssid[96] = "", state[64] = "", mode[64] = "";

	if (read_kv("/run/lele-ap/status", "mode", state, sizeof(state)) && strcmp(state, "ap") == 0) {
		if (!read_kv("/run/lele-ap/status", "ssid", ssid, sizeof(ssid)))
			snprintf(ssid, sizeof(ssid), "unknown");
		snprintf(out, len, "%s AP", ssid);
		return;
	}

	read_kv("/run/alioth-wifi-state", "mode", mode, sizeof(mode));
	read_kv("/run/alioth-wifi-state", "ssid", ssid, sizeof(ssid));
	read_kv("/run/alioth-wifi-state", "wpa=wpa_state", state, sizeof(state));
	if (ssid[0] && (strcmp(mode, "client-connected") == 0 || strcmp(state, "COMPLETED") == 0)) {
		snprintf(out, len, "%s %s", ssid, state[0] ? state : "CONNECTED");
		return;
	}

	ssid[0] = '\0';
	state[0] = '\0';
	read_kv("/run/alioth-net-status", "ssid", ssid, sizeof(ssid));
	read_kv("/run/alioth-net-status", "wpa_state", state, sizeof(state));
	if (!ssid[0])
		read_kv("/run/alioth-wpa-status.log", "ssid", ssid, sizeof(ssid));
	if (!state[0])
		read_kv("/run/alioth-wpa-status.log", "wpa_state", state, sizeof(state));
	if (ssid[0])
		snprintf(out, len, "%s %s", ssid, state[0] ? state : "UNKNOWN");
	else
		snprintf(out, len, "not connected");
}

static void wifi_mode_line(char *out, size_t len)
{
	char mode[64] = "", ssid[96] = "", url[96] = "";
	char ip[INET_ADDRSTRLEN] = "";

	if (read_kv("/run/lele-ap/status", "mode", mode, sizeof(mode)) && strcmp(mode, "ap") == 0) {
		read_kv("/run/lele-ap/status", "ssid", ssid, sizeof(ssid));
		read_kv("/run/lele-ap/status", "url", url, sizeof(url));
		if (!iface_ipv4("wlan0", ip, sizeof(ip)))
			snprintf(ip, sizeof(ip), "noip");
		snprintf(out, len, "AP %s %s", ssid[0] ? ssid : "unknown", url[0] ? url : ip);
		return;
	}
	iface_ipv4("wlan0", ip, sizeof(ip));
	snprintf(out, len, "CLIENT %s", ip[0] ? ip : "noip");
}

static void ap_client_line(char *out, size_t len)
{
	FILE *f = fopen("/run/lele-ap/dnsmasq.log", "r");
	char line[256], last[128] = "";

	if (!f) {
		snprintf(out, len, "inactive");
		return;
	}
	while (fgets(line, sizeof(line), f)) {
		char *p = trim(line);
		if (strstr(p, "DHCPACK") || strstr(p, "DHCPDISCOVER") || strstr(p, "DHCPOFFER"))
			short_copy(last, sizeof(last), p, 70);
	}
	fclose(f);
	snprintf(out, len, "%s", last[0] ? last : "waiting clients");
}

static void default_route_line(char *out, size_t len)
{
	FILE *f = fopen("/proc/net/route", "r");
	char line[256], iface[64], gwbuf[INET_ADDRSTRLEN];
	unsigned long dest, gateway;

	snprintf(out, len, "no default route");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		if (sscanf(line, "%63s %lx %lx", iface, &dest, &gateway) != 3)
			continue;
		if (dest == 0) {
			if (gateway == 0) {
				snprintf(out, len, "%s direct", iface);
			} else {
				struct in_addr addr;
				addr.s_addr = (in_addr_t)gateway;
				if (!inet_ntop(AF_INET, &addr, gwbuf, sizeof(gwbuf)))
					snprintf(gwbuf, sizeof(gwbuf), "unknown");
				snprintf(out, len, "%s via %s", iface, gwbuf);
			}
			break;
		}
	}
	fclose(f);
}

static void dns_line(char *out, size_t len)
{
	FILE *f = fopen("/etc/resolv.conf", "r");
	char line[256], dns[96] = "";
	int count = 0;

	if (!f) {
		snprintf(out, len, "none");
		return;
	}
	while (fgets(line, sizeof(line), f) && count < 2) {
		char *p = trim(line);
		char *value;
		if (strncmp(p, "nameserver", 10) != 0 || !isspace((unsigned char)p[10]))
			continue;
		value = trim(p + 10);
		if (!value[0])
			continue;
		if (dns[0])
			strncat(dns, " ", sizeof(dns) - strlen(dns) - 1);
		strncat(dns, value, sizeof(dns) - strlen(dns) - 1);
		count++;
	}
	fclose(f);
	snprintf(out, len, "%s", dns[0] ? dns : "none");
}

static bool read_ps(const char *name, const char *file, char *out, size_t len)
{
	char path[256];
	snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s", name, file);
	return read_file(path, out, len);
}

static bool read_ps_long(const char *name, const char *file, long *out)
{
	char raw[64] = "";
	char *end;
	long value;

	if (!read_ps(name, file, raw, sizeof(raw)))
		return false;
	errno = 0;
	value = strtol(raw, &end, 10);
	if (errno != 0 || end == raw)
		return false;
	*out = value;
	return true;
}

static bool read_ull_file(const char *path, unsigned long long *out)
{
	char raw[64] = "";
	char *end;
	unsigned long long value;

	if (!read_file(path, raw, sizeof(raw)))
		return false;
	errno = 0;
	value = strtoull(raw, &end, 10);
	if (errno != 0 || end == raw)
		return false;
	*out = value;
	return true;
}

static bool read_long_file(const char *path, long *out)
{
	char raw[64] = "";
	char *end;
	long value;

	if (!read_file(path, raw, sizeof(raw)))
		return false;
	errno = 0;
	value = strtol(raw, &end, 10);
	if (errno != 0 || end == raw)
		return false;
	*out = value;
	return true;
}

static bool write_long_file(const char *path, long value)
{
	char raw[64];
	int fd;
	int len;
	ssize_t n;

	len = snprintf(raw, sizeof(raw), "%ld\n", value);
	if (len <= 0 || len >= (int)sizeof(raw))
		return false;
	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	n = write(fd, raw, (size_t)len);
	close(fd);
	return n == len;
}

static bool write_string_file(const char *path, const char *value)
{
	int fd;
	bool ok;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	ok = write_all_fd(fd, value, strlen(value));
	if (ok)
		ok = write_all_fd(fd, "\n", 1);
	close(fd);
	return ok;
}

static bool numeric_name(const char *s)
{
	if (!s || !s[0])
		return false;
	while (*s) {
		if (!isdigit((unsigned char)*s))
			return false;
		s++;
	}
	return true;
}

static bool cpu_dir_name(const char *name, int *core)
{
	char *end;
	long value;

	if (strncmp(name, "cpu", 3) != 0 || !isdigit((unsigned char)name[3]))
		return false;
	errno = 0;
	value = strtol(name + 3, &end, 10);
	if (errno != 0 || end == name + 3 || *end != '\0' || value < 0 || value > 255)
		return false;
	*core = (int)value;
	return true;
}

static bool word_list_contains(const char *list, const char *word)
{
	size_t word_len = strlen(word);
	const char *p = list;

	while (p && *p) {
		while (*p && isspace((unsigned char)*p))
			p++;
		if (strncmp(p, word, word_len) == 0 &&
			(p[word_len] == '\0' || isspace((unsigned char)p[word_len])))
			return true;
		p = strpbrk(p, " \t\r\n");
	}
	return false;
}

static bool read_cpu_governor(int core, char *out, size_t len)
{
	char path[128];

	snprintf(path, sizeof(path),
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", core);
	return read_file(path, out, len);
}

static void cpu_governor_line(char *out, size_t len)
{
	DIR *dir;
	struct dirent *de;
	char first[32] = "";
	int first_core = -1;
	bool mixed = false;
	int count = 0;

	snprintf(out, len, "governor unavailable");
	dir = opendir("/sys/devices/system/cpu");
	if (!dir)
		return;
	while ((de = readdir(dir))) {
		char gov[32] = "";
		int core;

		if (!cpu_dir_name(de->d_name, &core))
			continue;
		if (!read_cpu_governor(core, gov, sizeof(gov)))
			continue;
		if (!first[0]) {
			short_copy(first, sizeof(first), gov, sizeof(first) - 1);
			first_core = core;
		} else if (strcmp(first, gov) != 0) {
			mixed = true;
		}
		count++;
	}
	closedir(dir);
	if (count <= 0)
		return;
	if (mixed)
		snprintf(out, len, "mixed cpu%d=%s", first_core, first);
	else
		snprintf(out, len, "%s", first);
}

static bool cpu_governor_matches(const char *wanted)
{
	char gov[96];

	cpu_governor_line(gov, sizeof(gov));
	return strcmp(gov, wanted) == 0;
}

static bool cpu_governor_available(const char *governor)
{
	char available[160] = "";

	if (!read_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors",
		available, sizeof(available)))
		return false;
	return word_list_contains(available, governor);
}

static bool set_cpu_governor(const char *governor)
{
	DIR *dir;
	struct dirent *de;
	int changed = 0;
	int failed = 0;

	if (!cpu_governor_available(governor)) {
		snprintf(g_cpu_governor_message, sizeof(g_cpu_governor_message),
			"%s unavailable", governor);
		log_msg("cpu governor unavailable: %s", governor);
		return false;
	}
	dir = opendir("/sys/devices/system/cpu");
	if (!dir) {
		snprintf(g_cpu_governor_message, sizeof(g_cpu_governor_message),
			"cpu sysfs unavailable");
		return false;
	}
	while ((de = readdir(dir))) {
		char path[160];
		int core;

		if (!cpu_dir_name(de->d_name, &core))
			continue;
		snprintf(path, sizeof(path),
			"/sys/devices/system/cpu/%s/cpufreq/scaling_governor", de->d_name);
		if (access(path, W_OK) != 0)
			continue;
		if (write_string_file(path, governor))
			changed++;
		else
			failed++;
	}
	closedir(dir);
	if (changed > 0 && failed == 0) {
		snprintf(g_cpu_governor_message, sizeof(g_cpu_governor_message),
			"%s applied to %d cores", governor, changed);
		log_msg("cpu governor set=%s cores=%d", governor, changed);
		return true;
	}
	snprintf(g_cpu_governor_message, sizeof(g_cpu_governor_message),
		"%s partial cores=%d failed=%d", governor, changed, failed);
	log_msg("cpu governor set partial=%s cores=%d failed=%d", governor, changed, failed);
	return changed > 0;
}

static bool cpu_freq_mhz(int core, long *cur_mhz, long *max_mhz)
{
	char path[128];
	long cur = 0, max = 0;

	snprintf(path, sizeof(path),
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", core);
	if (!read_long_file(path, &cur))
		return false;
	snprintf(path, sizeof(path),
		"/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", core);
	if (!read_long_file(path, &max))
		return false;
	*cur_mhz = (cur + 500) / 1000;
	*max_mhz = (max + 500) / 1000;
	return true;
}

static void cpu_frequency_line(char *out, size_t len)
{
	long l_cur = 0, l_max = 0, b_cur = 0, b_max = 0, p_cur = 0, p_max = 0;
	bool l_ok = cpu_freq_mhz(0, &l_cur, &l_max);
	bool b_ok = cpu_freq_mhz(4, &b_cur, &b_max);
	bool p_ok = cpu_freq_mhz(7, &p_cur, &p_max);

	if (l_ok && b_ok && p_ok) {
		snprintf(out, len, "L%.1f B%.1f P%.1fGHz",
			(double)l_cur / 1000.0, (double)b_cur / 1000.0, (double)p_cur / 1000.0);
		return;
	}
	snprintf(out, len, "freq unavailable");
}

#define CPU_THREAD_TRACK_MAX 4096

struct cpu_thread_track {
	int pid;
	int tid;
	uint64_t ticks;
	bool seen;
};

static struct cpu_thread_track g_cpu_thread_track[CPU_THREAD_TRACK_MAX];
static int g_cpu_thread_track_count;
static int64_t g_cpu_thread_track_ms;
static int64_t g_cpu_hot_update_ms;
static char g_cpu_hot_value[96] = "sampling";
static char g_cpu_hot_detail[160] = "waiting for CPU delta";

static struct cpu_thread_track *cpu_thread_track_find(int pid, int tid)
{
	int i;

	for (i = 0; i < g_cpu_thread_track_count; i++) {
		if (g_cpu_thread_track[i].pid == pid && g_cpu_thread_track[i].tid == tid)
			return &g_cpu_thread_track[i];
	}
	if (g_cpu_thread_track_count >= CPU_THREAD_TRACK_MAX)
		return NULL;
	g_cpu_thread_track[g_cpu_thread_track_count].pid = pid;
	g_cpu_thread_track[g_cpu_thread_track_count].tid = tid;
	g_cpu_thread_track[g_cpu_thread_track_count].ticks = 0;
	g_cpu_thread_track[g_cpu_thread_track_count].seen = false;
	return &g_cpu_thread_track[g_cpu_thread_track_count++];
}

static void cpu_thread_track_compact(void)
{
	int i, out = 0;

	for (i = 0; i < g_cpu_thread_track_count; i++) {
		if (!g_cpu_thread_track[i].seen)
			continue;
		if (out != i)
			g_cpu_thread_track[out] = g_cpu_thread_track[i];
		out++;
	}
	g_cpu_thread_track_count = out;
}

static bool read_thread_stat(int pid, int tid, uint64_t *ticks, int *processor,
	char *comm, size_t comm_len)
{
	char path[128];
	char buf[768];
	char *left, *right, *rest, *saveptr = NULL, *token;
	unsigned long long utime = 0, stime = 0;
	bool have_utime = false, have_stime = false;
	int field = 3;

	snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", pid, tid);
	if (!read_file(path, buf, sizeof(buf)))
		return false;
	left = strchr(buf, '(');
	right = strrchr(buf, ')');
	if (!left || !right || right <= left)
		return false;
	if (comm_len > 0) {
		size_t n = (size_t)(right - left - 1);
		if (n >= comm_len)
			n = comm_len - 1;
		memcpy(comm, left + 1, n);
		comm[n] = '\0';
	}
	rest = right + 1;
	while (*rest && isspace((unsigned char)*rest))
		rest++;
	token = strtok_r(rest, " \t\r\n", &saveptr);
	while (token) {
		if (field == 14) {
			utime = strtoull(token, NULL, 10);
			have_utime = true;
		} else if (field == 15) {
			stime = strtoull(token, NULL, 10);
			have_stime = true;
		} else if (field == 39) {
			*processor = atoi(token);
			break;
		}
		field++;
		token = strtok_r(NULL, " \t\r\n", &saveptr);
	}
	if (!have_utime || !have_stime)
		return false;
	*ticks = utime + stime;
	return true;
}

static bool read_proc_cmdline(int pid, char *out, size_t len)
{
	char path[128];
	int fd;
	ssize_t n;
	size_t i;

	if (len == 0)
		return false;
	out[0] = '\0';
	snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	n = read(fd, out, len - 1);
	close(fd);
	if (n <= 0)
		return false;
	for (i = 0; i < (size_t)n; i++) {
		if (out[i] == '\0')
			out[i] = ' ';
	}
	while (n > 0 && isspace((unsigned char)out[n - 1]))
		n--;
	out[n] = '\0';
	return out[0] != '\0';
}

static void update_cpu_hot_thread_cache(void)
{
	DIR *proc;
	struct dirent *pde;
	int64_t now = monotonic_ms();
	int64_t elapsed_ms;
	long clk_tck;
	double top_pct = 0.0;
	int top_pid = -1, top_tid = -1, top_cpu = -1;
	char top_comm[64] = "";
	char top_cmd[160] = "";
	int i;

	if (g_cpu_hot_update_ms != 0 && now - g_cpu_hot_update_ms < 1500)
		return;
	for (i = 0; i < g_cpu_thread_track_count; i++)
		g_cpu_thread_track[i].seen = false;
	elapsed_ms = g_cpu_thread_track_ms ? now - g_cpu_thread_track_ms : 0;
	clk_tck = sysconf(_SC_CLK_TCK);
	if (clk_tck <= 0)
		clk_tck = 100;
	proc = opendir("/proc");
	if (!proc) {
		snprintf(g_cpu_hot_value, sizeof(g_cpu_hot_value), "proc unavailable");
		snprintf(g_cpu_hot_detail, sizeof(g_cpu_hot_detail), "cannot scan /proc");
		return;
	}
	while ((pde = readdir(proc))) {
		DIR *tasks;
		struct dirent *tde;
		char task_path[128];
		int pid;

		if (!numeric_name(pde->d_name))
			continue;
		pid = atoi(pde->d_name);
		snprintf(task_path, sizeof(task_path), "/proc/%d/task", pid);
		tasks = opendir(task_path);
		if (!tasks)
			continue;
		while ((tde = readdir(tasks))) {
			struct cpu_thread_track *track;
			uint64_t ticks = 0;
			int tid, processor = -1;
			char comm[64] = "";

			if (!numeric_name(tde->d_name))
				continue;
			tid = atoi(tde->d_name);
			if (!read_thread_stat(pid, tid, &ticks, &processor, comm, sizeof(comm)))
				continue;
			track = cpu_thread_track_find(pid, tid);
			if (!track)
				continue;
			if (elapsed_ms > 0 && ticks >= track->ticks) {
				double pct = 100.0 * (double)(ticks - track->ticks) * 1000.0 /
					((double)clk_tck * (double)elapsed_ms);
				if (pct > top_pct) {
					top_pct = pct;
					top_pid = pid;
					top_tid = tid;
					top_cpu = processor;
					short_copy(top_comm, sizeof(top_comm), comm, sizeof(top_comm) - 1);
				}
			}
			track->ticks = ticks;
			track->seen = true;
		}
		closedir(tasks);
	}
	closedir(proc);
	cpu_thread_track_compact();
	g_cpu_thread_track_ms = now;
	g_cpu_hot_update_ms = now;
	if (elapsed_ms <= 0) {
		snprintf(g_cpu_hot_value, sizeof(g_cpu_hot_value), "sampling");
		snprintf(g_cpu_hot_detail, sizeof(g_cpu_hot_detail), "waiting for CPU delta");
		return;
	}
	if (top_pid < 0 || top_pct < 1.0) {
		snprintf(g_cpu_hot_value, sizeof(g_cpu_hot_value), "idle");
		snprintf(g_cpu_hot_detail, sizeof(g_cpu_hot_detail), "no userspace thread above 1%%");
		return;
	}
	if (!read_proc_cmdline(top_pid, top_cmd, sizeof(top_cmd)))
		snprintf(top_cmd, sizeof(top_cmd), "[%s]", top_comm[0] ? top_comm : "thread");
	snprintf(g_cpu_hot_value, sizeof(g_cpu_hot_value), "C%d %.0f%% %s pid %d",
		top_cpu, top_pct, top_comm[0] ? top_comm : "thread", top_pid);
	if (top_tid != top_pid)
		snprintf(g_cpu_hot_detail, sizeof(g_cpu_hot_detail), "tid %d  %s", top_tid, top_cmd);
	else
		snprintf(g_cpu_hot_detail, sizeof(g_cpu_hot_detail), "%s", top_cmd);
}

static void cpu_hot_thread_line(char *value, size_t value_len, char *detail, size_t detail_len)
{
	update_cpu_hot_thread_cache();
	snprintf(value, value_len, "%s", g_cpu_hot_value);
	snprintf(detail, detail_len, "%s", g_cpu_hot_detail);
}

static bool ps_truthy(const char *raw)
{
	return strcmp(raw, "1") == 0 || strcasecmp(raw, "yes") == 0 || strcasecmp(raw, "true") == 0 ||
		strcasecmp(raw, "online") == 0;
}

static bool supply_online(const char *name)
{
	char raw[64] = "";

	if (!read_ps(name, "online", raw, sizeof(raw)))
		read_ps(name, "present", raw, sizeof(raw));
	return ps_truthy(raw);
}

static int battery_capacity_value(void)
{
	char cap[32] = "";

	if (!read_ps("battery", "capacity", cap, sizeof(cap)))
		read_ps("bms", "capacity", cap, sizeof(cap));
	if (!cap[0])
		return -1;
	return atoi(cap);
}

static void battery_status_value(char *status, size_t len)
{
	if (!read_ps("battery", "status", status, len))
		read_ps("bms", "status", status, len);
}

static void battery_line(char *out, size_t len)
{
	char cap[32] = "", status[64] = "", volt[64] = "", cur[64] = "", temp[64] = "";
	char tmp[64];
	long v;

	if (!read_ps("battery", "capacity", cap, sizeof(cap)))
		read_ps("bms", "capacity", cap, sizeof(cap));
	if (!read_ps("battery", "status", status, sizeof(status)))
		read_ps("bms", "status", status, sizeof(status));
	if (read_ps("battery", "voltage_now", tmp, sizeof(tmp)) || read_ps("bms", "voltage_now", tmp, sizeof(tmp))) {
		v = atol(tmp);
		snprintf(volt, sizeof(volt), "%.2fV", (double)v / 1000000.0);
	}
	if (read_ps("battery", "current_now", tmp, sizeof(tmp)) || read_ps("bms", "current_now", tmp, sizeof(tmp))) {
		v = atol(tmp);
		snprintf(cur, sizeof(cur), "%+.0fmA", (double)v / 1000.0);
	}
	if (read_ps("battery", "temp", tmp, sizeof(tmp)) || read_ps("bms", "temp", tmp, sizeof(tmp))) {
		v = atol(tmp);
		if (v > 1000)
			snprintf(temp, sizeof(temp), "%.1fC", (double)v / 1000.0);
		else
			snprintf(temp, sizeof(temp), "%.1fC", (double)v / 10.0);
	}
	snprintf(out, len, "bat %s%% %s %s %s %s",
		cap[0] ? cap : "?",
		status[0] ? status : "?",
		volt, cur, temp);
}

static const char *charger_label(const char *real_type, long pd_active, long apdo_max,
	long quick_charge_type, long fastcharge_mode, long pump_present, long pump_current,
	bool dc_online)
{
	if (pump_present > 0 || pump_current > 0)
		return "PUMP";
	if (apdo_max > 0)
		return "PPS";
	if (pd_active > 0 || strcasestr(real_type, "PD"))
		return "PD";
	if (quick_charge_type > 0 || fastcharge_mode > 0 || strcasestr(real_type, "HVDCP") ||
		strcasestr(real_type, "QC"))
		return "QC";
	if (strcasestr(real_type, "DCP"))
		return "DCP";
	if (strcasestr(real_type, "CDP"))
		return "CDP";
	if (strcasestr(real_type, "USB"))
		return "USB";
	if (dc_online)
		return "DC";
	return real_type[0] ? real_type : "USB";
}

static bool usb_input_current(long *ua)
{
	return read_ps_long("usb", "input_current_now", ua) ||
		read_ps_long("usb", "current_now", ua) ||
		read_ps_long("usb", "input_current_settled", ua) ||
		read_ps_long("usb", "current_max", ua);
}

static bool usb_input_voltage(long *uv)
{
	return read_ps_long("usb", "voltage_now", uv) ||
		read_ps_long("main", "input_voltage_settled", uv);
}

static void charge_speed_line(char *out, size_t len)
{
	char real_type[64] = "";
	char charge_type[64] = "";
	const char *label;
	long uv = 0, ua = 0, pd_active = 0, apdo_max = 0, quick_charge_type = 0;
	long fastcharge_mode = 0, pump_present = 0, pump_current = 0;
	bool usb_online = supply_online("usb");
	bool dc_online = supply_online("dc");
	bool have_v, have_i;
	double volts, amps, watts;

	if (!usb_online && !dc_online) {
		snprintf(out, len, "unplugged usb=off dc=off");
		return;
	}

	read_ps("usb", "real_type", real_type, sizeof(real_type));
	read_ps("battery", "charge_type", charge_type, sizeof(charge_type));
	read_ps_long("usb", "pd_active", &pd_active);
	read_ps_long("usb", "apdo_max", &apdo_max);
	read_ps_long("usb", "quick_charge_type", &quick_charge_type);
	read_ps_long("usb", "fastcharge_mode", &fastcharge_mode);
	read_ps_long("bq2597x-standalone", "present", &pump_present);
	read_ps_long("bq2597x-standalone", "ti_bus_current", &pump_current);

	have_v = usb_input_voltage(&uv);
	have_i = usb_input_current(&ua);
	label = charger_label(real_type, pd_active, apdo_max, quick_charge_type,
		fastcharge_mode, pump_present, pump_current, dc_online);

	if (have_v && have_i) {
		volts = (double)uv / 1000000.0;
		amps = (double)labs(ua) / 1000000.0;
		watts = volts * amps;
		if (strcmp(label, "USB") == 0 && strcasecmp(charge_type, "Fast") == 0 && watts >= 7.5)
			label = "FAST?";
		snprintf(out, len, "%s %.1fW %.2fV %.2fA", label, watts, volts, amps);
	} else if (have_v) {
		snprintf(out, len, "%s %.2fV current?", label, (double)uv / 1000000.0);
	} else if (have_i) {
		snprintf(out, len, "%s %.2fA voltage?", label, (double)labs(ua) / 1000000.0);
	} else {
		snprintf(out, len, "%s telemetry pending", label);
	}
}

static bool backlight_values(long *brightness, long *actual, long *max_value, long *bl_power)
{
	bool have_brightness;
	bool have_max;

	have_brightness = read_long_file(BACKLIGHT_BRIGHTNESS_PATH, brightness);
	if (!read_long_file(BACKLIGHT_ACTUAL_PATH, actual))
		*actual = have_brightness ? *brightness : -1;
	have_max = read_long_file(BACKLIGHT_MAX_PATH, max_value);
	if (!read_long_file(BACKLIGHT_POWER_PATH, bl_power))
		*bl_power = -1;
	return have_brightness && have_max && *max_value > 0;
}

static int backlight_percent_value(void)
{
	long brightness = 0, actual = 0, max_value = 0, bl_power = 0;

	if (!backlight_values(&brightness, &actual, &max_value, &bl_power))
		return -1;
	return (int)((brightness * 100 + max_value / 2) / max_value);
}

static void backlight_line(char *value, size_t value_len, char *detail, size_t detail_len,
	double *pct)
{
	long brightness = 0, actual = 0, max_value = 0, bl_power = 0;
	int percent;

	*pct = -1.0;
	if (!backlight_values(&brightness, &actual, &max_value, &bl_power)) {
		snprintf(value, value_len, "Backlight ?");
		snprintf(detail, detail_len, "panel backlight unavailable");
		return;
	}
	percent = (int)((brightness * 100 + max_value / 2) / max_value);
	*pct = (double)brightness / (double)max_value;
	snprintf(value, value_len, "%d%%", percent);
	if (actual > 0 && actual != brightness)
		snprintf(detail, detail_len, "set %ld actual %ld / %ld", brightness, actual, max_value);
	else
		snprintf(detail, detail_len, "%ld / %ld  sysfs setpoint", brightness, max_value);
}

static bool set_backlight_percent(int percent)
{
	long brightness = 0, actual = 0, max_value = 0, bl_power = 0;
	long target;

	if (!backlight_values(&brightness, &actual, &max_value, &bl_power))
		return false;
	if (percent < 1)
		percent = 1;
	if (percent > 100)
		percent = 100;
	target = (max_value * percent + 50) / 100;
	if (target < 1)
		target = 1;
	if (target > max_value)
		target = max_value;
	if (!write_long_file(BACKLIGHT_BRIGHTNESS_PATH, target)) {
		log_msg("backlight set failed percent=%d target=%ld", percent, target);
		return false;
	}
	log_msg("backlight percent=%d target=%ld max=%ld", percent, target, max_value);
	return true;
}

static bool adjust_backlight_percent(int delta)
{
	int percent = backlight_percent_value();

	if (percent < 0)
		return false;
	return set_backlight_percent(percent + delta);
}

static bool battery_net_power(double *watts, double *volts, double *milliamps)
{
	long uv = 0, ua = 0;

	if (!(read_ps_long("battery", "voltage_now", &uv) || read_ps_long("bms", "voltage_now", &uv)))
		return false;
	if (!(read_ps_long("battery", "current_now", &ua) || read_ps_long("bms", "current_now", &ua)))
		return false;
	*volts = (double)uv / 1000000.0;
	*milliamps = (double)ua / 1000.0;
	*watts = (*volts) * ((double)ua / 1000000.0);
	return true;
}

static bool usb_input_power(double *watts, double *volts, double *milliamps)
{
	long uv = 0, ua = 0;

	if (!supply_online("usb") && !supply_online("dc"))
		return false;
	if (!usb_input_voltage(&uv) || !usb_input_current(&ua))
		return false;
	*volts = (double)uv / 1000000.0;
	*milliamps = (double)labs(ua) / 1000.0;
	*watts = (*volts) * ((double)labs(ua) / 1000000.0);
	return true;
}

static void top_status_line(char *out, size_t len)
{
	char clock_part[32] = "";
	char bat_part[48] = "BAT ?";
	char usb_part[48] = "USB off";
	double watts = 0, volts = 0, ma = 0;
	time_t t = time(NULL);
	struct tm tm;

	if (localtime_r(&t, &tm))
		strftime(clock_part, sizeof(clock_part), "%H:%M:%S", &tm);
	else
		snprintf(clock_part, sizeof(clock_part), "--:--:--");
	if (battery_net_power(&watts, &volts, &ma))
		snprintf(bat_part, sizeof(bat_part), "BAT %+.2fW", watts);
	if (usb_input_power(&watts, &volts, &ma))
		snprintf(usb_part, sizeof(usb_part), "USB %.2fW", watts);
	snprintf(out, len, "%s  %s  %s", clock_part, bat_part, usb_part);
}

static void draw_top_status(struct canvas *c, uint32_t color)
{
	char line[160];
	int tw;
	int x;

	top_status_line(line, sizeof(line));
	tw = text_width(c, line, 3);
	x = c->width - tw - 48;
	if (x < 360)
		x = 360;
	text(c, x, 178, line, 3, color);
}

static void draw_charging_animation(struct canvas *c, int x, int y, uint32_t muted, uint32_t accent, uint32_t warn)
{
	char status[64] = "";
	int cap = battery_capacity_value();
	bool usb = supply_online("usb");
	bool dc = supply_online("dc");
	bool charging, full, plugged;
	int outer_w = 156, outer_h = 44, term_w = 10, border = 4;
	int ix = x + border, iy = y + border;
	int iw = outer_w - border * 2, ih = outer_h - border * 2;
	int fill_w;

	battery_status_value(status, sizeof(status));
	charging = strcasecmp(status, "Charging") == 0;
	full = strcasecmp(status, "Full") == 0 || cap >= 100;
	plugged = charging || full || usb || dc;
	if (!plugged)
		return;
	if (cap < 0)
		cap = full ? 100 : 0;
	if (cap > 100)
		cap = 100;

	rect(c, x, y, outer_w, outer_h, muted);
	rect(c, x + 2, y + 2, outer_w - 4, outer_h - 4, 0x00212b36);
	rect(c, x + outer_w, y + 13, term_w, 18, muted);
	rect(c, x + outer_w + 3, y + 16, term_w - 3, 12, 0x00212b36);

	fill_w = (iw * cap) / 100;
	rect(c, ix, iy, iw, ih, 0x0041505e);
	if (fill_w > 0)
		rect(c, ix, iy, fill_w, ih, full ? accent : 0x00078f76);

	if (charging && !full) {
		int pulse_w = 14;
		int span = iw + pulse_w;
		int px = ix + (int)((monotonic_ms() / 260) % span) - pulse_w;
		int visible_x = px < ix ? ix : px;
		int visible_w = px < ix ? pulse_w - (ix - px) : pulse_w;
		if (visible_x + visible_w > ix + iw)
			visible_w = ix + iw - visible_x;
		if (visible_w > 0)
			rect(c, visible_x, iy, visible_w, ih, warn);
	}
}

static void mem_line(char *out, size_t len, double *pct)
{
	FILE *f = fopen("/proc/meminfo", "r");
	char key[64], unit[32];
	long value, total = 0, avail = 0;
	*pct = 0;
	if (!f) {
		snprintf(out, len, "mem unknown");
		return;
	}
	while (fscanf(f, "%63s %ld %31s\n", key, &value, unit) == 3) {
		if (strcmp(key, "MemTotal:") == 0)
			total = value;
		else if (strcmp(key, "MemAvailable:") == 0)
			avail = value;
	}
	fclose(f);
	if (total > 0)
		*pct = (double)(total - avail) / (double)total;
	snprintf(out, len, "mem %ldMB / %ldMB", (total - avail) / 1024, total / 1024);
}

static void thermal_line(char *out, size_t len)
{
	int i;
	char path[256], type[128], temp[64];

	snprintf(out, len, "temp unknown");
	for (i = 0; i < 80; i++) {
		snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/type", i);
		if (!read_file(path, type, sizeof(type)))
			continue;
		for (char *p = type; *p; p++)
			*p = (char)tolower((unsigned char)*p);
		if (!strstr(type, "cpu") && !strstr(type, "soc"))
			continue;
		snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", i);
		if (read_file(path, temp, sizeof(temp))) {
			long v = atol(temp);
			if (v > 1000)
				snprintf(out, len, "%s %.1fC", type, (double)v / 1000.0);
			else
				snprintf(out, len, "%s %ldC", type, v);
			return;
		}
	}
}

static void kernel_line(char *out, size_t len)
{
	struct utsname u;
	if (uname(&u) == 0)
		snprintf(out, len, "%s %s %s", u.sysname, u.release, u.machine);
	else
		snprintf(out, len, "unknown");
}

static void load_line(char *out, size_t len)
{
	char buf[160], one[32], five[32], fifteen[32], run[32];

	if (read_file("/proc/loadavg", buf, sizeof(buf)) &&
	    sscanf(buf, "%31s %31s %31s %31s", one, five, fifteen, run) == 4) {
		snprintf(out, len, "%s %s %s run %s", one, five, fifteen, run);
		return;
	}
	snprintf(out, len, "unknown");
}

static void process_line(char *out, size_t len)
{
	DIR *d = opendir("/proc");
	struct dirent *de;
	int procs = 0;

	if (!d) {
		snprintf(out, len, "unknown");
		return;
	}
	while ((de = readdir(d))) {
		const char *p = de->d_name;
		if (!isdigit((unsigned char)*p))
			continue;
		while (*p && isdigit((unsigned char)*p))
			p++;
		if (*p == '\0')
			procs++;
	}
	closedir(d);
	snprintf(out, len, "%d processes", procs);
}

static void disk_line(char *out, size_t len, double *pct)
{
	struct statvfs st;
	unsigned long long total, avail, used;
	unsigned long long ufs_sectors = 0, data_sectors = 0;
	unsigned long long ufs_g = 0, data_g = 0, root_g = 0;

	*pct = 0;
	if (statvfs("/", &st) != 0 || st.f_blocks == 0) {
		snprintf(out, len, "disk unknown");
		return;
	}
	total = (unsigned long long)st.f_blocks * st.f_frsize;
	avail = (unsigned long long)st.f_bavail * st.f_frsize;
	used = total > avail ? total - avail : 0;
	if (total > 0)
		*pct = (double)used / (double)total;
	root_g = total >> 30;
	read_ull_file("/sys/class/block/sda/size", &ufs_sectors);
	read_ull_file("/sys/class/block/sda35/size", &data_sectors);
	ufs_g = (ufs_sectors * 512ull) >> 30;
	data_g = (data_sectors * 512ull) >> 30;
	if (ufs_g && data_g)
		snprintf(out, len, "ufs %lluG  userdata %lluG  root %llu/%lluG",
			ufs_g, data_g, used >> 30, root_g);
	else
		snprintf(out, len, "root %lluG / %lluG", used >> 30, root_g);
}

static void swap_line(char *out, size_t len)
{
	FILE *f = fopen("/proc/meminfo", "r");
	char key[64], unit[32];
	long value, total = 0, freev = 0;

	if (!f) {
		snprintf(out, len, "swap unknown");
		return;
	}
	while (fscanf(f, "%63s %ld %31s\n", key, &value, unit) == 3) {
		if (strcmp(key, "SwapTotal:") == 0)
			total = value;
		else if (strcmp(key, "SwapFree:") == 0)
			freev = value;
	}
	fclose(f);
	if (total == 0)
		snprintf(out, len, "none");
	else
		snprintf(out, len, "%ldMB / %ldMB", (total - freev) / 1024, total / 1024);
}

static bool proc_comm_exists(const char *name)
{
	DIR *d = opendir("/proc");
	struct dirent *de;

	if (!d)
		return false;
	while ((de = readdir(d))) {
		char path[128], comm[80];
		const char *p = de->d_name;
		if (!isdigit((unsigned char)*p))
			continue;
		snprintf(path, sizeof(path), "/proc/%s/comm", de->d_name);
		if (read_file(path, comm, sizeof(comm)) && strcmp(comm, name) == 0) {
			closedir(d);
			return true;
		}
	}
	closedir(d);
	return false;
}

static void docker_line(char *out, size_t len)
{
	bool dockerd = proc_comm_exists("dockerd");
	bool containerd = proc_comm_exists("containerd");
	struct stat st;

	if (dockerd || containerd)
		snprintf(out, len, "dockerd %s containerd %s", dockerd ? "on" : "off", containerd ? "on" : "off");
	else if (stat("/usr/bin/docker", &st) == 0)
		snprintf(out, len, "installed stopped");
	else
		snprintf(out, len, "not installed");
}

static void service_health_line(char *value, size_t value_len, char *detail,
	size_t detail_len, int *failed_count)
{
	static int64_t last_update_ms;
	static int cached_failed = -1;
	static char cached_value[32] = "?";
	static char cached_detail[160] = "systemd status pending";
	int64_t now = monotonic_ms();
	FILE *f;
	char line[256];
	int count = -1;
	char names[128] = "";
	int seen = 0;

	if (cached_failed >= 0 && now - last_update_ms < 5000) {
		snprintf(value, value_len, "%s", cached_value);
		snprintf(detail, detail_len, "%s", cached_detail);
		*failed_count = cached_failed;
		return;
	}

	f = popen("systemctl show -p NFailedUnits --value 2>/dev/null", "r");
	if (f) {
		if (fgets(line, sizeof(line), f)) {
			char *p = trim(line);
			if (isdigit((unsigned char)p[0]))
				count = atoi(p);
		}
		pclose(f);
	}

	if (count > 0) {
		f = popen("systemctl --failed --plain --no-legend --no-pager 2>/dev/null", "r");
		if (f) {
			while (seen < 2 && fgets(line, sizeof(line), f)) {
				char unit[96];
				size_t used;

				if (sscanf(line, "%95s", unit) != 1)
					continue;
				used = strlen(names);
				snprintf(names + used, sizeof(names) - used, "%s%s",
					seen ? ", " : "", unit);
				seen++;
			}
			pclose(f);
		}
	}

	cached_failed = count;
	last_update_ms = now;
	if (count < 0) {
		snprintf(cached_value, sizeof(cached_value), "?");
		snprintf(cached_detail, sizeof(cached_detail), "systemd status unavailable");
	} else if (count == 0) {
		snprintf(cached_value, sizeof(cached_value), "OK");
		snprintf(cached_detail, sizeof(cached_detail), "systemd no failed units");
	} else {
		snprintf(cached_value, sizeof(cached_value), "%d failed", count);
		if (names[0])
			snprintf(cached_detail, sizeof(cached_detail), "%s", names);
		else
			snprintf(cached_detail, sizeof(cached_detail), "failed units listed by systemd");
	}

	snprintf(value, value_len, "%s", cached_value);
	snprintf(detail, detail_len, "%s", cached_detail);
	*failed_count = cached_failed;
}

static void one_line(char *s)
{
	char *p;

	for (p = s; *p; p++) {
		if (*p == '\n' || *p == '\r' || *p == '\t')
			*p = ' ';
	}
}

static bool command_first_line(const char *cmd, char *out, size_t len)
{
	FILE *f;
	char line[256];

	if (len == 0)
		return false;
	out[0] = '\0';
	f = popen(cmd, "r");
	if (!f)
		return false;
	if (!fgets(line, sizeof(line), f)) {
		pclose(f);
		return false;
	}
	pclose(f);
	short_copy(out, len, trim(line), len - 1);
	one_line(out);
	return out[0] != '\0';
}

static bool read_state_value(const char *path, const char *key, char *out, size_t len)
{
	FILE *f;
	char line[256];
	size_t key_len = strlen(key);

	if (len == 0)
		return false;
	out[0] = '\0';
	f = fopen(path, "r");
	if (!f)
		return false;
	while (fgets(line, sizeof(line), f)) {
		char *p = trim(line);
		if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
			short_copy(out, len, p + key_len + 1, len - 1);
			fclose(f);
			return true;
		}
	}
	fclose(f);
	return false;
}

static void services_failed_summary(char *value, size_t value_len, char *detail, size_t detail_len,
	int *failed_count)
{
	FILE *f;
	char line[256];
	char names[144] = "";
	char count_raw[32] = "";
	int count = -1;
	int seen = 0;

	if (command_first_line("systemctl show -p NFailedUnits --value 2>/dev/null",
		count_raw, sizeof(count_raw)) && isdigit((unsigned char)count_raw[0]))
		count = atoi(count_raw);

	if (count > 0) {
		f = popen("systemctl --failed --plain --no-legend --no-pager 2>/dev/null", "r");
		if (f) {
			while (seen < 3 && fgets(line, sizeof(line), f)) {
				char unit[96];
				size_t used;
				if (sscanf(line, "%95s", unit) != 1)
					continue;
				used = strlen(names);
				if (used < sizeof(names) - 4)
					snprintf(names + used, sizeof(names) - used, "%s%s",
						seen ? ", " : "", unit);
				seen++;
			}
			pclose(f);
		}
	}

	*failed_count = count;
	if (count < 0) {
		snprintf(value, value_len, "?");
		snprintf(detail, detail_len, "systemd unavailable");
	} else if (count == 0) {
		snprintf(value, value_len, "OK");
		snprintf(detail, detail_len, "no failed units");
	} else {
		snprintf(value, value_len, "%d failed", count);
		snprintf(detail, detail_len, "%s", names[0] ? names : "failed units listed by systemd");
	}
}

static void service_unit_summary(const char *unit, char *out, size_t len)
{
	char cmd[256];
	FILE *f;
	char line[256];
	char active[32] = "?";
	char sub[48] = "?";
	char result[48] = "";

	snprintf(cmd, sizeof(cmd),
		"systemctl show '%s' -p ActiveState -p SubState -p Result 2>/dev/null",
		unit);
	f = popen(cmd, "r");
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			char *p = trim(line);
			if (strncmp(p, "ActiveState=", 12) == 0)
				short_copy(active, sizeof(active), p + 12, sizeof(active) - 1);
			else if (strncmp(p, "SubState=", 9) == 0)
				short_copy(sub, sizeof(sub), p + 9, sizeof(sub) - 1);
			else if (strncmp(p, "Result=", 7) == 0)
				short_copy(result, sizeof(result), p + 7, sizeof(result) - 1);
		}
		pclose(f);
	}
	if (result[0] && strcmp(result, "success") != 0)
		snprintf(out, len, "%s/%s result=%s", active, sub, result);
	else
		snprintf(out, len, "%s/%s", active, sub);
}

static void services_audio_summary(char *out, size_t len)
{
	char mode[48] = "";
	char adsp[48] = "";
	char nodes[32] = "";

	read_state_value("/run/alioth-audio-state", "mode", mode, sizeof(mode));
	read_state_value("/run/alioth-audio-state", "adsp_state", adsp, sizeof(adsp));
	read_state_value("/run/alioth-audio-state", "sound_nodes", nodes, sizeof(nodes));
	snprintf(out, len, "mode=%s adsp=%s nodes=%s",
		mode[0] ? mode : "?",
		adsp[0] ? adsp : "?",
		nodes[0] ? nodes : "?");
}

static void services_wifi_summary(char *out, size_t len)
{
	char mode[64] = "";
	char ssid[80] = "";
	char state[64] = "";

	read_state_value("/run/alioth-wifi-state", "mode", mode, sizeof(mode));
	read_state_value("/run/alioth-wifi-state", "ssid", ssid, sizeof(ssid));
	read_state_value("/run/alioth-wifi-state", "wpa=wpa_state", state, sizeof(state));
	snprintf(out, len, "%s  %s%s%s",
		mode[0] ? mode : "unknown",
		ssid[0] ? ssid : "no ssid",
		state[0] ? "  " : "",
		state[0] ? state : "");
}

static void read_command_lines(const char *cmd, char lines[][160], int max_lines)
{
	FILE *f;
	char line[256];
	int i;

	for (i = 0; i < max_lines; i++)
		lines[i][0] = '\0';
	f = popen(cmd, "r");
	if (!f)
		return;
	i = 0;
	while (i < max_lines && fgets(line, sizeof(line), f)) {
		short_copy(lines[i], 160, trim(line), 159);
		one_line(lines[i]);
		i++;
	}
	pclose(f);
}

static void update_services_page_cache(void)
{
	static const char *units[] = {
		"lele-status-ui.service",
		"alioth-wifi-connect.service",
		"alioth-wifi-bringup.service",
		"lele-usb-net.service",
		"ssh.service",
		"systemd-networkd.service",
		"alioth-audio-adsp-boot.service",
		"fwupd-refresh.timer",
	};
	int64_t now = monotonic_ms();
	int i;

	if (g_services_page_cache.last_update_ms > 0 &&
		now - g_services_page_cache.last_update_ms < 5000)
		return;

	memset(&g_services_page_cache, 0, sizeof(g_services_page_cache));
	g_services_page_cache.last_update_ms = now;
	services_failed_summary(g_services_page_cache.failed_value,
		sizeof(g_services_page_cache.failed_value),
		g_services_page_cache.failed_detail,
		sizeof(g_services_page_cache.failed_detail),
		&g_services_page_cache.failed_count);
	for (i = 0; i < (int)(sizeof(units) / sizeof(units[0])); i++)
		service_unit_summary(units[i], g_services_page_cache.service_lines[i],
			sizeof(g_services_page_cache.service_lines[i]));
	services_audio_summary(g_services_page_cache.audio_state,
		sizeof(g_services_page_cache.audio_state));
	services_wifi_summary(g_services_page_cache.wifi_state,
		sizeof(g_services_page_cache.wifi_state));
	if (read_file(GPU_METRICS_PATH, g_services_page_cache.panel_metrics,
		sizeof(g_services_page_cache.panel_metrics)))
		one_line(g_services_page_cache.panel_metrics);
	else
		snprintf(g_services_page_cache.panel_metrics,
			sizeof(g_services_page_cache.panel_metrics), "metrics unavailable");
	read_command_lines("tail -n 5 /run/alioth-status-ui.log 2>/dev/null",
		g_services_page_cache.log_tail, 5);
}

static int64_t monotonic_ms(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static bool parse_cpu_numbers(const char *line, uint64_t *total, uint64_t *idle_all)
{
	unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
	unsigned long long irq = 0, softirq = 0, steal = 0, guest = 0, guest_nice = 0;
	int matched;

	matched = sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
		&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
	if (matched < 4)
		return false;
	*idle_all = idle + iowait;
	*total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
	return true;
}

static void update_cpu_sample(struct cpu_sample *sample)
{
	FILE *f = fopen("/proc/stat", "r");
	char line[256];
	int seen_cores = 0;

	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		int idx = -1, core;
		uint64_t total, idle;
		double pct = 0;

		if (strncmp(line, "cpu ", 4) == 0) {
			idx = 0;
		} else if (sscanf(line, "cpu%d", &core) == 1 && core >= 0 && core < 16) {
			idx = core + 1;
			if (core + 1 > seen_cores)
				seen_cores = core + 1;
		} else {
			continue;
		}
		if (!parse_cpu_numbers(line, &total, &idle))
			continue;
		if (sample->valid && total > sample->prev_total[idx]) {
			uint64_t dt = total - sample->prev_total[idx];
			uint64_t di = idle >= sample->prev_idle[idx] ? idle - sample->prev_idle[idx] : 0;
			pct = dt > 0 ? 100.0 * (double)(dt - di) / (double)dt : 0;
			if (pct < 0)
				pct = 0;
			if (pct > 100)
				pct = 100;
		}
		if (idx == 0)
			sample->total_pct = pct;
		else
			sample->core_pct[idx - 1] = pct;
		sample->prev_total[idx] = total;
		sample->prev_idle[idx] = idle;
	}
	fclose(f);
	sample->cores = seen_cores;
	sample->valid = true;
}

static void draw_line(struct canvas *c, int x, int y, const char *label, const char *value, uint32_t label_color, uint32_t text_color)
{
	char clipped[96];
	short_copy(clipped, sizeof(clipped), value, 44);
	text(c, x, y, label, 3, label_color);
	text(c, x + 148, y, clipped, 3, text_color);
}

static int cpu_core_capacity_hint(int core)
{
	static int cached[16];
	char path[128], raw[32];
	long value;

	if (core < 0 || core >= 16)
		return 0;
	if (cached[core] != 0)
		return cached[core];
	snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpu_capacity", core);
	if (read_file(path, raw, sizeof(raw))) {
		value = strtol(raw, NULL, 10);
		if (value > 0) {
			cached[core] = (int)value;
			return cached[core];
		}
	}
	snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", core);
	if (read_file(path, raw, sizeof(raw))) {
		value = strtol(raw, NULL, 10);
		if (value > 0) {
			if (value >= 3000000)
				cached[core] = 1024;
			else if (value >= 2000000)
				cached[core] = 777;
			else
				cached[core] = 313;
			return cached[core];
		}
	}
	if (core >= 7)
		return 1024;
	if (core >= 4)
		return 777;
	return 313;
}

static const char *cpu_core_kind_label(int core)
{
	int cap = cpu_core_capacity_hint(core);

	if (cap >= 900)
		return "P";
	if (cap >= 500)
		return "B";
	return "L";
}

static uint32_t cpu_core_kind_color(int core, uint32_t accent)
{
	const char *kind = cpu_core_kind_label(core);

	if (kind[0] == 'P')
		return 0x00ff6170;
	if (kind[0] == 'B')
		return 0x00f4b84a;
	if (kind[0] == 'L')
		return 0x005aa8ff;
	return accent;
}

static void draw_cpu_grid(struct canvas *c, int x, int y, int w, const struct cpu_sample *cpu, uint32_t muted, uint32_t accent)
{
	int i, col_w = w / 2;
	char pct[16];

	for (i = 0; i < cpu->cores && i < 8; i++) {
		const char *kind = cpu_core_kind_label(i);
		uint32_t kind_color = cpu_core_kind_color(i, accent);
		int col = i / 4;
		int row = i % 4;
		int bx = x + col * col_w;
		int by = y + row * 60;
		int chip_x = bx + 58;
		int chip_w = 38;
		int tw = text_width(c, kind, 3);

		snprintf(pct, sizeof(pct), "C%d", i);
		text(c, bx, by, pct, 4, muted);
		rect(c, chip_x, by + 5, chip_w, 30, kind_color);
		text(c, chip_x + (chip_w - tw) / 2, by + 7, kind, 3, 0x000b0f14);
		snprintf(pct, sizeof(pct), "%3.0f%%", cpu->core_pct[i]);
		text(c, bx + 112, by, pct, 4, muted);
		bar(c, bx + 222, by + 10, col_w - 258, 22, cpu->core_pct[i] / 100.0, 0x0041505e, accent);
	}
}

#include "alioth-panel/ui_runtime.h"

static const char *page_name(enum app_page page)
{
	switch (page) {
	case PAGE_MONITOR:
		return "MONITOR";
	case PAGE_WIFI:
		return "WIFI";
	case PAGE_POWER:
		return "POWER";
	case PAGE_GPU:
		return "GPU";
	case PAGE_LOGS:
		return "LOGS";
	case PAGE_COUNT:
	default:
		return "?";
	}
}

static bool point_in(int px, int py, int x, int y, int w, int h)
{
	return px >= x && px < x + w && py >= y && py < y + h;
}

static int clamp_int(int value, int min_value, int max_value)
{
	if (value < min_value)
		return min_value;
	if (value > max_value)
		return max_value;
	return value;
}

static void draw_button(struct canvas *c, int x, int y, int w, int h, const char *label,
	bool active, uint32_t bg, uint32_t border, uint32_t fg)
{
	int tw = text_width(c, label, 3);
	int tx = x + (w - tw) / 2;
	int ty = y + (h - 21) / 2 - 1;

	if (tx < x + 16)
		tx = x + 16;
	rect(c, x, y, w, h, active ? border : bg);
	rect(c, x, y, w, 4, border);
	rect(c, x, y + h - 4, w, 4, border);
	rect(c, x, y, 4, h, border);
	rect(c, x + w - 4, y, 4, h, border);
	text(c, tx, ty, label, 3, fg);
}

static void draw_nav(struct canvas *c)
{
	int y = c->height - NAV_HEIGHT;
	int w = c->width / PAGE_COUNT;
	int i;

	rect(c, 0, y, c->width, NAV_HEIGHT, 0x00070a0f);
	rect(c, 0, y, c->width, 4, UI.line);
	for (i = 0; i < PAGE_COUNT; i++) {
		int x = i * w;
		bool active = g_app_page == (enum app_page)i;
		uint32_t fg = active ? UI.text : UI.muted;
		const char *name = page_name((enum app_page)i);
		int tw = text_width(c, name, 4);
		if (active) {
			rect(c, x + 18, y + 20, w - 36, 84, UI.panel_alt);
			rect(c, x + 18, y + 20, w - 36, 6, UI.accent);
		}
		text(c, x + (w - tw) / 2, y + 50, name, 4, fg);
	}
}

static void strip_line_end(char *s)
{
	size_t n = strlen(s);

	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
		s[--n] = '\0';
}

static bool read_wifi_config_pair(FILE *f, char *ssid, size_t ssid_len,
	char *psk, size_t psk_len)
{
	char ssid_line[WIFI_SSID_MAX];
	char psk_line[WIFI_PSK_MAX];

	while (fgets(ssid_line, sizeof(ssid_line), f)) {
		strip_line_end(ssid_line);
		if (!fgets(psk_line, sizeof(psk_line), f))
			psk_line[0] = '\0';
		else
			strip_line_end(psk_line);
		if (!ssid_line[0])
			continue;
		short_copy(ssid, ssid_len, ssid_line, WIFI_SSID_MAX - 1);
		short_copy(psk, psk_len, psk_line, WIFI_PSK_MAX - 1);
		return true;
	}
	return false;
}

static bool read_wifi_default_pair(char *ssid, size_t ssid_len, char *psk, size_t psk_len)
{
	FILE *f = fopen(WIFI_CONFIG_PATH, "r");
	bool ok = false;

	if (!f)
		return false;
	ok = read_wifi_config_pair(f, ssid, ssid_len, psk, psk_len);
	fclose(f);
	return ok;
}

static bool read_wifi_saved_psk(const char *wanted_ssid, char *psk, size_t psk_len)
{
	FILE *f;
	char ssid[WIFI_SSID_MAX];
	char saved_psk[WIFI_PSK_MAX];

	if (!wanted_ssid || !wanted_ssid[0])
		return false;
	f = fopen(WIFI_CONFIG_PATH, "r");
	if (!f)
		return false;
	while (read_wifi_config_pair(f, ssid, sizeof(ssid), saved_psk, sizeof(saved_psk))) {
		if (strcmp(ssid, wanted_ssid) == 0) {
			short_copy(psk, psk_len, saved_psk, WIFI_PSK_MAX - 1);
			fclose(f);
			return true;
		}
	}
	fclose(f);
	return false;
}

static bool wifi_fill_saved_password(void)
{
	char psk[WIFI_PSK_MAX] = "";

	if (!read_wifi_saved_psk(g_wifi.ssid, psk, sizeof(psk)))
		return false;
	short_copy(g_wifi.psk, sizeof(g_wifi.psk), psk, WIFI_PSK_MAX - 1);
	return true;
}

static void wifi_select_ssid(const char *ssid)
{
	bool saved;

	short_copy(g_wifi.ssid, sizeof(g_wifi.ssid), ssid, WIFI_SSID_MAX - 1);
	g_wifi.psk[0] = '\0';
	g_wifi.field = WIFI_FIELD_PSK;
	saved = wifi_fill_saved_password();
	if (saved && g_wifi.psk[0])
		snprintf(g_wifi.message, sizeof(g_wifi.message), "selected %s / password saved", g_wifi.ssid);
	else if (saved)
		snprintf(g_wifi.message, sizeof(g_wifi.message), "selected %s / open network saved", g_wifi.ssid);
	else
		snprintf(g_wifi.message, sizeof(g_wifi.message), "selected %s", g_wifi.ssid);
}

static void wifi_prefill_once(void)
{
	char ssid[WIFI_SSID_MAX] = "";
	char psk[WIFI_PSK_MAX] = "";
	char default_ssid[WIFI_SSID_MAX] = "";
	char default_psk[WIFI_PSK_MAX] = "";
	bool has_default;

	if (g_wifi.initialized)
		return;
	g_wifi.initialized = true;
	g_wifi.field = WIFI_FIELD_PSK;
	g_wifi.layout = KB_ALPHA;
	g_wifi.keyboard_visible = false;
	g_wifi.shift = false;
	g_wifi.scan_scroll = 0;
	g_wifi.scan_scroll_px = 0;
	g_wifi.scan_drag_active = false;
	g_wifi.scan_drag_last_y = 0;
	g_wifi.message[0] = '\0';

	has_default = read_wifi_default_pair(default_ssid, sizeof(default_ssid),
		default_psk, sizeof(default_psk));
	read_kv("/run/alioth-net-status", "ssid", ssid, sizeof(ssid));
	if (!ssid[0])
		read_kv("/run/alioth-wpa-status.log", "ssid", ssid, sizeof(ssid));
	if (!ssid[0])
		read_kv("/run/alioth-wifi-state", "ssid", ssid, sizeof(ssid));
	if (!ssid[0] && has_default)
		short_copy(ssid, sizeof(ssid), default_ssid, WIFI_SSID_MAX - 1);
	if (ssid[0])
		short_copy(g_wifi.ssid, sizeof(g_wifi.ssid), ssid, WIFI_SSID_MAX - 1);
	if (g_wifi.ssid[0] && read_wifi_saved_psk(g_wifi.ssid, psk, sizeof(psk)))
		short_copy(g_wifi.psk, sizeof(g_wifi.psk), psk, WIFI_PSK_MAX - 1);
	else if (has_default && strcmp(g_wifi.ssid, default_ssid) == 0)
		short_copy(g_wifi.psk, sizeof(g_wifi.psk), default_psk, WIFI_PSK_MAX - 1);
}

static int read_wifi_scan_entries(struct wifi_scan_entry *entries, int max_entries)
{
	FILE *f = fopen(WIFI_SCAN_PATH, "r");
	struct wifi_scan_entry all[WIFI_SCAN_MAX_UNIQUE];
	char line[256];
	int total = 0;
	int i, j;

	if (!f)
		return 0;
	memset(all, 0, sizeof(all));
	while (fgets(line, sizeof(line), f)) {
		char *p = trim(line);
		char *ssid = NULL;
		char bssid[32] = "", freq[16] = "-";
		double signal = -999.0;
		int consumed = 0;
		int fields = 0;
		char *q;
		int found = -1;

		if (!p[0] || strncmp(p, "iface=", 6) == 0 || strncmp(p, "BSSID", 5) == 0)
			continue;
		if (strstr(p, "<hidden>"))
			continue;
		if (sscanf(p, "%31s %lf %15s %n", bssid, &signal, freq, &consumed) >= 3 &&
			consumed > 0 && p[consumed]) {
			ssid = trim(p + consumed);
		} else {
			q = p;
			while (*q && fields < 3) {
				while (*q && isspace((unsigned char)*q))
					q++;
				while (*q && !isspace((unsigned char)*q))
					q++;
				fields++;
			}
			ssid = trim(q);
		}
		if (!ssid || !ssid[0])
			continue;
		for (i = 0; i < total; i++) {
			if (strcmp(all[i].ssid, ssid) == 0) {
				found = i;
				break;
			}
		}
		if (found < 0) {
			if (total >= WIFI_SCAN_MAX_UNIQUE)
				continue;
			found = total++;
			short_copy(all[found].ssid, sizeof(all[found].ssid), ssid, WIFI_SSID_MAX - 1);
			all[found].best_signal = signal;
			short_copy(all[found].freq, sizeof(all[found].freq), freq, sizeof(all[found].freq) - 1);
			all[found].ap_count = 1;
		} else {
			all[found].ap_count++;
			if (signal > all[found].best_signal) {
				all[found].best_signal = signal;
				short_copy(all[found].freq, sizeof(all[found].freq), freq, sizeof(all[found].freq) - 1);
			}
		}
	}
	fclose(f);
	for (i = 0; i < total; i++) {
		for (j = i + 1; j < total; j++) {
			if (all[j].best_signal > all[i].best_signal) {
				struct wifi_scan_entry tmp = all[i];
				all[i] = all[j];
				all[j] = tmp;
			}
		}
	}
	if (total > max_entries)
		total = max_entries;
	for (i = 0; i < total; i++) {
		entries[i] = all[i];
		if (entries[i].ap_count > 1)
			snprintf(entries[i].meta, sizeof(entries[i].meta), "%.0f %s %dAP",
				entries[i].best_signal, entries[i].freq, entries[i].ap_count);
		else
			snprintf(entries[i].meta, sizeof(entries[i].meta), "%.0f %s",
				entries[i].best_signal, entries[i].freq);
	}
	return total;
}

static int wifi_list_y(void)
{
	return 970;
}

static int wifi_list_h(void)
{
	return g_wifi.keyboard_visible ? 420 : 1100;
}

static int wifi_list_row_h(void)
{
	return g_wifi.keyboard_visible ? 72 : 112;
}

static int wifi_list_visible_count(void)
{
	return g_wifi.keyboard_visible ? 5 : 8;
}

static int wifi_list_body_y(void)
{
	return wifi_list_y() + 70;
}

static int wifi_list_body_h(void)
{
	return wifi_list_visible_count() * wifi_list_row_h();
}

static bool wifi_point_in_list(int x, int y)
{
	return point_in(x, y, 66, wifi_list_body_y(), g_screen_width - 132, wifi_list_body_h());
}

static void clamp_wifi_scroll(int count)
{
	int max_scroll;
	int max_px = (count - wifi_list_visible_count()) * wifi_list_row_h();

	if (max_px < 0)
		max_px = 0;
	g_wifi.scan_scroll_px = clamp_int(g_wifi.scan_scroll_px, 0, max_px);
	max_scroll = count - wifi_list_visible_count();
	if (max_scroll < 0)
		max_scroll = 0;
	g_wifi.scan_scroll = clamp_int(g_wifi.scan_scroll, 0, max_scroll);
	g_wifi.scan_scroll = g_wifi.scan_scroll_px / wifi_list_row_h();
}

static double wifi_signal_pct(double signal)
{
	double pct;

	if (signal < -95.0)
		signal = -95.0;
	if (signal > -35.0)
		signal = -35.0;
	pct = (signal + 95.0) / 60.0;
	if (pct < 0.0)
		pct = 0.0;
	if (pct > 1.0)
		pct = 1.0;
	return pct;
}

static const char *wifi_failure_text(const char *reason)
{
	if (!reason || !reason[0])
		return "unknown failure";
	if (strcmp(reason, "wrong-key") == 0)
		return "wrong password";
	if (strcmp(reason, "starting") == 0)
		return "starting";
	if (strcmp(reason, "connect-failed") == 0)
		return "connect failed";
	if (strcmp(reason, "wpa-4WAY_HANDSHAKE") == 0)
		return "handshake failed";
	if (strcmp(reason, "wpa-DISCONNECTED") == 0)
		return "disconnected";
	if (strncmp(reason, "wpa-", 4) == 0)
		return reason + 4;
	return reason;
}

static bool wifi_status_line(char *out, size_t len, bool *failed, bool *busy)
{
	char mode[64] = "";
	char reason[96] = "";
	char wpa[64] = "";

	if (failed)
		*failed = false;
	if (busy)
		*busy = false;
	if (len == 0)
		return false;
	out[0] = '\0';

	if (!read_kv("/run/alioth-wifi-state", "mode", mode, sizeof(mode)))
		return false;
	read_kv("/run/alioth-wifi-state", "reason", reason, sizeof(reason));
	read_kv("/run/alioth-wifi-state", "wpa=wpa_state", wpa, sizeof(wpa));

	if (strcmp(mode, "client-failed") == 0) {
		if (failed)
			*failed = true;
		snprintf(out, len, "failed: %s", wifi_failure_text(reason));
		return true;
	}
	if (strcmp(mode, "client-connecting") == 0) {
		if (busy)
			*busy = true;
		if (wpa[0] && reason[0])
			snprintf(out, len, "connecting: %s / %s", wifi_failure_text(reason), wpa);
		else if (wpa[0])
			snprintf(out, len, "connecting: %s", wpa);
		else
			snprintf(out, len, "connecting: %s", wifi_failure_text(reason));
		return true;
	}
	if (strcmp(mode, "client-connected") == 0) {
		snprintf(out, len, "connected");
		return true;
	}

	snprintf(out, len, "%s%s%s", mode, reason[0] ? ": " : "", reason[0] ? wifi_failure_text(reason) : "");
	return true;
}

static void draw_field(struct canvas *c, int x, int y, int w, int h, const char *label,
	const char *value, bool active)
{
	const uint32_t bg = UI.panel;
	const uint32_t border = active ? UI.accent : UI.line;
	const uint32_t text_color = UI.text;
	const uint32_t muted = UI.faint;
	char clipped[96];

	rect(c, x, y, w, h, bg);
	ui_outline(c, x, y, w, h, border);
	if (active)
		rect(c, x, y, w, 6, UI.accent);
	text(c, x + 18, y + 18, label, 2, muted);
	ui_ellipsize_copy(clipped, sizeof(clipped), value && value[0] ? value : "-", 44);
	while (text_width(c, clipped, 4) > w - 36 && strlen(clipped) > 6)
		ui_ellipsize_copy(clipped, sizeof(clipped), value && value[0] ? value : "-", (int)strlen(clipped) - 2);
	text(c, x + 18, y + 50, clipped, 4, text_color);
}

static void mask_password(const char *value, char *out, size_t len)
{
	size_t n;
	size_t i;

	if (len == 0)
		return;
	if (!value || !value[0]) {
		out[0] = '\0';
		return;
	}
	n = strlen(value);
	if (n > len - 1)
		n = len - 1;
	if (n > 24)
		n = 24;
	for (i = 0; i < n; i++)
		out[i] = '*';
	out[n] = '\0';
}

static const char *keyboard_row(enum keyboard_layout layout, int row)
{
	if (layout == KB_SYMBOL) {
		switch (row) {
		case 0: return "!@#$%^&*()";
		case 1: return "-_=+[]{}";
		case 2: return ";:'\",.?";
		case 3: return "/\\|~`<>";
		default: return "";
		}
	}
	switch (row) {
	case 0: return "1234567890";
	case 1: return "qwertyuiop";
	case 2: return "asdfghjkl";
	case 3: return "zxcvbnm.-_";
	default: return "";
	}
}

static void draw_key_row(struct canvas *c, int y, const char *chars, bool alpha_letters)
{
	const uint32_t key_bg = 0x001b2734;
	const uint32_t border = 0x0041505e;
	const uint32_t fg = 0x00e8f1ff;
	int n = (int)strlen(chars);
	int gap = 8;
	int margin = 30;
	int key_w;
	int x;
	int i;

	if (n <= 0)
		return;
	key_w = (c->width - margin * 2 - gap * (n - 1)) / n;
	x = (c->width - (key_w * n + gap * (n - 1))) / 2;
	for (i = 0; i < n; i++) {
		char label[2] = { chars[i], '\0' };
		if (alpha_letters && isalpha((unsigned char)label[0]) && g_wifi.shift)
			label[0] = (char)toupper((unsigned char)label[0]);
		draw_button(c, x + i * (key_w + gap), y, key_w, 104, label, false, key_bg, border, fg);
	}
}

static void draw_keyboard(struct canvas *c)
{
	const uint32_t bg = 0x000a1119;
	const uint32_t key_bg = 0x001b2734;
	const uint32_t border = 0x0041505e;
	const uint32_t active = 0x00076558;
	const uint32_t fg = 0x00e8f1ff;
	int y0 = c->height - NAV_HEIGHT - 730;
	int row_h = 118;
	int gap = 12;
	int row;
	int y;

	rect(c, 0, y0 - 18, c->width, 730, bg);
	for (row = 0; row < 4; row++) {
		bool alpha = g_wifi.layout == KB_ALPHA && row > 0;
		draw_key_row(c, y0 + row * (row_h + gap), keyboard_row(g_wifi.layout, row), alpha);
	}
	y = y0 + 4 * (row_h + gap);
	if (g_wifi.layout == KB_ALPHA) {
		draw_button(c, 30, y, 160, 112, g_wifi.shift ? "SHIFT*" : "SHIFT", g_wifi.shift, key_bg, active, fg);
		draw_button(c, 202, y, 135, 112, "SYM", false, key_bg, border, fg);
	} else {
		draw_button(c, 30, y, 307, 112, "ABC", false, key_bg, border, fg);
	}
	draw_button(c, 349, y, 330, 112, "SPACE", false, key_bg, border, fg);
	draw_button(c, 691, y, 160, 112, "DEL", false, key_bg, border, fg);
	draw_button(c, 863, y, 187, 112, "OK", false, key_bg, active, fg);
}

static void draw_wifi_page(struct canvas *c)
{
	char wifi[128], wifi_mode[160], ssid[128], route[128];
	char wifi_status[160] = "";
	char scan_msg[128] = "";
	char connect_tail[128] = "";
	char saved_psk[WIFI_PSK_MAX] = "";
	struct wifi_scan_entry entries[WIFI_SCAN_MAX_UNIQUE];
	bool wifi_failed = false;
	bool wifi_busy = false;
	bool stale_connect_message = false;
	uint32_t wifi_status_color = UI.text;
	int count;
	int network_y;
	int network_h;
	int row_h;
	int draw_count;
	int first;
	int scroll_px;
	int pixel_offset;
	int body_y;
	int body_h;
	int body_bottom;
	int i;

	wifi_prefill_once();
	iface_line("wlan0", 0, wifi, sizeof(wifi));
	wifi_mode_line(wifi_mode, sizeof(wifi_mode));
	ssid_line(ssid, sizeof(ssid));
	default_route_line(route, sizeof(route));
	wifi_status_line(wifi_status, sizeof(wifi_status), &wifi_failed, &wifi_busy);
	if (wifi_failed)
		wifi_status_color = UI.red;
	else if (wifi_busy)
		wifi_status_color = UI.amber;
	else if (wifi_status[0])
		wifi_status_color = UI.green;
	stale_connect_message = !wifi_busy && strncmp(g_wifi.message, "connecting to ", 14) == 0;
	count = read_wifi_scan_entries(entries, WIFI_SCAN_MAX_UNIQUE);
	clamp_wifi_scroll(count);
	read_file(WIFI_SCAN_LOG, scan_msg, sizeof(scan_msg));
	read_file(WIFI_CONNECT_LOG, connect_tail, sizeof(connect_tail));

	ui_page_header(c, "Wi-Fi", "wlan0 wireless control");

	ui_panel(c, 48, 260, c->width - 96, 310, "Connection", UI.blue);
	ui_info_row_fit(c, 78, 332, "Interface", wifi, UI.text, 820);
	ui_info_row_fit(c, 78, 386, "Mode", wifi_mode, UI.text, 820);
	ui_info_row_fit(c, 78, 440, "SSID", ssid,
		ssid[0] && strcmp(ssid, "not connected") != 0 ? UI.text : UI.amber, 820);
	ui_info_row_fit(c, 78, 494, "Route", route, UI.text, 820);
	ui_info_row_fit(c, 78, 532, "Status", wifi_status[0] ? wifi_status : "-", wifi_status_color, 820);

	ui_chip(c, 48, 580, 160, 84, "Scan", false, UI.blue);
	ui_chip(c, 224, 580, 190, 84, "Connect", false, UI.accent);
	ui_chip(c, 430, 580, 150, 84, "Clear", false, UI.line);
	ui_chip(c, 610, 580, 190, 84, g_wifi.field == WIFI_FIELD_SSID ? "SSID" : "Password",
		true, UI.accent);
	if (g_wifi.keyboard_visible)
		ui_chip(c, 820, 580, 210, 84, "Hide", true, UI.line);
	else
		ui_chip(c, 820, 580, 210, 84,
			read_wifi_saved_psk(g_wifi.ssid, saved_psk, sizeof(saved_psk)) ? "Saved" : "Not saved",
			false, UI.line);

	draw_field(c, 58, 700, c->width - 116, 108, "SSID", g_wifi.ssid,
		g_wifi.keyboard_visible && g_wifi.field == WIFI_FIELD_SSID);
	draw_field(c, 58, 830, c->width - 116, 108, "PASSWORD", g_wifi.psk,
		g_wifi.keyboard_visible && g_wifi.field == WIFI_FIELD_PSK);

	network_y = wifi_list_y();
	network_h = wifi_list_h();
	row_h = wifi_list_row_h();
	draw_count = wifi_list_visible_count();
	scroll_px = g_wifi.scan_scroll_px;
	first = scroll_px / row_h;
	pixel_offset = scroll_px % row_h;
	body_y = wifi_list_body_y();
	body_h = wifi_list_body_h();
	body_bottom = body_y + body_h;

	ui_panel(c, 48, network_y, c->width - 96, network_h, "Networks", UI.green);
	{
		char page[32];
		int last = first + draw_count + (pixel_offset > 0 ? 1 : 0);
		if (last > count)
			last = count;
		snprintf(page, sizeof(page), "%d-%d / %d", count ? first + 1 : 0, last, count);
		text(c, c->width - 220, network_y + 26, page, 2, UI.muted);
	}
	for (i = 0; i < draw_count + 2; i++) {
		int entry_index = first + i;
		int row_y = body_y - pixel_offset + i * row_h;
		int row_body_h = g_wifi.keyboard_visible ? 58 : 92;
		if (row_y >= body_bottom || row_y + row_body_h <= body_y)
			continue;
		rect(c, 66, row_y, c->width - 132, row_body_h,
			entry_index < count ? UI.panel_alt : 0x000f1720);
		if (entry_index < count) {
			char name[96];
			double signal = wifi_signal_pct(entries[entry_index].best_signal);
			int text_scale = g_wifi.keyboard_visible ? 3 : 4;
			ui_ellipsize_copy(name, sizeof(name), entries[entry_index].ssid,
				g_wifi.keyboard_visible ? 32 : 42);
			while (text_width(c, name, text_scale) > c->width - 470 && strlen(name) > 6)
				ui_ellipsize_copy(name, sizeof(name), entries[entry_index].ssid, (int)strlen(name) - 2);
			bar(c, 90, row_y + (g_wifi.keyboard_visible ? 20 : 37), 64, 14, signal, 0x002a3541,
				signal > 0.62 ? UI.green : (signal > 0.35 ? UI.amber : UI.red));
			{
				char index[8];
				snprintf(index, sizeof(index), "%d", entry_index + 1);
				text(c, 178, row_y + (g_wifi.keyboard_visible ? 16 : 28), index, text_scale, UI.muted);
			}
			text(c, 220, row_y + (g_wifi.keyboard_visible ? 16 : 28), name, text_scale, UI.text);
			if (entries[entry_index].meta[0])
				text(c, c->width - 282, row_y + (g_wifi.keyboard_visible ? 20 : 36),
					entries[entry_index].meta, 2, UI.muted);
		}
	}
	rect(c, 48, network_y, c->width - 96, 66, UI.panel);
	rect(c, 48, network_y, c->width - 96, 6, UI.green);
	text(c, 76, network_y + 26, "Networks", 3, UI.muted);
	{
		char page[32];
		int last = first + draw_count + (pixel_offset > 0 ? 1 : 0);
		if (last > count)
			last = count;
		snprintf(page, sizeof(page), "%d-%d / %d", count ? first + 1 : 0, last, count);
		text(c, c->width - 220, network_y + 26, page, 2, UI.muted);
	}
	rect(c, 48, body_bottom, c->width - 96, network_y + network_h - body_bottom, UI.panel);
	ui_outline(c, 48, network_y, c->width - 96, network_h, UI.line);
	if (count > draw_count) {
		int track_x = c->width - 86;
		int track_y = body_y;
		int track_h = body_h - (g_wifi.keyboard_visible ? 14 : 20);
		int max_px = (count - draw_count) * row_h;
		int thumb_h = track_h * draw_count / count;
		int thumb_y;
		if (thumb_h < 48)
			thumb_h = 48;
		if (thumb_h > track_h)
			thumb_h = track_h;
		thumb_y = track_y;
		if (max_px > 0)
			thumb_y += (track_h - thumb_h) * scroll_px / max_px;
		rect(c, track_x, track_y, 8, track_h, 0x002a3541);
		rect(c, track_x, thumb_y, 8, thumb_h, UI.accent);
	}

	if (wifi_failed && wifi_status[0])
		text(c, 68, g_wifi.keyboard_visible ? 1410 : 2090, wifi_status, 3, UI.red);
	else if (!stale_connect_message && g_wifi.message[0])
		text(c, 68, g_wifi.keyboard_visible ? 1410 : 2090, g_wifi.message, 2, UI.amber);
	else if (!wifi_busy && wifi_status[0])
		text(c, 68, g_wifi.keyboard_visible ? 1410 : 2090, wifi_status, 3, wifi_status_color);
	else if (connect_tail[0])
		text(c, 68, g_wifi.keyboard_visible ? 1410 : 2090, connect_tail, 2, UI.amber);
	else if (scan_msg[0])
		text(c, 68, g_wifi.keyboard_visible ? 1410 : 2090, scan_msg, 2, UI.muted);

	if (g_wifi.keyboard_visible)
		draw_keyboard(c);
	draw_nav(c);
}

static const char *gpu_helper_path(void)
{
	if (access(GPU_TEST_HELPER, X_OK) == 0)
		return GPU_TEST_HELPER;
	if (access(GPU_TEST_HELPER_ALT, X_OK) == 0)
		return GPU_TEST_HELPER_ALT;
	return GPU_TEST_HELPER;
}

static const char *gpu_icd_path(void)
{
	if (access(GPU_TEST_ICD, R_OK) == 0)
		return GPU_TEST_ICD;
	if (access(GPU_TEST_ICD_ALT, R_OK) == 0)
		return GPU_TEST_ICD_ALT;
	return GPU_TEST_ICD;
}

static bool read_log_line_containing(const char *needle, char *out, size_t len)
{
	FILE *f;
	char line[256];

	if (len == 0)
		return false;
	out[0] = '\0';
	f = fopen(GPU_TEST_LOG, "r");
	if (!f)
		return false;
	while (fgets(line, sizeof(line), f)) {
		char *p = trim(line);
		if (!strstr(p, needle))
			continue;
		short_copy(out, len, p, 52);
		fclose(f);
		return true;
	}
	fclose(f);
	return false;
}

static void gpu_nodes_line(char *out, size_t len)
{
	snprintf(out, len, "KGSL %s  DRM %s",
		file_exists_mode("/dev/kgsl-3d0", S_IFCHR) ? "OK" : "MISS",
		file_exists_mode("/dev/dri/card0", S_IFCHR) ? "OK" : "MISS");
}

static void gpu_runtime_line(char *out, size_t len)
{
	snprintf(out, len, "HELPER %s  ICD %s",
		access(gpu_helper_path(), X_OK) == 0 ? "OK" : "MISS",
		access(gpu_icd_path(), R_OK) == 0 ? "OK" : "MISS");
}

static void gpu_sysfs_line(char *out, size_t len)
{
	char model[64] = "";
	char busy[64] = "";
	char clock[64] = "";
	char resets[64] = "";
	char faults[64] = "";

	read_file("/sys/class/kgsl/kgsl-3d0/gpu_model", model, sizeof(model));
	read_file("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage", busy, sizeof(busy));
	read_file("/sys/class/kgsl/kgsl-3d0/clock_mhz", clock, sizeof(clock));
	read_file("/sys/class/kgsl/kgsl-3d0/reset_count", resets, sizeof(resets));
	read_file("/sys/class/kgsl/kgsl-3d0/snapshot/faultcount", faults, sizeof(faults));
	snprintf(out, len, "%s %s BUSY CLK %sMHZ R%s F%s",
		model[0] ? model : "ADRENO",
		busy[0] ? busy : "?",
		clock[0] ? clock : "?",
		resets[0] ? resets : "?",
		faults[0] ? faults : "?");
}

static void gpu_probe_line(char *out, size_t len)
{
	char line[160] = "";
	int64_t now;

	if (read_log_line_containing("alioth_gpu_probe=PASS", line, sizeof(line))) {
		g_gpu_test_started_ms = 0;
		g_gpu_message[0] = '\0';
		snprintf(out, len, "PASS SAFE QUEUE SUBMIT");
		return;
	}
	if (read_log_line_containing("failed", line, sizeof(line)) ||
		read_log_line_containing("FAILED", line, sizeof(line))) {
		g_gpu_test_started_ms = 0;
		snprintf(out, len, "FAIL %s", line);
		return;
	}
	if (g_gpu_test_started_ms > 0) {
		now = monotonic_ms();
		snprintf(out, len, "RUNNING %lldS", (long long)((now - g_gpu_test_started_ms) / 1000));
		return;
	}
	snprintf(out, len, "NOT RUN");
}

static void gpu_busy_clock_line(char *out, size_t len)
{
	char busy[32] = "";
	char clock[32] = "";

	read_file("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage", busy, sizeof(busy));
	read_file("/sys/class/kgsl/kgsl-3d0/clock_mhz", clock, sizeof(clock));
	if (busy[0] && strchr(busy, '%')) {
		snprintf(out, len, "KGSL %s  %sMHz", busy, clock[0] ? clock : "?");
	} else {
		snprintf(out, len, "KGSL %s%%  %sMHz",
			busy[0] ? busy : "?",
			clock[0] ? clock : "?");
	}
}

static void update_gpu_page_cache(void)
{
	int64_t now = monotonic_ms();

	if (g_gpu_page_cache.last_update_ms > 0 &&
		now - g_gpu_page_cache.last_update_ms < 500 &&
		g_gpu_page_cache.nodes[0])
		return;
	gpu_nodes_line(g_gpu_page_cache.nodes, sizeof(g_gpu_page_cache.nodes));
	gpu_runtime_line(g_gpu_page_cache.runtime, sizeof(g_gpu_page_cache.runtime));
	gpu_sysfs_line(g_gpu_page_cache.kgsl, sizeof(g_gpu_page_cache.kgsl));
	drm_line(g_gpu_page_cache.drm, sizeof(g_gpu_page_cache.drm));
	gpu_probe_line(g_gpu_page_cache.probe, sizeof(g_gpu_page_cache.probe));
	gpu_busy_clock_line(g_gpu_page_cache.busy_clock, sizeof(g_gpu_page_cache.busy_clock));
	g_gpu_page_cache.device[0] = '\0';
	g_gpu_page_cache.submit[0] = '\0';
	g_gpu_page_cache.verify[0] = '\0';
	g_gpu_page_cache.ext[0] = '\0';
	read_log_line_containing("physical_device=", g_gpu_page_cache.device,
		sizeof(g_gpu_page_cache.device));
	read_log_line_containing("gpu_probe_submit=PASS", g_gpu_page_cache.submit,
		sizeof(g_gpu_page_cache.submit));
	read_log_line_containing("gpu_probe_verify=PASS", g_gpu_page_cache.verify,
		sizeof(g_gpu_page_cache.verify));
	read_log_line_containing("required_ext VK_EXT_external_memory_dma_buf", g_gpu_page_cache.ext,
		sizeof(g_gpu_page_cache.ext));
	g_gpu_page_cache.last_update_ms = now;
}

static void gpu_metrics_tick(void)
{
	int64_t now = monotonic_ms();

	if (g_gpu_metrics.last_frame_ms > 0)
		g_gpu_metrics.frame_ms = (int)(now - g_gpu_metrics.last_frame_ms);
	g_gpu_metrics.last_frame_ms = now;
	if (g_gpu_metrics.window_ms == 0)
		g_gpu_metrics.window_ms = now;
	g_gpu_metrics.frames++;
	if (now - g_gpu_metrics.window_ms >= 1000) {
		int64_t window = now - g_gpu_metrics.window_ms;

		g_gpu_metrics.fps = (int)((g_gpu_metrics.frames * 1000 + window / 2) / window);
		if (g_gpu_metrics.frames > 0 && g_gpu_metrics.fps < 1)
			g_gpu_metrics.fps = 1;
		g_gpu_metrics.frames = 0;
		g_gpu_metrics.window_ms = now;
		{
			int fd = open(GPU_METRICS_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
			if (fd >= 0) {
				char line[192];
				int n = snprintf(line, sizeof(line),
					"fps=%d\nframe_ms=%d\npage=%s\npanel_hz=%d\n"
					"draw_ms=%d\ngpu_ms=%d\npresent_ms=%d\ntotal_ms=%d\n",
					g_gpu_metrics.fps, g_gpu_metrics.frame_ms, page_name(g_app_page), g_panel_refresh_hz,
					g_frame_draw_ms, g_frame_gpu_ms, g_frame_present_ms, g_frame_total_ms);
				if (n > 0)
					write_all_fd(fd, line, (size_t)n);
				close(fd);
			}
		}
	}
}

static void draw_gpu_page(struct canvas *c)
{
	char panel_value[32];
	char frame_detail[64];
	char busy[64] = "?";
	char clock[64] = "?";
	char clock_value[40];
	char timing[96];
	char total_timing[96];
	char bat_value[40] = "BAT ?";
	char usb_value[40] = "USB ?";
	char power_detail[96] = "";
	int render_ms;
	double watts = 0, volts = 0, ma = 0;

	update_gpu_page_cache();

	read_file("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage", busy, sizeof(busy));
	read_file("/sys/class/kgsl/kgsl-3d0/clock_mhz", clock, sizeof(clock));
	if (g_panel_refresh_hz > 0)
		snprintf(panel_value, sizeof(panel_value), "%dHz", g_panel_refresh_hz);
	else
		snprintf(panel_value, sizeof(panel_value), "?Hz");
	snprintf(frame_detail, sizeof(frame_detail), "direct scanout / 1s sample");
	snprintf(clock_value, sizeof(clock_value), "%s MHz", clock[0] ? clock : "?");
	snprintf(timing, sizeof(timing), "draw %dms  gpu %dms  present %dms",
		g_frame_draw_ms, g_frame_gpu_ms, g_frame_present_ms);
	render_ms = g_frame_draw_ms + g_frame_gpu_ms + g_frame_present_ms;
	snprintf(total_timing, sizeof(total_timing), "render %dms  wake %dms",
		render_ms, g_frame_total_ms);
	if (battery_net_power(&watts, &volts, &ma))
		snprintf(bat_value, sizeof(bat_value), "%+.2fW", watts);
	if (usb_input_power(&watts, &volts, &ma))
		snprintf(usb_value, sizeof(usb_value), "%.2fW", watts);
	snprintf(power_detail, sizeof(power_detail), "battery net / USB input");

	ui_page_header(c, "GPU Lab", "KGSL / Turnip / Vulkan renderer");

	ui_metric_card(c, 48, 260, 312, 250, "Panel rate", panel_value, frame_detail, UI.accent, -1.0);
	ui_metric_card(c, 384, 260, 312, 250, "KGSL busy", busy[0] ? busy : "?", "hardware busy percentage", UI.purple, -1.0);
	ui_metric_card(c, 720, 260, 312, 250, "Power", usb_value, power_detail, UI.amber, -1.0);

	ui_panel(c, 48, 545, c->width - 96, 390, "Renderer", UI.accent);
	ui_chip(c, 850, 574, 180, 92, "Probe", false, UI.blue);
	ui_info_row_fit(c, 78, 620, "Panel", g_panel_mode_name[0] ? g_panel_mode_name : "unknown panel mode",
		UI.text, 720);
	ui_info_row_fit(c, 78, 674, "Nodes", g_gpu_page_cache.nodes, UI.text, 720);
	ui_info_row_fit(c, 78, 728, "DRM", g_gpu_page_cache.drm, UI.text, 720);
	ui_info_row_fit(c, 78, 782, "KGSL", g_gpu_page_cache.kgsl, UI.text, 720);
	ui_info_row_fit(c, 78, 836, "ICD", g_gpu_page_cache.runtime, UI.text, 720);
	ui_info_row_fit(c, 78, 890, "Clock", clock_value, UI.text, 720);

	ui_panel(c, 48, 965, c->width - 96, 330, "Last probe", UI.green);
	ui_info_row_fit(c, 78, 1035, "Probe", g_gpu_page_cache.probe,
		strstr(g_gpu_page_cache.probe, "PASS") ? UI.green : UI.amber, c->width - 260);
	ui_info_row_fit(c, 78, 1089, "Device", g_gpu_page_cache.device[0] ? g_gpu_page_cache.device : "not tested",
		UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1143, "Submit", g_gpu_page_cache.submit[0] ? g_gpu_page_cache.submit : "-",
		UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1197, "Verify", g_gpu_page_cache.verify[0] ? g_gpu_page_cache.verify : "-",
		UI.text, c->width - 260);
	if (g_gpu_message[0])
		text(c, 78, 1249, g_gpu_message, 2, UI.amber);

	ui_panel(c, 48, 1325, c->width - 96, 410, "Render timing", UI.blue);
	ui_info_row_fit(c, 78, 1395, "Frame", frame_detail, UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1449, "Stages", timing, UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1503, "Total", total_timing, UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1557, "Battery", bat_value, UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1611, "USB", usb_value, UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1665, "Busy", g_gpu_page_cache.busy_clock, UI.text, c->width - 260);

	ui_panel(c, 48, 1765, c->width - 96, 310, "Runtime paths", UI.purple);
	ui_info_row_fit(c, 78, 1835, "Helper", gpu_helper_path(), UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1889, "ICD", gpu_icd_path(), UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1943, "Probe log", GPU_TEST_LOG, UI.text, c->width - 260);
	ui_info_row_fit(c, 78, 1997, "Metrics", GPU_METRICS_PATH, UI.text, c->width - 260);

	ui_panel(c, 48, 2105, c->width - 96, 100, "Status", UI.green);
	ui_info_row_fit(c, 78, 2160, "Renderer", "direct scanout active", UI.green,
		c->width - 260);

	draw_nav(c);
}

static uint32_t service_state_color(const char *line)
{
	if (strstr(line, "failed") || strstr(line, "result=exit-code"))
		return UI.red;
	if (strstr(line, "active/running") || strstr(line, "active/exited") ||
		strstr(line, "active/waiting"))
		return UI.green;
	if (strstr(line, "inactive") || strstr(line, "disabled") || strstr(line, "?"))
		return UI.amber;
	return UI.text;
}

static void draw_services_page(struct canvas *c)
{
	static const char *labels[] = {
		"Panel",
		"Wi-Fi conn",
		"Wi-Fi boot",
		"USB net",
		"SSH",
		"Networkd",
		"Audio",
		"fwupd",
	};
	struct services_page_cache *s = &g_services_page_cache;
	uint32_t health_color;
	int i;

	update_services_page_cache();
	health_color = s->failed_count > 0 ? UI.red : (s->failed_count == 0 ? UI.green : UI.amber);

	ui_page_header(c, "Services / Logs", "systemd health and runtime traces");

	ui_metric_card(c, 48, 260, 312, 250, "System", s->failed_value,
		s->failed_detail, health_color, -1.0);
	ui_metric_card(c, 384, 260, 312, 250, "Panel", "active",
		s->service_lines[0], service_state_color(s->service_lines[0]), -1.0);
	ui_metric_card(c, 720, 260, 312, 250, "Audio", s->failed_count > 0 ? "check" : "OK",
		s->audio_state, strstr(s->audio_state, "failed") ? UI.red : UI.amber, -1.0);

	ui_panel(c, 48, 545, c->width - 96, 560, "Key services", UI.accent);
	for (i = 0; i < 8; i++) {
		int y = 615 + i * 58;
		ui_info_row_fit(c, 78, y, labels[i], s->service_lines[i],
			service_state_color(s->service_lines[i]), c->width - 300);
	}

	ui_panel(c, 48, 1135, c->width - 96, 360, "Runtime state", UI.blue);
	ui_info_row_fit(c, 78, 1205, "Failed", s->failed_detail, health_color, c->width - 300);
	ui_info_row_fit(c, 78, 1259, "Wi-Fi", s->wifi_state, UI.text, c->width - 300);
	ui_info_row_fit(c, 78, 1313, "Audio", s->audio_state,
		strstr(s->audio_state, "failed") ? UI.red : UI.text, c->width - 300);
	ui_info_row_fit(c, 78, 1367, "Metrics", s->panel_metrics, UI.text, c->width - 300);

	ui_panel(c, 48, 1525, c->width - 96, 560, "Panel log tail", UI.purple);
	for (i = 0; i < 5; i++) {
		const char *line = s->log_tail[i][0] ? s->log_tail[i] : "-";
		ui_info_row_fit(c, 78, 1595 + i * 82, "log", line,
			strstr(line, "failed") ? UI.red : UI.text, c->width - 300);
	}

	ui_panel(c, 48, 2110, c->width - 96, 110, "Status", UI.green);
	ui_info_row_fit(c, 78, 2165, "Refresh", "service details cached for 5 seconds",
		UI.text, c->width - 300);

	draw_nav(c);
}

static const char *power_action_detail(enum power_action action)
{
	switch (action) {
	case POWER_BACK:
		return "Return to monitor";
	case POWER_OFF:
		return "System halt";
	case POWER_REBOOT:
		return "Restart Linux";
	case POWER_FASTBOOT:
		return "Bootloader mode";
	case POWER_ACTION_COUNT:
	default:
		return "";
	}
}

static void draw_power_page(struct canvas *c)
{
	char cap_value[32] = "?";
	char bat_detail[80] = "battery telemetry pending";
	char usb_value[32] = "USB ?";
	char usb_detail[96] = "input telemetry pending";
	char brightness_value[32] = "Backlight ?";
	char brightness_detail[96] = "panel backlight unavailable";
	char cpu_gov[96];
	char cpu_freq[128];
	char battery[160], charge[128], thermal[128], display[96];
	double power_w = 0, power_v = 0, power_ma = 0;
	double cap_pct = -1.0;
	double brightness_pct = -1.0;
	int cap = battery_capacity_value();
	int i;

	battery_line(battery, sizeof(battery));
	charge_speed_line(charge, sizeof(charge));
	thermal_line(thermal, sizeof(thermal));
	snprintf(display, sizeof(display), "%s", display_mode_name(g_display_mode));
	backlight_line(brightness_value, sizeof(brightness_value), brightness_detail,
		sizeof(brightness_detail), &brightness_pct);
	cpu_governor_line(cpu_gov, sizeof(cpu_gov));
	cpu_frequency_line(cpu_freq, sizeof(cpu_freq));
	if (cap >= 0) {
		snprintf(cap_value, sizeof(cap_value), "%d%%", cap);
		cap_pct = (double)cap / 100.0;
	}
	if (battery_net_power(&power_w, &power_v, &power_ma)) {
		char status[32] = "";
		battery_status_value(status, sizeof(status));
		snprintf(bat_detail, sizeof(bat_detail), "%s %+.2fW %.0fmA",
			status[0] ? status : "net", power_w, power_ma);
	}
	if (usb_input_power(&power_w, &power_v, &power_ma)) {
		snprintf(usb_value, sizeof(usb_value), "%.2fW", power_w);
		snprintf(usb_detail, sizeof(usb_detail), "%.2fV  %.0fmA input", power_v, power_ma);
	}

	ui_page_header(c, "Power", "battery, thermal and boot controls");

	ui_metric_card(c, 48, 260, 312, 250, "Battery", cap_value, bat_detail, UI.green, cap_pct);
	ui_metric_card(c, 384, 260, 312, 250, "USB input", usb_value, usb_detail, UI.amber, -1.0);
	ui_metric_card(c, 720, 260, 312, 250, "Backlight", brightness_value,
		brightness_detail, UI.purple, brightness_pct);

	ui_panel(c, 48, 535, c->width - 96, 300, "Live power", UI.amber);
	ui_info_row(c, 78, 612, "Battery", battery, UI.text);
	ui_info_row(c, 78, 666, "Charge", charge, UI.text);
	ui_info_row(c, 78, 720, "Thermal", thermal, UI.text);
	ui_info_row(c, 78, 774, "Screen", display, UI.text);

	if (g_ui_screen == UI_CONFIRM_ACTION) {
		ui_panel(c, 48, 855, c->width - 96, 560, "Confirm", UI.red);
		text(c, 96, 960, power_action_name(g_power_action), 7, UI.red);
		text(c, 100, 1065, power_action_detail(g_power_action), 3, UI.muted);
		ui_chip(c, 96, 1190, 390, 122, "Run", false, UI.red);
		ui_chip(c, 540, 1190, 390, 122, "Cancel", false, UI.accent);
		draw_nav(c);
		return;
	}

	g_ui_screen = UI_POWER_MENU;
	ui_panel(c, 48, 865, c->width - 96, 250, "CPU performance", UI.accent);
	ui_info_row_fit(c, 78, 935, "Governor", cpu_gov, UI.text, 360);
	ui_info_row_fit(c, 552, 935, "Freq", cpu_freq, UI.text, 390);
	ui_chip(c, 78, 1020, 180, 76, "Eco", cpu_governor_matches("powersave"), UI.green);
	ui_chip(c, 282, 1020, 180, 76, "Auto", cpu_governor_matches("schedutil"), UI.accent);
	ui_chip(c, 486, 1020, 180, 76, "Perf", cpu_governor_matches("performance"), UI.red);
	ui_info_row_fit(c, 700, 1030, "Last",
		g_cpu_governor_message[0] ? g_cpu_governor_message : "tap to apply",
		UI.muted, 250);

	ui_panel(c, 48, 1140, c->width - 96, 300, "Display controls", UI.blue);
	ui_chip(c, 78, 1220, 160, 82, "Normal", g_display_mode == DISPLAY_NORMAL, UI.accent);
	ui_chip(c, 250, 1220, 150, 82, "Low", g_display_mode == DISPLAY_LOW, UI.accent);
	ui_chip(c, 412, 1220, 160, 82, "Night", g_display_mode == DISPLAY_NIGHT, UI.accent);
	ui_chip(c, 584, 1220, 160, 82, "Lamp", g_display_mode == DISPLAY_LAMP, UI.accent);
	ui_chip(c, 756, 1220, 150, 82, "Off", g_display_mode == DISPLAY_OFF, UI.red);
	text(c, 78, 1350, "Brightness", 3, UI.faint);
	bar(c, 78, 1400, 450, 28, brightness_pct, 0x002a3541, UI.purple);
	ui_chip(c, 552, 1356, 88, 76, "-", false, UI.purple);
	ui_chip(c, 652, 1356, 88, 76, "25%", false, UI.purple);
	ui_chip(c, 752, 1356, 88, 76, "50%", false, UI.purple);
	ui_chip(c, 852, 1356, 88, 76, "75%", false, UI.purple);
	ui_chip(c, 952, 1356, 88, 76, "+", false, UI.purple);

	ui_panel(c, 48, 1465, c->width - 96, 640, "Actions", UI.accent);
	for (i = 0; i < POWER_ACTION_COUNT; i++) {
		int row_y = 1545 + i * 116;
		bool active_row = i == g_power_menu_index;
		uint32_t accent = i == POWER_BACK ? UI.blue : (i == POWER_OFF ? UI.red : UI.amber);
		rect(c, 78, row_y, c->width - 156, 104, active_row ? UI.panel_alt : 0x00131d28);
		if (active_row)
			rect(c, 78, row_y, 8, 104, accent);
		text(c, 112, row_y + 24, power_action_name((enum power_action)i), 4,
			active_row ? UI.text : UI.muted);
		text(c, 590, row_y + 30, power_action_detail((enum power_action)i), 3, UI.faint);
	}
	draw_nav(c);
}

static void display_line(char *out, size_t len)
{
	if (g_app_page == PAGE_WIFI) {
		snprintf(out, len, "WIFI page  touch scan/select/type/connect");
		return;
	}
	if (g_app_page == PAGE_POWER) {
		snprintf(out, len, "POWER page  touch action confirm");
		return;
	}
	if (g_app_page == PAGE_GPU) {
		snprintf(out, len, "GPU page  vulkan renderer metrics");
		return;
	}
	if (g_app_page == PAGE_LOGS) {
		snprintf(out, len, "LOGS page  service health details");
		return;
	}
	if (g_ui_screen == UI_POWER_MENU) {
		snprintf(out, len, "POWER MENU  touch action");
		return;
	}
	if (g_ui_screen == UI_CONFIRM_ACTION) {
		snprintf(out, len, "CONFIRM %s", power_action_name(g_power_action));
		return;
	}
	if (g_shutdown_confirm) {
		int64_t now = monotonic_ms();
		int remain = 0;
		if (g_shutdown_deadline_ms > now)
			remain = (int)((g_shutdown_deadline_ms - now + 999) / 1000);
		snprintf(out, len, "POWER OFF? %ds", remain);
		return;
	}
	snprintf(out, len, "%s", display_mode_name(g_display_mode));
}

#include "alioth-panel/panel_data.h"

static void draw_power_overlay(struct canvas *c)
{
	const uint32_t panel = 0x0008141c;
	const uint32_t border = 0x0000d1a7;
	const uint32_t selected = 0x00076558;
	const uint32_t text_color = 0x00e8f1ff;
	const uint32_t muted = 0x008fa7bd;
	const uint32_t warn = 0x00ffb020;
	int x = 86;
	int y = 390;
	int w = c->width - 172;
	int i;

	if (g_ui_screen == UI_STATUS)
		return;

	rect(c, x, y, w, 720, panel);
	rect(c, x, y, w, 6, border);
	rect(c, x, y + 714, w, 6, border);
	rect(c, x, y, 6, 720, border);
	rect(c, x + w - 6, y, 6, 720, border);

	if (g_ui_screen == UI_CONFIRM_ACTION) {
		text(c, x + 46, y + 58, "CONFIRM ACTION", 5, text_color);
		text(c, x + 46, y + 170, power_action_name(g_power_action), 7, warn);
		text(c, x + 46, y + 360, "RUN", 4, text_color);
		text(c, x + 46, y + 440, "CANCEL", 4, muted);
		return;
	}

	text(c, x + 46, y + 54, "POWER MENU", 5, text_color);
	text(c, x + 46, y + 130, "ACTIONS", 3, muted);
	for (i = 0; i < POWER_ACTION_COUNT; i++) {
		int row_y = y + 210 + i * 116;
		if (i == g_power_menu_index) {
			rect(c, x + 34, row_y - 24, w - 68, 86, selected);
			text(c, x + 66, row_y, power_action_name((enum power_action)i), 5, text_color);
		} else {
			text(c, x + 66, row_y, power_action_name((enum power_action)i), 5, muted);
		}
	}
}

static void draw_status(struct canvas *c, const struct cpu_sample *cpu)
{
	struct panel_data_snapshot data;
	const int left_value_w = 300;
	const int right_value_w = 330;
	const int full_value_w = 820;
	uint32_t health_color;

	if (g_display_mode == DISPLAY_OFF) {
		rect(c, 0, 0, c->width, c->height, 0x00000000);
		return;
	}
	if (g_display_mode == DISPLAY_LAMP) {
		rect(c, 0, 0, c->width, c->height, 0x00220802);
		return;
	}
	if (g_app_page == PAGE_WIFI) {
		draw_wifi_page(c);
		return;
	}
	if (g_app_page == PAGE_POWER) {
		draw_power_page(c);
		return;
	}
	if (g_app_page == PAGE_GPU) {
		draw_gpu_page(c);
		return;
	}
	if (g_app_page == PAGE_LOGS) {
		draw_services_page(c);
		return;
	}

	panel_data_refresh(&data, cpu);
	if (data.failed_units > 0)
		health_color = UI.red;
	else if (data.failed_units == 0)
		health_color = UI.green;
	else
		health_color = UI.amber;

	ui_page_header(c, data.title, "Headless Linux control panel");

	ui_metric_card(c, 48, 260, 312, 220, "Battery", data.battery_value,
		data.battery_detail, UI.green, data.battery_pct);
	ui_metric_card(c, 384, 260, 312, 220, "USB input", data.usb_value,
		data.usb_detail, UI.amber, -1.0);
	ui_metric_card(c, 720, 260, 312, 220, "Health", data.services_value,
		data.services_detail, health_color, -1.0);

	ui_panel(c, 48, 510, c->width - 96, 270, "Network", UI.blue);
	ui_info_row_fit(c, 78, 582, "USB", data.usb, UI.amber, full_value_w);
	ui_info_row_fit(c, 78, 636, "WLAN", data.wifi, UI.text, left_value_w);
	ui_info_row_fit(c, 552, 636, "Mode", data.wifi_mode, UI.text, right_value_w);
	ui_info_row_fit(c, 78, 690, "SSID", data.ssid,
		data.ssid[0] && strcmp(data.ssid, "not connected") != 0 ? UI.text : UI.amber,
		full_value_w);
	ui_info_row_fit(c, 78, 744, "Route", data.route, UI.text, full_value_w);

	ui_panel(c, 48, 810, c->width - 96, 250, "System", UI.accent);
	ui_info_row_fit(c, 78, 882, "Kernel", data.kernel, UI.text, full_value_w);
	ui_info_row_fit(c, 78, 936, "Distro", data.os, UI.text, full_value_w);
	ui_info_row_fit(c, 78, 990, "Mode", data.mode, UI.text, left_value_w);
	ui_info_row_fit(c, 552, 990, "Uptime", data.uptime, UI.text, right_value_w);
	ui_info_row_fit(c, 78, 1028, "Root", data.rootfs, UI.text, full_value_w);

	ui_panel(c, 48, 1080, c->width - 96, 330, "Runtime", UI.green);
	ui_info_row_fit(c, 78, 1152, "Load", data.load, UI.text, left_value_w);
	ui_info_row_fit(c, 78, 1204, "Proc", data.procs, UI.text, left_value_w);
	ui_info_row_fit(c, 78, 1256, "Memory", data.memory, UI.text, left_value_w);
	bar(c, 206, 1304, 292, 18, data.memory_pct, 0x002a3541, UI.green);
	ui_info_row_fit(c, 78, 1329, "Swap", data.swap, UI.text, left_value_w);
	ui_info_row_fit(c, 78, 1373, "Docker", data.docker, UI.text, left_value_w);
	ui_info_row_fit(c, 552, 1152, "Battery", data.battery, UI.text, right_value_w);
	ui_info_row_fit(c, 552, 1204, "Charge", data.charge, UI.text, right_value_w);
	ui_info_row_fit(c, 552, 1256, "Screen", data.display, UI.text, right_value_w);
	ui_info_row_fit(c, 552, 1308, "Thermal", data.thermal, UI.text, right_value_w);
	ui_info_row_fit(c, 552, 1360, "Services", data.services_detail,
		data.failed_units > 0 ? UI.red : UI.text, right_value_w);

	ui_panel(c, 48, 1420, c->width - 96, 420, "CPU", UI.accent);
	ui_info_row_fit(c, 78, 1488, "Total", data.cpu_all, UI.text, 390);
	ui_info_row_fit(c, 552, 1488, "Governor", data.cpu_governor, UI.text, right_value_w);
	ui_info_row_fit(c, 78, 1542, "Freq", data.cpu_freq, UI.text, 300);
	ui_info_row_fit(c, 552, 1542, "Top", data.cpu_top,
		strstr(data.cpu_top, "idle") ? UI.green : UI.amber, right_value_w);
	bar(c, 78, 1586, c->width - 156, 28, cpu->total_pct / 100.0, 0x002a3541, UI.accent);
	draw_cpu_grid(c, 78, 1628, c->width - 156, cpu, UI.muted, UI.accent);

	ui_panel(c, 48, 1860, c->width - 96, 360, "Hardware", UI.purple);
	ui_info_row_fit(c, 78, 1917, "Renderer", data.renderer_detail, UI.text, full_value_w);
	ui_info_row_fit(c, 78, 1963, "DRM", data.drm, UI.text, full_value_w);
	ui_info_row_fit(c, 78, 2017, "KGSL", data.kgsl, UI.text, full_value_w);
	ui_info_row_fit(c, 78, 2071, "Probe", data.gpu_probe,
		strstr(data.gpu_probe, "PASS") ? UI.green : UI.amber, full_value_w);
	ui_info_row_fit(c, 78, 2125, "Disk", data.disk, UI.text, full_value_w);
	bar(c, 78, 2172, c->width - 156, 22, data.disk_pct, 0x002a3541, UI.purple);
	ui_info_row_fit(c, 78, 2198, "AP", data.ap_clients, UI.text, full_value_w);

	draw_power_overlay(c);
	draw_nav(c);
}

static void init_input(struct input_state *in)
{
	int i;

	memset(in, 0, sizeof(*in));
	in->mode = DISPLAY_NORMAL;
	in->previous_lit_mode = DISPLAY_NORMAL;
	for (i = 0; i < 16; i++)
		in->fds[i] = -1;

	for (i = 0; i < 16 && in->count < 16; i++) {
		char path[64];
		char name[128] = "";
		int fd;
		bool keydev;
		bool touchdev = false;
		struct input_absinfo ax, ay;
		snprintf(path, sizeof(path), "/dev/input/event%d", i);
		fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
			name[0] = '\0';
		keydev = strstr(name, "qpnp_pon") || strstr(name, "gpio-keys");
		memset(&ax, 0, sizeof(ax));
		memset(&ay, 0, sizeof(ay));
		if (!in->touch_present &&
			ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ax) == 0 &&
			ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ay) == 0 &&
			ax.maximum > ax.minimum && ay.maximum > ay.minimum) {
			touchdev = true;
			in->touch_present = true;
			in->touch_min_x = ax.minimum;
			in->touch_max_x = ax.maximum;
			in->touch_min_y = ay.minimum;
			in->touch_max_y = ay.maximum;
		}
		if (!keydev && !touchdev) {
			close(fd);
			continue;
		}
		log_msg("input %s %s%s%s", path, name[0] ? name : "unknown",
			keydev ? " keys" : "", touchdev ? " touch" : "");
		in->fds[in->count] = fd;
		in->is_touch[in->count] = touchdev;
		short_copy(in->names[in->count], sizeof(in->names[in->count]), name[0] ? name : path, 100);
		in->count++;
	}
	sync_display_mode_from_file(in);
	log_msg("input fds=%d touch=%d range=%d,%d %d,%d display=%s", in->count,
		in->touch_present ? 1 : 0, in->touch_min_x, in->touch_max_x,
		in->touch_min_y, in->touch_max_y, display_mode_name(in->mode));
}

static void close_input(struct input_state *in)
{
	int i;
	for (i = 0; i < in->count; i++) {
		if (in->fds[i] >= 0)
			close(in->fds[i]);
	}
	in->count = 0;
}

static void set_display_mode(struct input_state *in, enum display_mode mode)
{
	in->mode = mode;
	g_display_mode = mode;
	if (mode != DISPLAY_OFF)
		in->previous_lit_mode = mode;
	write_display_mode(mode);
	log_msg("display mode=%s", display_mode_name(mode));
}

static void next_lit_mode(struct input_state *in)
{
	if (in->mode == DISPLAY_OFF)
		return;
	if (in->mode == DISPLAY_LAMP)
		set_display_mode(in, DISPLAY_NORMAL);
	else if (in->mode == DISPLAY_NIGHT)
		set_display_mode(in, DISPLAY_LOW);
	else if (in->mode == DISPLAY_LOW)
		set_display_mode(in, DISPLAY_NORMAL);
	else
		set_display_mode(in, DISPLAY_NIGHT);
}

static void prev_lit_mode(struct input_state *in)
{
	if (in->mode == DISPLAY_OFF)
		return;
	if (in->mode == DISPLAY_LAMP)
		set_display_mode(in, DISPLAY_NORMAL);
	else if (in->mode == DISPLAY_NORMAL)
		set_display_mode(in, DISPLAY_LOW);
	else if (in->mode == DISPLAY_LOW)
		set_display_mode(in, DISPLAY_NIGHT);
	else
		set_display_mode(in, DISPLAY_NORMAL);
}

static void set_page(struct input_state *in, enum app_page page)
{
	if (page < 0 || page >= PAGE_COUNT)
		return;
	if (in->mode == DISPLAY_OFF)
		set_display_mode(in, in->previous_lit_mode);
	g_app_page = page;
	if (page == PAGE_POWER) {
		if (g_ui_screen == UI_STATUS)
			g_ui_screen = UI_POWER_MENU;
	} else if (g_ui_screen == UI_POWER_MENU || g_ui_screen == UI_CONFIRM_ACTION) {
		g_ui_screen = UI_STATUS;
		g_power_action = POWER_BACK;
	}
	log_msg("page=%s", page_name(page));
}

static void apply_page_request(struct input_state *in)
{
	char page[64];

	if (!read_file(PAGE_REQUEST_PATH, page, sizeof(page)))
		return;
	unlink(PAGE_REQUEST_PATH);
	if (strcasecmp(page, "monitor") == 0 || strcmp(page, "0") == 0)
		set_page(in, PAGE_MONITOR);
	else if (strcasecmp(page, "wifi") == 0 || strcmp(page, "1") == 0)
		set_page(in, PAGE_WIFI);
	else if (strcasecmp(page, "power") == 0 || strcmp(page, "2") == 0)
		set_page(in, PAGE_POWER);
	else if (strcasecmp(page, "gpu") == 0 || strcasecmp(page, "test") == 0 ||
		strcasecmp(page, "gpu-test") == 0 || strcmp(page, "3") == 0)
		set_page(in, PAGE_GPU);
	else if (strcasecmp(page, "logs") == 0 || strcasecmp(page, "services") == 0 ||
		strcasecmp(page, "service") == 0 || strcmp(page, "4") == 0)
		set_page(in, PAGE_LOGS);
	else
		log_msg("unknown page request: %s", page);
}

static void apply_wifi_scroll_request(void)
{
	struct wifi_scan_entry entries[WIFI_SCAN_MAX_UNIQUE];
	char raw[32];
	int count;
	int delta;

	if (!read_file(WIFI_SCROLL_REQUEST_PATH, raw, sizeof(raw)))
		return;
	unlink(WIFI_SCROLL_REQUEST_PATH);
	if (strcasecmp(raw, "up") == 0)
		delta = -1;
	else if (strcasecmp(raw, "down") == 0)
		delta = 1;
	else
		delta = atoi(raw);
	count = read_wifi_scan_entries(entries, WIFI_SCAN_MAX_UNIQUE);
	g_wifi.scan_scroll_px += delta * wifi_list_row_h();
	clamp_wifi_scroll(count);
	snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi list %d/%d",
		count ? g_wifi.scan_scroll + 1 : 0, count);
}

static void handle_single_power(struct input_state *in)
{
	if (in->mode == DISPLAY_OFF)
		set_display_mode(in, in->previous_lit_mode);
	else
		set_display_mode(in, DISPLAY_OFF);
}

static void set_shutdown_confirm(struct input_state *in, bool enabled)
{
	int64_t now = monotonic_ms();

	in->shutdown_confirm = enabled;
	in->shutdown_deadline_ms = enabled ? now + SHUTDOWN_CONFIRM_MS : 0;
	in->power_pending = false;
	g_shutdown_confirm = enabled;
	g_shutdown_deadline_ms = in->shutdown_deadline_ms;
	if (enabled && in->mode == DISPLAY_OFF)
		set_display_mode(in, in->previous_lit_mode);
	log_msg("shutdown confirm=%d", enabled ? 1 : 0);
}

static void request_poweroff(void)
{
	pid_t pid = fork();

	if (pid < 0) {
		log_msg("poweroff fork failed: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		setsid();
		execl("/var/tmp/alioth-switchroot/alioth-reboot", "alioth-reboot", "poweroff", NULL);
		execl("/bin/alioth-reboot", "alioth-reboot", "poweroff", NULL);
		execl("/sbin/poweroff", "poweroff", NULL);
		execl("/usr/sbin/poweroff", "poweroff", NULL);
		execl("/bin/systemctl", "systemctl", "poweroff", NULL);
		execl("/usr/bin/systemctl", "systemctl", "poweroff", NULL);
		_exit(127);
	}
	log_msg("poweroff requested pid=%d", (int)pid);
}

static void request_reboot(void)
{
	pid_t pid = fork();

	if (pid < 0) {
		log_msg("reboot fork failed: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		setsid();
		execl("/bin/systemctl", "systemctl", "reboot", "--no-wall", NULL);
		execl("/usr/bin/systemctl", "systemctl", "reboot", "--no-wall", NULL);
		execl("/sbin/reboot", "reboot", NULL);
		execl("/usr/sbin/reboot", "reboot", NULL);
		_exit(127);
	}
	log_msg("reboot requested pid=%d", (int)pid);
}

static void request_fastboot(void)
{
	pid_t pid = fork();

	if (pid < 0) {
		log_msg("fastboot fork failed: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		setsid();
		execl("/var/tmp/alioth-switchroot/alioth-reboot", "alioth-reboot", "bootloader", NULL);
		execl("/bin/alioth-reboot", "alioth-reboot", "bootloader", NULL);
		execl("/bin/systemctl", "systemctl", "reboot", "--boot-loader-entry=auto-reboot-to-bootloader", NULL);
		_exit(127);
	}
	log_msg("fastboot requested pid=%d", (int)pid);
}

static void reap_children(void)
{
	int status;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		log_msg("child pid=%d status=%d", (int)pid, status);
}

static int wifi_saved_index(const struct wifi_saved_network *items, int count, const char *ssid)
{
	int i;

	for (i = 0; i < count; i++) {
		if (strcmp(items[i].ssid, ssid) == 0)
			return i;
	}
	return -1;
}

static bool write_wifi_default(void)
{
	int fd;
	FILE *f;
	struct wifi_saved_network saved[WIFI_SAVED_MAX];
	int saved_count = 0;
	int i;

	if (!g_wifi.ssid[0])
		return false;
	f = fopen(WIFI_CONFIG_PATH, "r");
	if (f) {
		char ssid[WIFI_SSID_MAX];
		char psk[WIFI_PSK_MAX];

		while (saved_count < WIFI_SAVED_MAX - 1 &&
			read_wifi_config_pair(f, ssid, sizeof(ssid), psk, sizeof(psk))) {
			if (strcmp(ssid, g_wifi.ssid) == 0)
				continue;
			if (wifi_saved_index(saved, saved_count, ssid) >= 0)
				continue;
			short_copy(saved[saved_count].ssid, sizeof(saved[saved_count].ssid),
				ssid, WIFI_SSID_MAX - 1);
			short_copy(saved[saved_count].psk, sizeof(saved[saved_count].psk),
				psk, WIFI_PSK_MAX - 1);
			saved_count++;
		}
		fclose(f);
	}
	fd = open(WIFI_CONFIG_TMP_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0) {
		snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi config write failed: %s", strerror(errno));
		return false;
	}
	{
		char line[WIFI_SSID_MAX + WIFI_PSK_MAX + 8];
		int n = snprintf(line, sizeof(line), "%s\n%s\n", g_wifi.ssid, g_wifi.psk);

		if (n <= 0 || !write_all_fd(fd, line, (size_t)n)) {
			close(fd);
			unlink(WIFI_CONFIG_TMP_PATH);
			snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi config write failed");
			return false;
		}
	}
	for (i = 0; i < saved_count; i++) {
		char line[WIFI_SSID_MAX + WIFI_PSK_MAX + 8];
		int n = snprintf(line, sizeof(line), "%s\n%s\n", saved[i].ssid, saved[i].psk);

		if (n <= 0 || !write_all_fd(fd, line, (size_t)n)) {
			close(fd);
			unlink(WIFI_CONFIG_TMP_PATH);
			snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi config write failed");
			return false;
		}
	}
	fsync(fd);
	if (close(fd) != 0) {
		unlink(WIFI_CONFIG_TMP_PATH);
		snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi config write failed");
		return false;
	}
	chmod(WIFI_CONFIG_TMP_PATH, 0600);
	if (rename(WIFI_CONFIG_TMP_PATH, WIFI_CONFIG_PATH) != 0) {
		unlink(WIFI_CONFIG_TMP_PATH);
		snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi config write failed: %s", strerror(errno));
		return false;
	}
	return true;
}

static void request_gpu_probe(void)
{
	const char *helper = gpu_helper_path();
	const char *icd = gpu_icd_path();
	pid_t pid;
	int log_fd;

	if (access(helper, X_OK) != 0) {
		snprintf(g_gpu_message, sizeof(g_gpu_message), "gpu helper missing");
		return;
	}
	if (access(icd, R_OK) != 0) {
		snprintf(g_gpu_message, sizeof(g_gpu_message), "vulkan icd missing");
		return;
	}
	log_fd = open(GPU_TEST_LOG, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (log_fd < 0) {
		snprintf(g_gpu_message, sizeof(g_gpu_message), "gpu log open failed");
		return;
	}
	pid = fork();
	if (pid < 0) {
		close(log_fd);
		snprintf(g_gpu_message, sizeof(g_gpu_message), "gpu probe fork failed");
		return;
	}
	if (pid == 0) {
		setsid();
		dup2(log_fd, STDOUT_FILENO);
		dup2(log_fd, STDERR_FILENO);
		close(log_fd);
		setenv("LD_LIBRARY_PATH", GPU_TEST_LD_LIBRARY_PATH, 1);
		setenv("VK_ICD_FILENAMES", icd, 1);
		setenv("TU_DEBUG", "startup", 1);
		execl(helper, "alioth_gpu_monitor_service", "probe", NULL);
		_exit(127);
	}
	close(log_fd);
	g_gpu_test_started_ms = monotonic_ms();
	snprintf(g_gpu_message, sizeof(g_gpu_message), "gpu probe running...");
	log_msg("gpu probe requested pid=%d helper=%s", (int)pid, helper);
}

static void request_wifi_scan(void)
{
	pid_t pid;
	int out_fd, err_fd;

	out_fd = open(WIFI_SCAN_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	err_fd = open(WIFI_SCAN_LOG, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (out_fd < 0 || err_fd < 0) {
		if (out_fd >= 0) close(out_fd);
		if (err_fd >= 0) close(err_fd);
		snprintf(g_wifi.message, sizeof(g_wifi.message), "scan log open failed");
		return;
	}
	pid = fork();
	if (pid < 0) {
		close(out_fd);
		close(err_fd);
		snprintf(g_wifi.message, sizeof(g_wifi.message), "scan fork failed");
		return;
	}
	if (pid == 0) {
		setsid();
		dup2(out_fd, STDOUT_FILENO);
		dup2(err_fd, STDERR_FILENO);
		close(out_fd);
		close(err_fd);
		execl("/usr/local/sbin/alioth-wifi-scan", "alioth-wifi-scan", NULL);
		execl("/bin/alioth-wifi-scan", "alioth-wifi-scan", NULL);
		_exit(127);
	}
	close(out_fd);
	close(err_fd);
	g_wifi.scan_started_ms = monotonic_ms();
	snprintf(g_wifi.message, sizeof(g_wifi.message), "scanning wifi...");
	log_msg("wifi scan requested pid=%d", (int)pid);
}

static void request_wifi_connect(void)
{
	pid_t pid;
	int log_fd;

	if (!g_wifi.ssid[0]) {
		snprintf(g_wifi.message, sizeof(g_wifi.message), "ssid is empty");
		return;
	}
	if (!g_wifi.psk[0])
		wifi_fill_saved_password();
	if (g_wifi.psk[0] && strlen(g_wifi.psk) < 8) {
		snprintf(g_wifi.message, sizeof(g_wifi.message), "password must be at least 8 chars");
		return;
	}
	if (!write_wifi_default())
		return;
	log_fd = open(WIFI_CONNECT_LOG, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (log_fd < 0) {
		snprintf(g_wifi.message, sizeof(g_wifi.message), "connect log open failed");
		return;
	}
	pid = fork();
	if (pid < 0) {
		close(log_fd);
		snprintf(g_wifi.message, sizeof(g_wifi.message), "connect fork failed");
		return;
	}
	if (pid == 0) {
		setsid();
		dup2(log_fd, STDOUT_FILENO);
		dup2(log_fd, STDERR_FILENO);
		close(log_fd);
		if (g_wifi.psk[0])
			execl("/usr/local/sbin/alioth-wifi-connect", "alioth-wifi-connect",
				g_wifi.ssid, g_wifi.psk, NULL);
		else
			execl("/usr/local/sbin/alioth-wifi-connect", "alioth-wifi-connect",
				g_wifi.ssid, NULL);
		if (g_wifi.psk[0])
			execl("/bin/alioth-wifi-connect", "alioth-wifi-connect", g_wifi.ssid, g_wifi.psk, NULL);
		else
			execl("/bin/alioth-wifi-connect", "alioth-wifi-connect", g_wifi.ssid, NULL);
		_exit(127);
	}
	close(log_fd);
	g_wifi.connect_started_ms = monotonic_ms();
	g_wifi.keyboard_visible = false;
	snprintf(g_wifi.message, sizeof(g_wifi.message), "connecting to %s...", g_wifi.ssid);
	log_msg("wifi connect requested pid=%d ssid=%s", (int)pid, g_wifi.ssid);
}

static void execute_power_action(enum power_action action)
{
	log_msg("power action execute=%s", power_action_name(action));
	switch (action) {
	case POWER_OFF:
		request_poweroff();
		break;
	case POWER_REBOOT:
		request_reboot();
		break;
	case POWER_FASTBOOT:
		request_fastboot();
		break;
	case POWER_BACK:
	case POWER_ACTION_COUNT:
	default:
		break;
	}
}

static void wifi_append_char(char ch)
{
	char *target = g_wifi.field == WIFI_FIELD_SSID ? g_wifi.ssid : g_wifi.psk;
	size_t limit = g_wifi.field == WIFI_FIELD_SSID ? WIFI_SSID_MAX : WIFI_PSK_MAX;
	size_t n = strlen(target);

	if (n + 1 >= limit)
		return;
	target[n] = ch;
	target[n + 1] = '\0';
	g_wifi.message[0] = '\0';
}

static void wifi_backspace(void)
{
	char *target = g_wifi.field == WIFI_FIELD_SSID ? g_wifi.ssid : g_wifi.psk;
	size_t n = strlen(target);

	if (n > 0)
		target[n - 1] = '\0';
	g_wifi.message[0] = '\0';
}

static void wifi_clear_active(void)
{
	char *target = g_wifi.field == WIFI_FIELD_SSID ? g_wifi.ssid : g_wifi.psk;
	target[0] = '\0';
	g_wifi.message[0] = '\0';
}

static bool handle_keyboard_touch(int x, int y)
{
	int y0 = g_screen_height - NAV_HEIGHT - 730;
	int row_h = 118;
	int gap = 12;
	int row;

	if (y < y0 || y >= y0 + 5 * (row_h + gap))
		return false;
	row = (y - y0) / (row_h + gap);
	if (row >= 0 && row < 4) {
		const char *chars = keyboard_row(g_wifi.layout, row);
		int n = (int)strlen(chars);
		int key_gap = 8;
		int margin = 30;
		int key_w, x0, idx, kx;

		if ((y - (y0 + row * (row_h + gap))) >= 104 || n <= 0)
			return true;
		key_w = (g_screen_width - margin * 2 - key_gap * (n - 1)) / n;
		x0 = (g_screen_width - (key_w * n + key_gap * (n - 1))) / 2;
		if (x < x0)
			return true;
		idx = (x - x0) / (key_w + key_gap);
		if (idx < 0 || idx >= n)
			return true;
		kx = x0 + idx * (key_w + key_gap);
		if (!point_in(x, y, kx, y0 + row * (row_h + gap), key_w, 104))
			return true;
		if (g_wifi.layout == KB_ALPHA && row > 0 && isalpha((unsigned char)chars[idx]) && g_wifi.shift)
			wifi_append_char((char)toupper((unsigned char)chars[idx]));
		else
			wifi_append_char(chars[idx]);
		return true;
	}

	{
		int special_y = y0 + 4 * (row_h + gap);
	if (g_wifi.layout == KB_ALPHA) {
		if (point_in(x, y, 30, special_y, 160, 112)) {
			g_wifi.shift = !g_wifi.shift;
			return true;
		}
		if (point_in(x, y, 202, special_y, 135, 112)) {
			g_wifi.layout = KB_SYMBOL;
			g_wifi.shift = false;
			return true;
		}
	} else if (point_in(x, y, 30, special_y, 307, 112)) {
		g_wifi.layout = KB_ALPHA;
		return true;
	}
	if (point_in(x, y, 349, special_y, 330, 112)) {
		wifi_append_char(' ');
		return true;
	}
	if (point_in(x, y, 691, special_y, 160, 112)) {
		wifi_backspace();
		return true;
	}
	if (point_in(x, y, 863, special_y, 187, 112)) {
		if (g_wifi.field == WIFI_FIELD_SSID)
			g_wifi.field = WIFI_FIELD_PSK;
		else
			request_wifi_connect();
		return true;
	}
	}
	return true;
}

static void handle_wifi_touch(struct input_state *in, int x, int y)
{
	struct wifi_scan_entry entries[WIFI_SCAN_MAX_UNIQUE];
	int count;
	int row_h;
	int body_y;
	int pixel_offset;
	int row;
	int entry_index;
	(void)in;

	wifi_prefill_once();
	if (point_in(x, y, 48, 580, 160, 84)) {
		request_wifi_scan();
		return;
	}
	if (point_in(x, y, 224, 580, 190, 84)) {
		request_wifi_connect();
		return;
	}
	if (point_in(x, y, 430, 580, 150, 84)) {
		wifi_clear_active();
		return;
	}
	if (point_in(x, y, 610, 580, 190, 84)) {
		g_wifi.field = g_wifi.field == WIFI_FIELD_SSID ? WIFI_FIELD_PSK : WIFI_FIELD_SSID;
		return;
	}
	if (g_wifi.keyboard_visible && point_in(x, y, 820, 580, 210, 84)) {
		g_wifi.keyboard_visible = false;
		return;
	}
	if (point_in(x, y, 58, 700, g_screen_width - 116, 108)) {
		g_wifi.field = WIFI_FIELD_SSID;
		g_wifi.keyboard_visible = true;
		return;
	}
	if (point_in(x, y, 58, 830, g_screen_width - 116, 108)) {
		g_wifi.field = WIFI_FIELD_PSK;
		g_wifi.keyboard_visible = true;
		return;
	}
	count = read_wifi_scan_entries(entries, WIFI_SCAN_MAX_UNIQUE);
	clamp_wifi_scroll(count);
	row_h = wifi_list_row_h();
	body_y = wifi_list_body_y();
	pixel_offset = g_wifi.scan_scroll_px % row_h;
	if (wifi_point_in_list(x, y)) {
		row = (y - body_y + pixel_offset) / row_h;
		entry_index = g_wifi.scan_scroll_px / row_h + row;
		if (row >= 0 && row < wifi_list_visible_count() && entry_index >= 0 &&
			entry_index < count) {
			wifi_select_ssid(entries[entry_index].ssid);
			return;
		}
	}
	if (g_wifi.keyboard_visible)
		handle_keyboard_touch(x, y);
}

static void handle_power_touch(struct input_state *in, int x, int y)
{
	int i;

	if (g_ui_screen == UI_CONFIRM_ACTION) {
		if (point_in(x, y, 96, 1190, 390, 122)) {
			enum power_action action = g_power_action;
			g_ui_screen = UI_STATUS;
			g_app_page = PAGE_MONITOR;
			g_power_action = POWER_BACK;
			execute_power_action(action);
			return;
		}
		if (point_in(x, y, 540, 1190, 390, 122)) {
			g_ui_screen = UI_POWER_MENU;
			g_power_action = POWER_BACK;
			return;
		}
		return;
	}
	if (point_in(x, y, 78, 1020, 180, 76)) {
		set_cpu_governor("powersave");
		return;
	}
	if (point_in(x, y, 282, 1020, 180, 76)) {
		set_cpu_governor("schedutil");
		return;
	}
	if (point_in(x, y, 486, 1020, 180, 76)) {
		set_cpu_governor("performance");
		return;
	}
	if (point_in(x, y, 78, 1220, 160, 82)) {
		set_display_mode(in, DISPLAY_NORMAL);
		return;
	}
	if (point_in(x, y, 250, 1220, 150, 82)) {
		set_display_mode(in, DISPLAY_LOW);
		return;
	}
	if (point_in(x, y, 412, 1220, 160, 82)) {
		set_display_mode(in, DISPLAY_NIGHT);
		return;
	}
	if (point_in(x, y, 584, 1220, 160, 82)) {
		set_display_mode(in, DISPLAY_LAMP);
		return;
	}
	if (point_in(x, y, 756, 1220, 150, 82)) {
		set_display_mode(in, DISPLAY_OFF);
		return;
	}
	if (point_in(x, y, 552, 1356, 88, 76)) {
		adjust_backlight_percent(-10);
		return;
	}
	if (point_in(x, y, 652, 1356, 88, 76)) {
		set_backlight_percent(25);
		return;
	}
	if (point_in(x, y, 752, 1356, 88, 76)) {
		set_backlight_percent(50);
		return;
	}
	if (point_in(x, y, 852, 1356, 88, 76)) {
		set_backlight_percent(75);
		return;
	}
	if (point_in(x, y, 952, 1356, 88, 76)) {
		adjust_backlight_percent(10);
		return;
	}
	for (i = 0; i < POWER_ACTION_COUNT; i++) {
		if (point_in(x, y, 78, 1545 + i * 116, g_screen_width - 156, 104)) {
			enum power_action action = (enum power_action)i;
			g_power_menu_index = i;
			if (action == POWER_BACK) {
				g_ui_screen = UI_STATUS;
				g_app_page = PAGE_MONITOR;
			} else {
				g_power_action = action;
				g_ui_screen = UI_CONFIRM_ACTION;
			}
			return;
		}
	}
}

static void handle_gpu_touch(struct input_state *in, int x, int y)
{
	(void)in;

	if (point_in(x, y, 850, 574, 180, 92)) {
		request_gpu_probe();
		return;
	}
}

static bool handle_nav_touch(struct input_state *in, int x, int y)
{
	int w = g_screen_width / PAGE_COUNT;
	int idx;

	if (y < g_screen_height - NAV_HEIGHT)
		return false;
	idx = x / w;
	if (idx < 0)
		idx = 0;
	if (idx >= PAGE_COUNT)
		idx = PAGE_COUNT - 1;
	set_page(in, (enum app_page)idx);
	return true;
}

static void handle_touch(struct input_state *in, int x, int y)
{
	log_msg("touch x=%d y=%d page=%s", x, y, page_name(g_app_page));
	if (in->mode == DISPLAY_OFF) {
		set_display_mode(in, in->previous_lit_mode);
		return;
	}
	if (handle_nav_touch(in, x, y))
		return;
	switch (g_app_page) {
	case PAGE_MONITOR:
			if (point_in(x, y, 48, 260, 312, 220) ||
				point_in(x, y, 384, 260, 312, 220) ||
				point_in(x, y, 552, 1152, 450, 240) ||
				point_in(x, y, 48, 1420, g_screen_width - 96, 420))
				set_page(in, PAGE_POWER);
		else if (point_in(x, y, 720, 260, 312, 220))
			set_page(in, PAGE_LOGS);
		else if (point_in(x, y, 48, 1830, g_screen_width - 96, 390))
			set_page(in, PAGE_GPU);
		else if (point_in(x, y, 48, 510, g_screen_width - 96, 270))
			set_page(in, PAGE_WIFI);
		break;
	case PAGE_WIFI:
		handle_wifi_touch(in, x, y);
		break;
	case PAGE_POWER:
		handle_power_touch(in, x, y);
		break;
	case PAGE_GPU:
		handle_gpu_touch(in, x, y);
		break;
	case PAGE_LOGS:
		break;
	case PAGE_COUNT:
	default:
		break;
	}
}

static bool handle_drag(struct input_state *in, int start_x, int start_y, int x, int y)
{
	(void)in;
	if (g_app_page == PAGE_WIFI) {
		struct wifi_scan_entry entries[WIFI_SCAN_MAX_UNIQUE];
		int count;
		int delta_px;

		if (!wifi_point_in_list(start_x, start_y) && !wifi_point_in_list(x, y))
			return false;
		count = read_wifi_scan_entries(entries, WIFI_SCAN_MAX_UNIQUE);
		delta_px = start_y - y;
		g_wifi.scan_scroll_px += delta_px;
		clamp_wifi_scroll(count);
		snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi list %d/%d",
			count ? g_wifi.scan_scroll + 1 : 0, count);
		log_msg("wifi list drag start=%d,%d end=%d,%d px=%d scroll_px=%d count=%d",
			start_x, start_y, x, y, delta_px, g_wifi.scan_scroll_px, count);
		return true;
	}
	return false;
}

static bool update_live_drag(struct input_state *in)
{
	(void)in;
	if (g_app_page != PAGE_WIFI)
		return false;
	if (!wifi_point_in_list(in->touch_start_x, in->touch_start_y) &&
		!wifi_point_in_list(in->touch_x, in->touch_y))
		return false;
	if (!g_wifi.scan_drag_active) {
		int dy = in->touch_y - in->touch_start_y;
		if (dy < 0)
			dy = -dy;
		if (dy < 12)
			return false;
		g_wifi.scan_drag_active = true;
		g_wifi.scan_drag_last_y = in->touch_start_y;
	}
	{
		struct wifi_scan_entry entries[WIFI_SCAN_MAX_UNIQUE];
		int count = read_wifi_scan_entries(entries, WIFI_SCAN_MAX_UNIQUE);
		int delta_px = g_wifi.scan_drag_last_y - in->touch_y;
		g_wifi.scan_drag_last_y = in->touch_y;
		g_wifi.scan_scroll_px += delta_px;
		clamp_wifi_scroll(count);
		snprintf(g_wifi.message, sizeof(g_wifi.message), "wifi list %d/%d",
			count ? g_wifi.scan_scroll + 1 : 0, count);
	}
	return true;
}

static void expire_shutdown_confirm(struct input_state *in, int64_t now)
{
	if (in->shutdown_confirm && now >= in->shutdown_deadline_ms)
		set_shutdown_confirm(in, false);
}

static void handle_key(struct input_state *in, const char *source, unsigned short code, int value)
{
	int64_t now = monotonic_ms();

	log_msg("key source=%s code=%u value=%d mode=%s", source, code, value, display_mode_name(in->mode));
	if (code != KEY_POWER && code != KEY_VOLUMEUP && code != KEY_VOLUMEDOWN)
		return;
	if (value != 0 && value != 1)
		return;
	if (g_ui_screen == UI_CONFIRM_ACTION) {
		if (value != 1)
			return;
		if (code == KEY_POWER) {
			enum power_action action = g_power_action;
			g_ui_screen = UI_STATUS;
			g_app_page = PAGE_MONITOR;
			g_power_action = POWER_BACK;
			execute_power_action(action);
		} else {
			g_ui_screen = UI_POWER_MENU;
			log_msg("power confirm cancelled");
		}
		return;
	}
	if (g_ui_screen == UI_POWER_MENU) {
		if (value != 1)
			return;
		if (code == KEY_VOLUMEUP) {
			g_power_menu_index = (g_power_menu_index + POWER_ACTION_COUNT - 1) % POWER_ACTION_COUNT;
			log_msg("power menu index=%d action=%s", g_power_menu_index,
				power_action_name((enum power_action)g_power_menu_index));
		} else if (code == KEY_VOLUMEDOWN) {
			g_power_menu_index = (g_power_menu_index + 1) % POWER_ACTION_COUNT;
			log_msg("power menu index=%d action=%s", g_power_menu_index,
				power_action_name((enum power_action)g_power_menu_index));
		} else if (code == KEY_POWER) {
			enum power_action action = (enum power_action)g_power_menu_index;
			if (action == POWER_BACK) {
				g_ui_screen = UI_STATUS;
				g_app_page = PAGE_MONITOR;
				log_msg("power menu closed");
			} else {
				g_power_action = action;
				g_ui_screen = UI_CONFIRM_ACTION;
				log_msg("power confirm action=%s", power_action_name(action));
			}
		}
		return;
	}
	if (code == KEY_POWER && value == 1) {
		if (in->mode == DISPLAY_OFF)
			set_display_mode(in, in->previous_lit_mode);
		in->power_pending = false;
		in->shutdown_confirm = false;
		g_shutdown_confirm = false;
		g_app_page = PAGE_POWER;
		g_ui_screen = UI_POWER_MENU;
		g_power_menu_index = 0;
		g_power_action = POWER_BACK;
		log_msg("power menu opened");
		return;
	}
	if (in->shutdown_confirm && code == KEY_POWER) {
		if (value == 1) {
			set_shutdown_confirm(in, false);
			request_poweroff();
		}
		return;
	}
	switch (code) {
	case KEY_POWER:
	{
		if (value == 1) {
			in->power_pressed = true;
			in->power_pressed_ms = now;
			break;
		}
		if (!in->power_pressed)
			break;
		in->power_pressed = false;
		if (now - in->power_pressed_ms >= POWER_MENU_HOLD_MS) {
			set_shutdown_confirm(in, true);
			break;
		}
		if (in->power_pending && now - in->last_power_ms <= POWER_DOUBLE_MS) {
			in->power_pending = false;
			if (in->mode == DISPLAY_LAMP)
				set_display_mode(in, DISPLAY_NORMAL);
			else
				set_display_mode(in, DISPLAY_LAMP);
		} else {
			in->power_pending = true;
			in->last_power_ms = now;
			in->power_deadline_ms = now + POWER_DOUBLE_MS;
		}
		break;
	}
	case KEY_VOLUMEUP:
		if (value == 1)
			next_lit_mode(in);
		break;
	case KEY_VOLUMEDOWN:
		if (value == 1)
			prev_lit_mode(in);
		break;
	default:
		break;
	}
}

static int map_touch_axis(int value, int min, int max, int size)
{
	long long num;
	int out;

	if (max <= min || size <= 1)
		return 0;
	if (value < min)
		value = min;
	if (value > max)
		value = max;
	num = (long long)(value - min) * (long long)(size - 1);
	out = (int)(num / (long long)(max - min));
	if (out < 0)
		out = 0;
	if (out >= size)
		out = size - 1;
	return out;
}

static void handle_touch_event(struct input_state *in, const char *source, const struct input_event *ev)
{
	(void)source;
	if (!in->touch_present)
		return;
	if (ev->type == EV_ABS) {
		if (ev->code == ABS_MT_SLOT) {
			in->touch_slot = ev->value;
			return;
		}
		if (in->touch_slot > 0)
			return;
		switch (ev->code) {
		case ABS_MT_TRACKING_ID:
			in->touch_finger_down = ev->value >= 0;
			break;
		case ABS_MT_POSITION_X:
			in->touch_x = map_touch_axis(ev->value, in->touch_min_x, in->touch_max_x, g_screen_width);
			in->touch_saw_x = true;
			in->touch_finger_down = true;
			break;
		case ABS_MT_POSITION_Y:
			in->touch_y = map_touch_axis(ev->value, in->touch_min_y, in->touch_max_y, g_screen_height);
			in->touch_saw_y = true;
			in->touch_finger_down = true;
			break;
		default:
			break;
		}
		return;
	}
	if (ev->type == EV_KEY && ev->code == BTN_TOUCH) {
		in->touch_finger_down = ev->value == 1;
		return;
	}
	if (ev->type != EV_SYN || ev->code != SYN_REPORT)
		return;
	if (in->touch_finger_down) {
		if (in->touch_saw_x && in->touch_saw_y) {
			if (!in->touch_reported) {
				in->touch_start_x = in->touch_x;
				in->touch_start_y = in->touch_y;
				in->touch_reported = true;
				g_wifi.scan_drag_active = false;
				g_wifi.scan_drag_last_y = in->touch_y;
			} else {
				update_live_drag(in);
			}
			in->touch_saw_x = false;
			in->touch_saw_y = false;
		}
		return;
	}
	if (in->touch_reported) {
		int dx = in->touch_x - in->touch_start_x;
		int dy = in->touch_y - in->touch_start_y;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		if (g_wifi.scan_drag_active) {
			log_msg("wifi list live drag end scroll_px=%d", g_wifi.scan_scroll_px);
			g_wifi.scan_drag_active = false;
		} else if (dx < 90 && dy < 90) {
			handle_touch(in, in->touch_x, in->touch_y);
		} else if (!handle_drag(in, in->touch_start_x, in->touch_start_y, in->touch_x, in->touch_y)) {
			log_msg("touch ignored as drag dx=%d dy=%d", dx, dy);
		}
	}
	in->touch_reported = false;
	in->touch_saw_x = false;
	in->touch_saw_y = false;
}

static void poll_input(struct input_state *in, int timeout_ms)
{
	struct pollfd pfds[16];
	int i, rc;
	int64_t now;

	sync_display_mode_from_file(in);
	apply_page_request(in);
	apply_wifi_scroll_request();
	reap_children();
	now = monotonic_ms();
	expire_shutdown_confirm(in, now);
	if (in->power_pending && now >= in->power_deadline_ms) {
		in->power_pending = false;
		handle_single_power(in);
	}
	if (in->count <= 0) {
		usleep((useconds_t)timeout_ms * 1000);
		return;
	}
	for (i = 0; i < in->count; i++) {
		pfds[i].fd = in->fds[i];
		pfds[i].events = POLLIN;
		pfds[i].revents = 0;
	}
	rc = poll(pfds, (nfds_t)in->count, timeout_ms);
	now = monotonic_ms();
	expire_shutdown_confirm(in, now);
	if (in->power_pending && now >= in->power_deadline_ms) {
		in->power_pending = false;
		handle_single_power(in);
	}
	if (rc <= 0)
		return;
	for (i = 0; i < in->count; i++) {
		struct input_event ev;
		if (!(pfds[i].revents & POLLIN))
			continue;
		while (read(in->fds[i], &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
			if (in->is_touch[i])
				handle_touch_event(in, in->names[i], &ev);
			else if (ev.type == EV_KEY && (ev.value == 0 || ev.value == 1))
				handle_key(in, in->names[i], ev.code, ev.value);
		}
	}
}

int main(void)
{
	struct drm_state d;
	struct canvas c;
	struct cpu_sample cpu;
	struct input_state input;
	int64_t last_cpu_update_ms = 0;
#ifdef ALIOTH_GPU_RENDERER
	struct gpu_state gpu;
	const size_t gpu_rect_cap = 120000;
	const size_t gpu_glyph_cap = 12000;
	const size_t gpu_op_cap = 140000;
	bool gpu_ready = false;
#endif

	if (!setup_drm(&d))
		return 1;

	memset(&c, 0, sizeof(c));
	c.width = d.width;
	c.height = d.height;
	c.pitch = d.pitch;
	c.data = d.map[d.front];
#ifdef ALIOTH_GPU_RENDERER
	memset(&gpu, 0, sizeof(gpu));
	c.rects = calloc(gpu_rect_cap, sizeof(*c.rects));
	c.glyphs = calloc(gpu_glyph_cap, sizeof(*c.glyphs));
	c.ops = calloc(gpu_op_cap, sizeof(*c.ops));
	c.rect_cap = c.rects ? gpu_rect_cap : 0;
	c.glyph_cap = c.glyphs ? gpu_glyph_cap : 0;
	c.op_cap = c.ops ? gpu_op_cap : 0;
	c.gpu = &gpu;
	if (c.rects && c.glyphs && c.ops &&
		gpu_init_panel(&gpu, &d, gpu_rect_cap, gpu_glyph_cap) == 0) {
		c.gpu_record = true;
		gpu_ready = true;
		log_msg("full panel GPU renderer enabled");
	} else {
		c.gpu_record = false;
		log_msg("full panel GPU renderer unavailable; using CPU renderer");
	}
#endif
	g_screen_width = c.width;
	g_screen_height = c.height;

	memset(&cpu, 0, sizeof(cpu));
	init_input(&input);
	update_cpu_sample(&cpu);
	last_cpu_update_ms = monotonic_ms();

#ifdef ALIOTH_GPU_RENDERER
	gpu_canvas_reset(&c);
#endif
	draw_status(&c, &cpu);
#ifdef ALIOTH_GPU_RENDERER
	if (gpu_ready) {
		if (gpu_render_canvas(&gpu, d.front, &c) != 0)
			return 1;
		maybe_write_screenshot(&c);
		if (!modeset_fb(&d, d.front))
			return 1;
		dirty(&d);
	} else {
		maybe_write_screenshot(&c);
		msync(d.map[d.front], d.size, MS_SYNC);
		if (!modeset_fb(&d, d.front))
			return 1;
		dirty(&d);
	}
#else
	maybe_write_screenshot(&c);
	msync(d.map[d.front], d.size, MS_SYNC);
	if (!modeset_fb(&d, d.front))
		return 1;
	dirty(&d);
#endif

	while (1) {
		int back = 1 - d.front;
		int timeout_ms = 1000;
		int64_t now_ms;
		int64_t frame_start_ms;
		int64_t after_poll_ms;
		int64_t after_draw_ms;
		int64_t after_gpu_ms;
		int64_t after_present_ms;

		frame_start_ms = monotonic_ms();
		c.data = d.map[back];
#ifdef ALIOTH_GPU_RENDERER
		gpu_canvas_reset(&c);
#endif
		poll_input(&input, timeout_ms);
		after_poll_ms = monotonic_ms();
		now_ms = after_poll_ms;
		if (!cpu.valid || now_ms - last_cpu_update_ms >= 1000) {
			update_cpu_sample(&cpu);
			last_cpu_update_ms = now_ms;
		}
		draw_status(&c, &cpu);
		after_draw_ms = monotonic_ms();
#ifdef ALIOTH_GPU_RENDERER
		if (gpu_ready) {
			if (gpu_render_canvas(&gpu, back, &c) != 0)
				return 1;
			maybe_write_screenshot(&c);
		} else {
			maybe_write_screenshot(&c);
			msync(d.map[back], d.size, MS_SYNC);
		}
#else
		maybe_write_screenshot(&c);
		msync(d.map[back], d.size, MS_SYNC);
#endif
		after_gpu_ms = monotonic_ms();
		dirty_fb(&d, back);
		if (!present_fb(&d, back))
			return 1;
		dirty(&d);
		after_present_ms = monotonic_ms();
		g_frame_draw_ms = (int)(after_draw_ms - after_poll_ms);
		g_frame_gpu_ms = (int)(after_gpu_ms - after_draw_ms);
		g_frame_present_ms = (int)(after_present_ms - after_gpu_ms);
		g_frame_total_ms = (int)(after_present_ms - frame_start_ms);
		gpu_metrics_tick();
	}
	close_input(&input);
}
