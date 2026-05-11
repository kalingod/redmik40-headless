#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <time.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <linux/dma-buf.h>
#include <vulkan/vulkan.h>

#define GLYPH_W 8u
#define GLYPH_H 8u
#define ATLAS_COLS 16u
#define ATLAS_ROWS 8u
#define TEXTURE_WIDTH (ATLAS_COLS * GLYPH_W)
#define TEXTURE_HEIGHT (ATLAS_ROWS * GLYPH_H)
#define TEXT_SCALE 4u
#define TEXT_MARGIN_X 96u
#define TEXT_MARGIN_Y 240u
#define LINE_HEIGHT (GLYPH_H * TEXT_SCALE + 18u)
#define MAX_MONITOR_LINES 6u
#define MAX_MONITOR_CHARS 32u
#define MAX_INPUT_FDS 16u
#define NORMAL_PAGE_COUNT 4u
#define POWER_MENU_PAGE 4u
#define POWER_ACTION_COUNT 4u
#define BYTES_PER_PIXEL 4u

static char monitor_lines[MAX_MONITOR_LINES][MAX_MONITOR_CHARS];
static uint32_t monitor_line_count;
static volatile sig_atomic_t stop_requested;
static bool service_mode;
static uint32_t monitor_page;
static uint32_t power_action;

struct input_ctx {
    int fds[MAX_INPUT_FDS];
    char names[MAX_INPUT_FDS][128];
    uint32_t count;
    uint32_t events_seen;
};

struct glyph_vertex {
    float pos[2];
    float uv[2];
    float color[3];
};

struct drm_target {
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
};

struct drm_buffer {
    uint32_t handle;
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint64_t size;
    void *map;
    int prime_fd;
};

struct vk_ctx {
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family;
    VkCommandPool command_pool;
    PFN_vkGetMemoryFdPropertiesKHR get_memory_fd_props;

    VkBuffer staging;
    VkDeviceMemory staging_memory;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    uint32_t vertex_count;
    VkImage texture;
    VkDeviceMemory texture_memory;
    VkImageView texture_view;
    VkSampler sampler;
    VkDescriptorSetLayout desc_layout;
    VkDescriptorPool desc_pool;
    VkDescriptorSet desc_set;

    VkRenderPass render_pass;
    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
};

struct imported_image {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkFramebuffer framebuffer;
    uint32_t width;
    uint32_t height;
    VkImageLayout layout;
    bool initialized;
};

struct flip_state {
    int waiting;
    unsigned int seen;
};

static void handle_signal(int signum)
{
    (void)signum;
    stop_requested = 1;
}

static void close_input_devices(struct input_ctx *input)
{
    for (uint32_t i = 0; i < input->count; i++) {
        if (input->fds[i] >= 0)
            close(input->fds[i]);
        input->fds[i] = -1;
    }
    input->count = 0;
}

static void open_input_devices(struct input_ctx *input)
{
    DIR *dir = opendir("/dev/input");

    memset(input, 0, sizeof(*input));
    for (uint32_t i = 0; i < MAX_INPUT_FDS; i++)
        input->fds[i] = -1;

    if (!dir) {
        printf("input_open skipped: %s\n", strerror(errno));
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && input->count < MAX_INPUT_FDS) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        char path[128];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;

        char name[128] = {0};
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
            snprintf(name, sizeof(name), "%s", ent->d_name);

        input->fds[input->count] = fd;
        snprintf(input->names[input->count], sizeof(input->names[input->count]),
                 "%s", name);
        printf("input[%u] %s %s\n", input->count, path, input->names[input->count]);
        input->count++;
    }
    closedir(dir);
    printf("input_fds=%u\n", input->count);
}

static const char *power_action_name(uint32_t action)
{
    switch (action) {
    case 1:
        return "UBUNTU DESKTOP";
    case 2:
        return "REBOOT SYSTEM";
    case 3:
        return "FASTBOOT";
    default:
        return "CANCEL";
    }
}

static void execute_power_action(void)
{
    printf("power_action_confirm action=%u name=\"%s\"\n",
           power_action, power_action_name(power_action));
    fflush(stdout);

    switch (power_action) {
    case 1:
        printf("power_action_system_rc=%d\n",
               system("systemctl start --no-block alioth-wayland-labwc-session.service"));
        break;
    case 2:
        printf("power_action_system_rc=%d\n", system("systemctl reboot"));
        break;
    case 3:
        printf("power_action_system_rc=%d\n",
               system("/var/tmp/alioth-switchroot/alioth-reboot bootloader"));
        break;
    default:
        monitor_page = 0;
        break;
    }
}

static bool handle_key_event(uint16_t code)
{
    uint32_t old_page = monitor_page;
    uint32_t old_action = power_action;

    switch (code) {
    case KEY_VOLUMEUP:
        if (monitor_page == POWER_MENU_PAGE)
            power_action = (power_action + 1u) % POWER_ACTION_COUNT;
        else
            monitor_page = (monitor_page + 1u) % NORMAL_PAGE_COUNT;
        break;
    case KEY_VOLUMEDOWN:
        if (monitor_page == POWER_MENU_PAGE)
            power_action = (power_action + POWER_ACTION_COUNT - 1u) % POWER_ACTION_COUNT;
        else
            monitor_page = (monitor_page + NORMAL_PAGE_COUNT - 1u) % NORMAL_PAGE_COUNT;
        break;
    case KEY_POWER:
        if (monitor_page == POWER_MENU_PAGE)
            execute_power_action();
        else {
            monitor_page = POWER_MENU_PAGE;
            power_action = 0;
        }
        break;
    default:
        return false;
    }

    if (monitor_page != old_page || power_action != old_action) {
        printf("input_page_change key=%u old_page=%u new_page=%u old_action=%u new_action=%u\n",
               code, old_page, monitor_page, old_action, power_action);
        return true;
    }
    printf("input_key key=%u page=%u action=%u\n", code, monitor_page, power_action);
    return false;
}

static bool poll_input_devices(struct input_ctx *input)
{
    if (input->count == 0)
        return false;

    struct pollfd fds[MAX_INPUT_FDS];
    for (uint32_t i = 0; i < input->count; i++) {
        fds[i].fd = input->fds[i];
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    int ready = poll(fds, input->count, 0);
    if (ready <= 0)
        return false;

    bool changed = false;
    for (uint32_t i = 0; i < input->count; i++) {
        if (!(fds[i].revents & POLLIN))
            continue;

        for (;;) {
            struct input_event ev;
            ssize_t n = read(input->fds[i], &ev, sizeof(ev));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                break;
            }
            if ((size_t)n != sizeof(ev))
                break;
            if (ev.type != EV_KEY || ev.value != 1)
                continue;

            input->events_seen++;
            printf("input_event fd=%u name=\"%s\" code=%u value=%d page=%u count=%u\n",
                   i, input->names[i], ev.code, ev.value, monitor_page,
                   input->events_seen);
            if (handle_key_event(ev.code))
                changed = true;
        }
    }

    return changed;
}

static int vk_ok(VkResult res, const char *expr)
{
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s failed: %d\n", expr, res);
        return -1;
    }
    return 0;
}

static int read_file(const char *path, uint32_t **words, size_t *word_count)
{
    FILE *fp = fopen(path, "rb");
    long size;
    uint32_t *buf;

    if (!fp) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    size = ftell(fp);
    if (size <= 0 || (size % 4) != 0) {
        fprintf(stderr, "invalid SPIR-V size for %s: %ld\n", path, size);
        fclose(fp);
        return -1;
    }
    rewind(fp);

    buf = malloc((size_t)size);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        fprintf(stderr, "read %s failed\n", path);
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    *words = buf;
    *word_count = (size_t)size / 4;
    return 0;
}

static uint32_t choose_crtc(drmModeRes *res, drmModeEncoder *enc)
{
    for (int i = 0; i < res->count_crtcs; i++) {
        if (enc->possible_crtcs & (1u << i))
            return res->crtcs[i];
    }
    return 0;
}

static int find_target(int fd, struct drm_target *target)
{
    drmModeRes *res = drmModeGetResources(fd);

    if (!res) {
        fprintf(stderr, "drmModeGetResources failed: %s\n", strerror(errno));
        return -1;
    }

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        uint32_t crtc_id = 0;

        if (!conn)
            continue;
        if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
            drmModeFreeConnector(conn);
            continue;
        }

        if (conn->encoder_id) {
            drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc) {
                crtc_id = enc->crtc_id ? enc->crtc_id : choose_crtc(res, enc);
                drmModeFreeEncoder(enc);
            }
        }
        if (!crtc_id) {
            for (int j = 0; j < conn->count_encoders; j++) {
                drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[j]);
                if (!enc)
                    continue;
                crtc_id = enc->crtc_id ? enc->crtc_id : choose_crtc(res, enc);
                drmModeFreeEncoder(enc);
                if (crtc_id)
                    break;
            }
        }
        if (!crtc_id && res->count_crtcs > 0)
            crtc_id = res->crtcs[0];

        if (crtc_id) {
            target->connector_id = conn->connector_id;
            target->crtc_id = crtc_id;
            target->mode = conn->modes[0];
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            return 0;
        }

        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    return -1;
}

static void dma_buf_sync_fd(int fd, uint64_t flags, const char *label)
{
    struct dma_buf_sync sync = { .flags = flags };

    if (ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync) == 0) {
        printf("dma_buf_sync_%s=ok flags=0x%llx\n",
               label, (unsigned long long)flags);
        return;
    }
    printf("dma_buf_sync_%s=skip errno=%d %s flags=0x%llx\n",
           label, errno, strerror(errno), (unsigned long long)flags);
}

static void describe_prime_fd(int fd)
{
    char proc_path[64];
    char target[256];
    ssize_t len;

    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
    len = readlink(proc_path, target, sizeof(target) - 1);
    if (len >= 0) {
        target[len] = '\0';
        printf("prime_fd=%d target=%s\n", fd, target);
    }
}

static int create_drm_buffer(int fd, const struct drm_target *target,
                             struct drm_buffer *buf)
{
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};

    memset(buf, 0, sizeof(*buf));
    buf->prime_fd = -1;

    create_req.width = target->mode.hdisplay;
    create_req.height = target->mode.vdisplay;
    create_req.bpp = 32;
    if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) != 0) {
        fprintf(stderr, "CREATE_DUMB failed: %s\n", strerror(errno));
        return -1;
    }

    buf->handle = create_req.handle;
    buf->width = create_req.width;
    buf->height = create_req.height;
    buf->pitch = create_req.pitch;
    buf->size = create_req.size;

    map_req.handle = buf->handle;
    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) != 0) {
        fprintf(stderr, "MAP_DUMB failed: %s\n", strerror(errno));
        return -1;
    }

    buf->map = mmap(NULL, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, map_req.offset);
    if (buf->map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        buf->map = NULL;
        return -1;
    }

    if (drmPrimeHandleToFD(fd, buf->handle, DRM_CLOEXEC, &buf->prime_fd) != 0) {
        fprintf(stderr, "drmPrimeHandleToFD failed: %s\n", strerror(errno));
        return -1;
    }

    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE, "cpu_zero_start");
    memset(buf->map, 0, buf->size);
    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE, "cpu_zero_end");

    uint32_t handles[4] = {buf->handle, 0, 0, 0};
    uint32_t pitches[4] = {buf->pitch, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    if (drmModeAddFB2(fd, buf->width, buf->height, DRM_FORMAT_XRGB8888,
                      handles, pitches, offsets, &buf->fb_id, 0) != 0) {
        fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(errno));
        return -1;
    }

    printf("drm_buffer handle=%u fb=%u width=%u height=%u pitch=%u size=%llu\n",
           buf->handle, buf->fb_id, buf->width, buf->height, buf->pitch,
           (unsigned long long)buf->size);
    describe_prime_fd(buf->prime_fd);
    return 0;
}

static void destroy_drm_buffer(int fd, struct drm_buffer *buf)
{
    if (buf->fb_id)
        drmModeRmFB(fd, buf->fb_id);
    if (buf->map)
        munmap(buf->map, buf->size);
    if (buf->prime_fd >= 0)
        close(buf->prime_fd);
    if (buf->handle) {
        struct drm_mode_destroy_dumb destroy_req = { .handle = buf->handle };
        ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
}

static bool has_extension(const VkExtensionProperties *exts, uint32_t count,
                          const char *name)
{
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, name) == 0)
            return true;
    }
    return false;
}

static int find_memory_type_any(VkPhysicalDevice phys, uint32_t type_bits,
                                uint32_t *type_index,
                                VkMemoryPropertyFlags *type_flags)
{
    VkPhysicalDeviceMemoryProperties props;

    vkGetPhysicalDeviceMemoryProperties(phys, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if (!(type_bits & (1u << i)))
            continue;
        *type_index = i;
        *type_flags = props.memoryTypes[i].propertyFlags;
        return 0;
    }

    return -1;
}

static int find_memory_type_flags(VkPhysicalDevice phys, uint32_t type_bits,
                                  VkMemoryPropertyFlags required,
                                  VkMemoryPropertyFlags preferred,
                                  uint32_t *type_index,
                                  VkMemoryPropertyFlags *type_flags)
{
    VkPhysicalDeviceMemoryProperties props;
    int fallback = -1;

    vkGetPhysicalDeviceMemoryProperties(phys, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags = props.memoryTypes[i].propertyFlags;

        if (!(type_bits & (1u << i)))
            continue;
        if ((flags & required) != required)
            continue;
        if ((flags & preferred) == preferred) {
            *type_index = i;
            *type_flags = flags;
            return 0;
        }
        if (fallback < 0)
            fallback = (int)i;
    }
    if (fallback >= 0) {
        *type_index = (uint32_t)fallback;
        *type_flags = props.memoryTypes[fallback].propertyFlags;
        return 0;
    }
    return -1;
}

static int init_vulkan(struct vk_ctx *ctx)
{
    const char *required_exts[] = {
        "VK_KHR_external_memory",
        "VK_KHR_external_memory_fd",
        "VK_KHR_dedicated_allocation",
        "VK_KHR_bind_memory2",
        "VK_EXT_external_memory_dma_buf",
    };
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t ext_count = 0;
    VkExtensionProperties *exts = NULL;

    memset(ctx, 0, sizeof(*ctx));
    ctx->queue_family = UINT32_MAX;

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-drm-prime-vulkan-textured-pageflip",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    if (vk_ok(vkCreateInstance(&instance_info, NULL, &ctx->instance),
              "vkCreateInstance") != 0)
        return -1;

    if (vk_ok(vkEnumeratePhysicalDevices(ctx->instance, &phys_count, NULL),
              "vkEnumeratePhysicalDevices(count)") != 0)
        return -1;
    if (phys_count == 0) {
        fprintf(stderr, "no physical devices\n");
        return -1;
    }

    VkPhysicalDevice *phys_list = calloc(phys_count, sizeof(*phys_list));
    if (!phys_list)
        return -1;
    int rc = vk_ok(vkEnumeratePhysicalDevices(ctx->instance, &phys_count, phys_list),
                   "vkEnumeratePhysicalDevices(list)");
    if (rc == 0)
        ctx->phys = phys_list[0];
    free(phys_list);
    if (rc != 0)
        return -1;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ctx->phys, &props);
    printf("physical_device=%s api=%u.%u.%u driver=%u\n",
           props.deviceName,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion),
           props.driverVersion);

    vkGetPhysicalDeviceQueueFamilyProperties(ctx->phys, &queue_count, NULL);
    VkQueueFamilyProperties *queues = calloc(queue_count, sizeof(*queues));
    if (!queues)
        return -1;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->phys, &queue_count, queues);
    for (uint32_t i = 0; i < queue_count; i++) {
        printf("queue_family[%u] flags=0x%x count=%u\n",
               i, queues[i].queueFlags, queues[i].queueCount);
        if (ctx->queue_family == UINT32_MAX &&
            queues[i].queueCount > 0 &&
            (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            ctx->queue_family = i;
    }
    free(queues);
    if (ctx->queue_family == UINT32_MAX) {
        fprintf(stderr, "no graphics-capable queue family\n");
        return -1;
    }

    if (vk_ok(vkEnumerateDeviceExtensionProperties(ctx->phys, NULL, &ext_count, NULL),
              "vkEnumerateDeviceExtensionProperties(count)") != 0)
        return -1;
    exts = calloc(ext_count ? ext_count : 1, sizeof(*exts));
    if (!exts)
        return -1;
    rc = vk_ok(vkEnumerateDeviceExtensionProperties(ctx->phys, NULL, &ext_count, exts),
               "vkEnumerateDeviceExtensionProperties(list)");
    if (rc != 0) {
        free(exts);
        return -1;
    }
    for (uint32_t i = 0; i < sizeof(required_exts) / sizeof(required_exts[0]); i++) {
        bool present = has_extension(exts, ext_count, required_exts[i]);
        printf("required_ext %-32s %s\n", required_exts[i], present ? "yes" : "no");
        if (!present) {
            free(exts);
            return -1;
        }
    }
    free(exts);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->queue_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = sizeof(required_exts) / sizeof(required_exts[0]),
        .ppEnabledExtensionNames = required_exts,
    };
    if (vk_ok(vkCreateDevice(ctx->phys, &device_info, NULL, &ctx->device),
              "vkCreateDevice") != 0)
        return -1;
    vkGetDeviceQueue(ctx->device, ctx->queue_family, 0, &ctx->queue);

    ctx->get_memory_fd_props =
        (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(ctx->device,
                                                            "vkGetMemoryFdPropertiesKHR");
    if (!ctx->get_memory_fd_props) {
        fprintf(stderr, "missing vkGetMemoryFdPropertiesKHR\n");
        return -1;
    }

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = ctx->queue_family,
    };
    if (vk_ok(vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->command_pool),
              "vkCreateCommandPool") != 0)
        return -1;

    return 0;
}

static int create_buffer(struct vk_ctx *ctx, VkDeviceSize size,
                         VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags required,
                         VkMemoryPropertyFlags preferred,
                         VkBuffer *buffer, VkDeviceMemory *memory,
                         VkMemoryPropertyFlags *memory_flags)
{
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (vk_ok(vkCreateBuffer(ctx->device, &buffer_info, NULL, buffer),
              "vkCreateBuffer") != 0)
        return -1;

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx->device, *buffer, &req);
    uint32_t memory_type = UINT32_MAX;
    if (find_memory_type_flags(ctx->phys, req.memoryTypeBits, required, preferred,
                               &memory_type, memory_flags) != 0) {
        fprintf(stderr, "no memory type for buffer type_bits=0x%x\n", req.memoryTypeBits);
        return -1;
    }

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memory_type,
    };
    if (vk_ok(vkAllocateMemory(ctx->device, &alloc, NULL, memory),
              "vkAllocateMemory(buffer)") != 0)
        return -1;
    if (vk_ok(vkBindBufferMemory(ctx->device, *buffer, *memory, 0),
              "vkBindBufferMemory") != 0)
        return -1;

    printf("buffer size=%llu req_size=%llu type=%u flags=0x%x usage=0x%x\n",
           (unsigned long long)size,
           (unsigned long long)req.size,
           memory_type,
           *memory_flags,
           usage);
    return 0;
}

static int create_optimal_image(struct vk_ctx *ctx, uint32_t width, uint32_t height,
                                VkFormat format, VkImageUsageFlags usage,
                                VkImage *image, VkDeviceMemory *memory,
                                VkMemoryPropertyFlags *memory_flags)
{
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { .width = width, .height = height, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (vk_ok(vkCreateImage(ctx->device, &image_info, NULL, image),
              "vkCreateImage(optimal)") != 0)
        return -1;

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx->device, *image, &req);
    uint32_t memory_type = UINT32_MAX;
    if (find_memory_type_flags(ctx->phys, req.memoryTypeBits, 0,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               &memory_type, memory_flags) != 0) {
        fprintf(stderr, "no memory type for image type_bits=0x%x\n", req.memoryTypeBits);
        return -1;
    }

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memory_type,
    };
    if (vk_ok(vkAllocateMemory(ctx->device, &alloc, NULL, memory),
              "vkAllocateMemory(image)") != 0)
        return -1;
    if (vk_ok(vkBindImageMemory(ctx->device, *image, *memory, 0),
              "vkBindImageMemory") != 0)
        return -1;

    printf("optimal_image %ux%u usage=0x%x req_size=%llu type=%u flags=0x%x\n",
           width, height, usage, (unsigned long long)req.size,
           memory_type, *memory_flags);
    return 0;
}

static void trim_newline(char *s)
{
    size_t len = strlen(s);

    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                       s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static void sanitize_upper(char *s)
{
    for (size_t i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];

        if (c >= 'a' && c <= 'z') {
            s[i] = (char)toupper(c);
        } else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                   c == ' ' || c == '.' || c == '-' || c == ':' ||
                   c == '/' || c == '_') {
            continue;
        } else {
            s[i] = ' ';
        }
    }
}

static void read_os_pretty(char *out, size_t out_size)
{
    FILE *fp = fopen("/etc/os-release", "r");

    snprintf(out, out_size, "UBUNTU");
    if (!fp)
        return;

    char line[192];
    while (fgets(line, sizeof(line), fp)) {
        const char key[] = "PRETTY_NAME=";
        size_t key_len = sizeof(key) - 1;

        if (strncmp(line, key, key_len) != 0)
            continue;
        char *value = line + key_len;
        trim_newline(value);
        if (value[0] == '"') {
            value++;
            char *end = strrchr(value, '"');
            if (end)
                *end = '\0';
        }
        snprintf(out, out_size, "%s", value);
        break;
    }
    fclose(fp);
}

static void read_systemd_state(char *out, size_t out_size)
{
    FILE *fp = popen("systemctl is-system-running 2>/dev/null", "r");

    snprintf(out, out_size, "UNKNOWN");
    if (!fp)
        return;
    if (fgets(out, out_size, fp))
        trim_newline(out);
    pclose(fp);
}

static bool read_first_line(const char *path, char *out, size_t out_size)
{
    FILE *fp = fopen(path, "r");

    if (!fp)
        return false;
    if (!fgets(out, out_size, fp)) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    trim_newline(out);
    return out[0] != '\0';
}

static bool read_key_value(const char *path, const char *key,
                           char *out, size_t out_size)
{
    FILE *fp = fopen(path, "r");
    size_t key_len = strlen(key);
    char line[160];

    if (!fp)
        return false;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, key_len) != 0)
            continue;
        snprintf(out, out_size, "%s", line + key_len);
        trim_newline(out);
        fclose(fp);
        return out[0] != '\0';
    }
    fclose(fp);
    return false;
}

static void read_failed_units(char *out, size_t out_size)
{
    FILE *fp = popen("systemctl --failed --no-legend 2>/dev/null | wc -l", "r");

    snprintf(out, out_size, "?");
    if (!fp)
        return;
    if (fgets(out, out_size, fp))
        trim_newline(out);
    pclose(fp);
}

static void read_battery_line(char *out, size_t out_size)
{
    char capacity[16];
    char status[32];

    if (!read_first_line("/sys/class/power_supply/battery/capacity",
                         capacity, sizeof(capacity)))
        snprintf(capacity, sizeof(capacity), "?");
    if (!read_first_line("/sys/class/power_supply/battery/status",
                         status, sizeof(status)))
        snprintf(status, sizeof(status), "UNKNOWN");
    snprintf(out, out_size, "BAT %s %s", capacity, status);
}

static bool read_wireless_quality(const char *ifname, int *quality_out)
{
    FILE *fp = fopen("/proc/net/wireless", "r");
    char line[160];
    int quality;

    if (!fp)
        return false;

    while (fgets(line, sizeof(line), fp)) {
        char name[48];
        if (sscanf(line, " %47[^:]:", name) != 1)
            continue;
        if (strcmp(name, ifname) != 0)
            continue;
        char *colon = strchr(line, ':');
        if (!colon)
            continue;
        if (sscanf(colon + 1, " %*d %d %*d %*d", &quality) != 1)
            continue;
        if (quality_out)
            *quality_out = quality;
        fclose(fp);
        return true;
    }

    fclose(fp);
    return false;
}

static void read_net_line(const char *ifname, const char *label,
                          char *out, size_t out_size)
{
    char path[96];
    char carrier_path[128];
    char state[32];
    int quality = -1;

    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ifname);
    if (!read_first_line(path, state, sizeof(state))) {
        snprintf(out, out_size, "%s MISSING", label);
        return;
    }
    if (strcmp(ifname, "wlan0") == 0) {
        bool has_wireless = false;

        has_wireless = read_wireless_quality(ifname, &quality);
        snprintf(carrier_path, sizeof(carrier_path),
                 "/sys/class/net/%s/carrier", ifname);
        if (strcmp(state, "up") == 0) {
            if (read_first_line(carrier_path, state, sizeof(state)) &&
                strcmp(state, "1") == 0) {
                snprintf(out, out_size, "%s LINK", label);
                return;
            }
            if (has_wireless) {
                snprintf(out, out_size, "%s SCAN READY", label);
            } else {
                snprintf(out, out_size, "%s UP", label);
            }
            return;
        }
        snprintf(out, out_size, "%s %s", label, has_wireless ? "SCAN READY" : "DOWN");
        return;
    }
    snprintf(out, out_size, "%s %s", label, state);
}

static void read_audio_line(char *out, size_t out_size)
{
    char adsp[32];
    char nodes[16];

    if (!read_key_value("/run/alioth-audio-state", "adsp_state=",
                        adsp, sizeof(adsp))) {
        snprintf(out, out_size, "AUDIO UNKNOWN");
        return;
    }
    if (read_key_value("/run/alioth-audio-state", "sound_nodes=",
                       nodes, sizeof(nodes)))
        snprintf(out, out_size, "AUDIO %s SND %s", adsp, nodes);
    else
        snprintf(out, out_size, "AUDIO ADSP %s", adsp);
}

static void set_monitor_line(uint32_t index, const char *prefix, const char *value)
{
    if (index >= MAX_MONITOR_LINES)
        return;
    snprintf(monitor_lines[index], MAX_MONITOR_CHARS, "%s%s", prefix, value);
    sanitize_upper(monitor_lines[index]);
}

static bool prepare_monitor_lines(void)
{
    char old_lines[MAX_MONITOR_LINES][MAX_MONITOR_CHARS];
    uint32_t old_count = monitor_line_count;
    char os[96];
    char systemd[48];
    char failed[16];
    char line[96];
    struct utsname uts;

    memcpy(old_lines, monitor_lines, sizeof(old_lines));
    monitor_line_count = 6;

    switch (monitor_page) {
    case 1:
        set_monitor_line(0, "", "HARDWARE STACK");
        set_monitor_line(1, "", "FD650 ADRENO KGSL");
        set_monitor_line(2, "", "DRM KMS PAGEFLIP");
        set_monitor_line(3, "", "TOUCH FTS EVDEV");
        set_monitor_line(4, "", "AUDIO CS35L41 SPK");
        set_monitor_line(5, "", "WIFI CNSS SCAN OK");
        break;
    case 2:
        set_monitor_line(0, "", "LAUNCHER");
        set_monitor_line(1, "", "POWER OPENS MENU");
        set_monitor_line(2, "", "VOL KEYS SWITCH PAGE");
        set_monitor_line(3, "", "DESKTOP LABWC READY");
        set_monitor_line(4, "", "SSH 172.16.42.2");
        set_monitor_line(5, "", "REBOOT FASTBOOT SAFE");
        break;
    case POWER_MENU_PAGE:
        set_monitor_line(0, "", "POWER MENU");
        set_monitor_line(1, "ACTION ", power_action_name(power_action));
        set_monitor_line(2, "", "VOL UP CHOOSE");
        set_monitor_line(3, "", "POWER CONFIRM");
        set_monitor_line(4, "", "CANCEL DEFAULT SAFE");
        set_monitor_line(5, "", "DESKTOP REBOOT FASTBOOT");
        break;
    default:
        set_monitor_line(0, "", "ALIOTH RUNTIME");
        read_systemd_state(systemd, sizeof(systemd));
        read_failed_units(failed, sizeof(failed));
        snprintf(line, sizeof(line), "SYSTEMD %s FAIL %s", systemd, failed);
        set_monitor_line(1, "", line);
        read_battery_line(line, sizeof(line));
        set_monitor_line(2, "", line);
        read_net_line("usb0", "USB0", line, sizeof(line));
        set_monitor_line(3, "", line);
        read_net_line("wlan0", "WIFI", line, sizeof(line));
        set_monitor_line(4, "", line);
        read_audio_line(line, sizeof(line));
        set_monitor_line(5, "", line);
        break;
    case 3:
        set_monitor_line(0, "", "SYSTEM DETAILS");
        read_os_pretty(os, sizeof(os));
        set_monitor_line(1, "", os);
        if (uname(&uts) == 0)
            set_monitor_line(2, "KERNEL ", uts.release);
        else
            set_monitor_line(2, "KERNEL ", "UNKNOWN");
        read_systemd_state(systemd, sizeof(systemd));
        set_monitor_line(3, "SYSTEMD ", systemd);
        set_monitor_line(4, "", "FD650 KGSL VULKAN OK");
        set_monitor_line(5, "", "DRM KMS PAGEFLIP OK");
        break;
    }

    bool changed = old_count != monitor_line_count ||
                   memcmp(old_lines, monitor_lines, sizeof(old_lines)) != 0;

    if (changed) {
        for (uint32_t i = 0; i < monitor_line_count; i++)
            printf("monitor_line[%u]=\"%s\"\n", i, monitor_lines[i]);
    }
    return changed;
}

static const uint8_t *glyph_rows(char c)
{
    static const uint8_t blank[GLYPH_H] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    static const uint8_t glyph_qmark[GLYPH_H] = {
        0x3c, 0x66, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x00,
    };
    static const uint8_t glyph_dot[GLYPH_H] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00,
    };
    static const uint8_t glyph_colon[GLYPH_H] = {
        0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00,
    };
    static const uint8_t glyph_dash[GLYPH_H] = {
        0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
    };
    static const uint8_t glyph_slash[GLYPH_H] = {
        0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00,
    };
    static const uint8_t glyph_0[GLYPH_H] = {
        0x3c, 0x66, 0x6e, 0x76, 0x66, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_1[GLYPH_H] = {
        0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
    };
    static const uint8_t glyph_2[GLYPH_H] = {
        0x3c, 0x66, 0x06, 0x0c, 0x18, 0x30, 0x7e, 0x00,
    };
    static const uint8_t glyph_3[GLYPH_H] = {
        0x3c, 0x66, 0x06, 0x1c, 0x06, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_4[GLYPH_H] = {
        0x0c, 0x1c, 0x3c, 0x6c, 0x7e, 0x0c, 0x0c, 0x00,
    };
    static const uint8_t glyph_5[GLYPH_H] = {
        0x7e, 0x60, 0x7c, 0x06, 0x06, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_6[GLYPH_H] = {
        0x1c, 0x30, 0x60, 0x7c, 0x66, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_7[GLYPH_H] = {
        0x7e, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00,
    };
    static const uint8_t glyph_8[GLYPH_H] = {
        0x3c, 0x66, 0x66, 0x3c, 0x66, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_9[GLYPH_H] = {
        0x3c, 0x66, 0x66, 0x3e, 0x06, 0x0c, 0x38, 0x00,
    };
    static const uint8_t glyph_a[GLYPH_H] = {
        0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00,
    };
    static const uint8_t glyph_b[GLYPH_H] = {
        0x7c, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x7c, 0x00,
    };
    static const uint8_t glyph_c[GLYPH_H] = {
        0x3c, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_d[GLYPH_H] = {
        0x78, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00,
    };
    static const uint8_t glyph_e[GLYPH_H] = {
        0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x7e, 0x00,
    };
    static const uint8_t glyph_f[GLYPH_H] = {
        0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x00,
    };
    static const uint8_t glyph_g[GLYPH_H] = {
        0x3c, 0x66, 0x60, 0x6e, 0x66, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_h[GLYPH_H] = {
        0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00,
    };
    static const uint8_t glyph_i[GLYPH_H] = {
        0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
    };
    static const uint8_t glyph_j[GLYPH_H] = {
        0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x6c, 0x38, 0x00,
    };
    static const uint8_t glyph_k[GLYPH_H] = {
        0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00,
    };
    static const uint8_t glyph_l[GLYPH_H] = {
        0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00,
    };
    static const uint8_t glyph_m[GLYPH_H] = {
        0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00,
    };
    static const uint8_t glyph_n[GLYPH_H] = {
        0x66, 0x76, 0x7e, 0x7e, 0x6e, 0x66, 0x66, 0x00,
    };
    static const uint8_t glyph_o[GLYPH_H] = {
        0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_p[GLYPH_H] = {
        0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x00,
    };
    static const uint8_t glyph_q[GLYPH_H] = {
        0x3c, 0x66, 0x66, 0x66, 0x6e, 0x3c, 0x0e, 0x00,
    };
    static const uint8_t glyph_r[GLYPH_H] = {
        0x7c, 0x66, 0x66, 0x7c, 0x78, 0x6c, 0x66, 0x00,
    };
    static const uint8_t glyph_s[GLYPH_H] = {
        0x3c, 0x66, 0x60, 0x3c, 0x06, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_t[GLYPH_H] = {
        0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,
    };
    static const uint8_t glyph_u[GLYPH_H] = {
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00,
    };
    static const uint8_t glyph_v[GLYPH_H] = {
        0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00,
    };
    static const uint8_t glyph_w[GLYPH_H] = {
        0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00,
    };
    static const uint8_t glyph_x[GLYPH_H] = {
        0x66, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x66, 0x00,
    };
    static const uint8_t glyph_y[GLYPH_H] = {
        0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x00,
    };
    static const uint8_t glyph_z[GLYPH_H] = {
        0x7e, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x7e, 0x00,
    };

    switch (c) {
    case '?':
        return glyph_qmark;
    case '.':
        return glyph_dot;
    case ':':
        return glyph_colon;
    case '-':
    case '_':
        return glyph_dash;
    case '/':
        return glyph_slash;
    case '0':
        return glyph_0;
    case '1':
        return glyph_1;
    case '2':
        return glyph_2;
    case '3':
        return glyph_3;
    case '4':
        return glyph_4;
    case '5':
        return glyph_5;
    case '6':
        return glyph_6;
    case '7':
        return glyph_7;
    case '8':
        return glyph_8;
    case '9':
        return glyph_9;
    case 'A':
        return glyph_a;
    case 'B':
        return glyph_b;
    case 'C':
        return glyph_c;
    case 'D':
        return glyph_d;
    case 'E':
        return glyph_e;
    case 'F':
        return glyph_f;
    case 'G':
        return glyph_g;
    case 'H':
        return glyph_h;
    case 'I':
        return glyph_i;
    case 'J':
        return glyph_j;
    case 'K':
        return glyph_k;
    case 'L':
        return glyph_l;
    case 'M':
        return glyph_m;
    case 'N':
        return glyph_n;
    case 'O':
        return glyph_o;
    case 'P':
        return glyph_p;
    case 'Q':
        return glyph_q;
    case 'R':
        return glyph_r;
    case 'S':
        return glyph_s;
    case 'T':
        return glyph_t;
    case 'U':
        return glyph_u;
    case 'V':
        return glyph_v;
    case 'W':
        return glyph_w;
    case 'X':
        return glyph_x;
    case 'Y':
        return glyph_y;
    case 'Z':
        return glyph_z;
    default:
        return blank;
    }
}

static void fill_glyph_atlas(uint8_t *dst)
{
    memset(dst, 0, TEXTURE_WIDTH * TEXTURE_HEIGHT * BYTES_PER_PIXEL);
    for (uint32_t ch = 0; ch < 128; ch++) {
        const uint8_t *rows = glyph_rows((char)ch);
        uint32_t cell_x = (ch % ATLAS_COLS) * GLYPH_W;
        uint32_t cell_y = (ch / ATLAS_COLS) * GLYPH_H;

        for (uint32_t y = 0; y < GLYPH_H; y++) {
            for (uint32_t x = 0; x < GLYPH_W; x++) {
                bool on = (rows[y] & (1u << (7u - x))) != 0;
                uint8_t *px = dst + ((size_t)(cell_y + y) * TEXTURE_WIDTH +
                                      (cell_x + x)) * BYTES_PER_PIXEL;

                px[0] = 0xff;
                px[1] = 0xff;
                px[2] = 0xff;
                px[3] = on ? 0xff : 0x00;
            }
        }
    }
}

static float ndc_x(uint32_t x, uint32_t width)
{
    return ((float)x * 2.0f / (float)width) - 1.0f;
}

static float ndc_y(uint32_t y, uint32_t height)
{
    return ((float)y * 2.0f / (float)height) - 1.0f;
}

static void line_color(uint32_t line, const char *text, float color[3])
{
    color[0] = 0.92f;
    color[1] = 0.94f;
    color[2] = 0.90f;

    if (line == 0) {
        color[0] = 1.00f;
        color[1] = 1.00f;
        color[2] = 1.00f;
    } else if (line == 3 && strstr(text, "RUNNING")) {
        color[0] = 0.36f;
        color[1] = 0.92f;
        color[2] = 0.54f;
    } else if (line == 3) {
        color[0] = 0.95f;
        color[1] = 0.72f;
        color[2] = 0.28f;
    } else if (line == 4) {
        color[0] = 0.42f;
        color[1] = 0.84f;
        color[2] = 1.00f;
    } else if (line == 5) {
        color[0] = 0.72f;
        color[1] = 0.78f;
        color[2] = 0.86f;
    }
}

static void set_vertex(struct glyph_vertex *v, uint32_t x, uint32_t y,
                       uint32_t width, uint32_t height, float u, float vv,
                       const float color[3])
{
    v->pos[0] = ndc_x(x, width);
    v->pos[1] = ndc_y(y, height);
    v->uv[0] = u;
    v->uv[1] = vv;
    v->color[0] = color[0];
    v->color[1] = color[1];
    v->color[2] = color[2];
}

static int create_text_vertices(struct vk_ctx *ctx, uint32_t width, uint32_t height)
{
    uint32_t glyph_count = 0;
    const uint32_t glyph_w = GLYPH_W * TEXT_SCALE;
    const uint32_t glyph_h = GLYPH_H * TEXT_SCALE;
    const uint32_t advance = glyph_w + TEXT_SCALE;

    for (uint32_t line = 0; line < monitor_line_count; line++) {
        for (size_t i = 0; monitor_lines[line][i]; i++) {
            if (monitor_lines[line][i] != ' ')
                glyph_count++;
        }
    }
    ctx->vertex_count = glyph_count * 6;
    VkDeviceSize vertex_size = (VkDeviceSize)ctx->vertex_count * sizeof(struct glyph_vertex);
    VkMemoryPropertyFlags vertex_flags = 0;

    if (create_buffer(ctx, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      0, &ctx->vertex_buffer, &ctx->vertex_memory,
                      &vertex_flags) != 0)
        return -1;

    struct glyph_vertex *vertices = NULL;
    if (vk_ok(vkMapMemory(ctx->device, ctx->vertex_memory, 0, vertex_size, 0,
                          (void **)&vertices),
              "vkMapMemory(vertices)") != 0)
        return -1;

    uint32_t out = 0;
    for (uint32_t line = 0; line < monitor_line_count; line++) {
        uint32_t cursor_x = TEXT_MARGIN_X;
        uint32_t y0 = TEXT_MARGIN_Y + line * LINE_HEIGHT;
        float color[3];

        line_color(line, monitor_lines[line], color);

        for (size_t i = 0; monitor_lines[line][i]; i++) {
            unsigned char ch = (unsigned char)monitor_lines[line][i];

            if (cursor_x + glyph_w >= width)
                break;
            if (ch == ' ') {
                cursor_x += advance;
                continue;
            }
            if (ch >= 128)
                ch = '?';

            uint32_t cell_x = (ch % ATLAS_COLS) * GLYPH_W;
            uint32_t cell_y = (ch / ATLAS_COLS) * GLYPH_H;
            float u0 = (float)cell_x / (float)TEXTURE_WIDTH;
            float v0 = (float)cell_y / (float)TEXTURE_HEIGHT;
            float u1 = (float)(cell_x + GLYPH_W) / (float)TEXTURE_WIDTH;
            float v1 = (float)(cell_y + GLYPH_H) / (float)TEXTURE_HEIGHT;
            uint32_t x0 = cursor_x;
            uint32_t x1 = x0 + glyph_w;
            uint32_t y1 = y0 + glyph_h;

            set_vertex(&vertices[out++], x0, y0, width, height, u0, v0, color);
            set_vertex(&vertices[out++], x1, y0, width, height, u1, v0, color);
            set_vertex(&vertices[out++], x0, y1, width, height, u0, v1, color);
            set_vertex(&vertices[out++], x1, y0, width, height, u1, v0, color);
            set_vertex(&vertices[out++], x1, y1, width, height, u1, v1, color);
            set_vertex(&vertices[out++], x0, y1, width, height, u0, v1, color);
            cursor_x += advance;
        }
    }

    vkUnmapMemory(ctx->device, ctx->vertex_memory);
    printf("glyph_vertices lines=%u glyphs=%u vertices=%u size=%llu margin=%ux%u scale=%u\n",
           monitor_line_count, glyph_count, ctx->vertex_count,
           (unsigned long long)vertex_size,
           TEXT_MARGIN_X, TEXT_MARGIN_Y, TEXT_SCALE);
    return out == ctx->vertex_count ? 0 : -1;
}

static void destroy_text_vertices(struct vk_ctx *ctx)
{
    if (!ctx->device)
        return;
    if (ctx->vertex_buffer) {
        vkDestroyBuffer(ctx->device, ctx->vertex_buffer, NULL);
        ctx->vertex_buffer = VK_NULL_HANDLE;
    }
    if (ctx->vertex_memory) {
        vkFreeMemory(ctx->device, ctx->vertex_memory, NULL);
        ctx->vertex_memory = VK_NULL_HANDLE;
    }
    ctx->vertex_count = 0;
}

static int refresh_text_vertices_if_needed(struct vk_ctx *ctx,
                                           uint32_t width, uint32_t height,
                                           unsigned int frame)
{
    if (!prepare_monitor_lines())
        return 0;

    if (vk_ok(vkDeviceWaitIdle(ctx->device), "vkDeviceWaitIdle(refresh)") != 0)
        return -1;
    destroy_text_vertices(ctx);
    if (create_text_vertices(ctx, width, height) != 0)
        return -1;
    printf("monitor_vertices_refreshed frame=%u vertices=%u\n",
           frame, ctx->vertex_count);
    return 0;
}

static int upload_texture(struct vk_ctx *ctx)
{
    const VkDeviceSize texture_size = TEXTURE_WIDTH * TEXTURE_HEIGHT * BYTES_PER_PIXEL;
    VkMemoryPropertyFlags staging_flags = 0;
    VkMemoryPropertyFlags texture_flags = 0;
    void *mapped = NULL;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    int rc = -1;

    if (create_buffer(ctx, texture_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      0, &ctx->staging, &ctx->staging_memory,
                      &staging_flags) != 0)
        return -1;
    if (vk_ok(vkMapMemory(ctx->device, ctx->staging_memory, 0, texture_size, 0, &mapped),
              "vkMapMemory(staging)") != 0)
        return -1;
    fill_glyph_atlas(mapped);
    vkUnmapMemory(ctx->device, ctx->staging_memory);
    mapped = NULL;
    printf("glyph_atlas texture=%ux%u glyph=%ux%u bytes=%llu\n",
           TEXTURE_WIDTH, TEXTURE_HEIGHT, GLYPH_W, GLYPH_H,
           (unsigned long long)texture_size);

    if (create_optimal_image(ctx, TEXTURE_WIDTH, TEXTURE_HEIGHT,
                             VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT,
                             &ctx->texture, &ctx->texture_memory,
                             &texture_flags) != 0)
        return -1;

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx->texture,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    if (vk_ok(vkCreateImageView(ctx->device, &view_info, NULL, &ctx->texture_view),
              "vkCreateImageView(texture)") != 0)
        return -1;

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .compareEnable = VK_FALSE,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    if (vk_ok(vkCreateSampler(ctx->device, &sampler_info, NULL, &ctx->sampler),
              "vkCreateSampler") != 0)
        return -1;

    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vk_ok(vkAllocateCommandBuffers(ctx->device, &cmd_alloc, &command_buffer),
              "vkAllocateCommandBuffers(texture)") != 0)
        return -1;

    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (vk_ok(vkBeginCommandBuffer(command_buffer, &begin),
              "vkBeginCommandBuffer(texture)") != 0)
        goto cleanup;

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageMemoryBarrier to_dst = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx->texture,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_dst);

    VkBufferImageCopy copy = {
        .bufferOffset = 0,
        .bufferRowLength = TEXTURE_WIDTH,
        .bufferImageHeight = TEXTURE_HEIGHT,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { .x = 0, .y = 0, .z = 0 },
        .imageExtent = { .width = TEXTURE_WIDTH, .height = TEXTURE_HEIGHT, .depth = 1 },
    };
    vkCmdCopyBufferToImage(command_buffer, ctx->staging, ctx->texture,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier to_sample = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx->texture,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_sample);

    if (vk_ok(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(texture)") != 0)
        goto cleanup;

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    if (vk_ok(vkQueueSubmit(ctx->queue, 1, &submit, VK_NULL_HANDLE),
              "vkQueueSubmit(texture)") != 0)
        goto cleanup;
    if (vk_ok(vkQueueWaitIdle(ctx->queue), "vkQueueWaitIdle(texture)") != 0)
        goto cleanup;

    printf("texture_upload=PASS\n");
    rc = 0;

cleanup:
    if (command_buffer)
        vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &command_buffer);
    return rc;
}

static int create_descriptor_set(struct vk_ctx *ctx)
{
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutCreateInfo desc_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_binding,
    };
    if (vk_ok(vkCreateDescriptorSetLayout(ctx->device, &desc_layout_info, NULL,
                                          &ctx->desc_layout),
              "vkCreateDescriptorSetLayout") != 0)
        return -1;

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    if (vk_ok(vkCreateDescriptorPool(ctx->device, &desc_pool_info, NULL,
                                     &ctx->desc_pool),
              "vkCreateDescriptorPool") != 0)
        return -1;

    VkDescriptorSetAllocateInfo desc_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx->desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ctx->desc_layout,
    };
    if (vk_ok(vkAllocateDescriptorSets(ctx->device, &desc_alloc, &ctx->desc_set),
              "vkAllocateDescriptorSets") != 0)
        return -1;

    VkDescriptorImageInfo image_info = {
        .sampler = ctx->sampler,
        .imageView = ctx->texture_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ctx->desc_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
    };
    vkUpdateDescriptorSets(ctx->device, 1, &write, 0, NULL);
    printf("descriptor_set=PASS\n");
    return 0;
}

static int create_pipeline(struct vk_ctx *ctx, uint32_t width, uint32_t height,
                           const char *vert_path, const char *frag_path)
{
    uint32_t *vert_spv = NULL;
    uint32_t *frag_spv = NULL;
    size_t vert_words = 0;
    size_t frag_words = 0;
    int rc = -1;

    if (read_file(vert_path, &vert_spv, &vert_words) != 0 ||
        read_file(frag_path, &frag_spv, &frag_words) != 0)
        goto cleanup;
    printf("shader_words vert=%zu frag=%zu\n", vert_words, frag_words);

    VkAttachmentDescription attachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };
    VkSubpassDependency deps[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = deps,
    };
    if (vk_ok(vkCreateRenderPass(ctx->device, &render_pass_info, NULL, &ctx->render_pass),
              "vkCreateRenderPass") != 0)
        goto cleanup;

    VkShaderModuleCreateInfo vert_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_words * sizeof(uint32_t),
        .pCode = vert_spv,
    };
    VkShaderModuleCreateInfo frag_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_words * sizeof(uint32_t),
        .pCode = frag_spv,
    };
    if (vk_ok(vkCreateShaderModule(ctx->device, &vert_info, NULL, &ctx->vert_shader),
              "vkCreateShaderModule(vert)") != 0)
        goto cleanup;
    if (vk_ok(vkCreateShaderModule(ctx->device, &frag_info, NULL, &ctx->frag_shader),
              "vkCreateShaderModule(frag)") != 0)
        goto cleanup;

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = ctx->vert_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = ctx->frag_shader,
            .pName = "main",
        },
    };
    VkVertexInputBindingDescription vertex_binding = {
        .binding = 0,
        .stride = sizeof(struct glyph_vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription vertex_attrs[3] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(struct glyph_vertex, pos),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(struct glyph_vertex, uv),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(struct glyph_vertex, color),
        },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = vertex_attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width = width, .height = height },
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->desc_layout,
    };
    if (vk_ok(vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL,
                                     &ctx->pipeline_layout),
              "vkCreatePipelineLayout") != 0)
        goto cleanup;

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pDepthStencilState = NULL,
        .pColorBlendState = &blend,
        .layout = ctx->pipeline_layout,
        .renderPass = ctx->render_pass,
        .subpass = 0,
    };
    if (vk_ok(vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, NULL, &ctx->pipeline),
              "vkCreateGraphicsPipelines") != 0)
        goto cleanup;

    rc = 0;

cleanup:
    free(vert_spv);
    free(frag_spv);
    return rc;
}

static void destroy_vulkan(struct vk_ctx *ctx)
{
    if (ctx->device) {
        if (ctx->pipeline)
            vkDestroyPipeline(ctx->device, ctx->pipeline, NULL);
        if (ctx->pipeline_layout)
            vkDestroyPipelineLayout(ctx->device, ctx->pipeline_layout, NULL);
        if (ctx->frag_shader)
            vkDestroyShaderModule(ctx->device, ctx->frag_shader, NULL);
        if (ctx->vert_shader)
            vkDestroyShaderModule(ctx->device, ctx->vert_shader, NULL);
        if (ctx->render_pass)
            vkDestroyRenderPass(ctx->device, ctx->render_pass, NULL);
        if (ctx->desc_pool)
            vkDestroyDescriptorPool(ctx->device, ctx->desc_pool, NULL);
        if (ctx->desc_layout)
            vkDestroyDescriptorSetLayout(ctx->device, ctx->desc_layout, NULL);
        if (ctx->sampler)
            vkDestroySampler(ctx->device, ctx->sampler, NULL);
        if (ctx->texture_view)
            vkDestroyImageView(ctx->device, ctx->texture_view, NULL);
        if (ctx->texture)
            vkDestroyImage(ctx->device, ctx->texture, NULL);
        if (ctx->texture_memory)
            vkFreeMemory(ctx->device, ctx->texture_memory, NULL);
        if (ctx->vertex_buffer)
            vkDestroyBuffer(ctx->device, ctx->vertex_buffer, NULL);
        if (ctx->vertex_memory)
            vkFreeMemory(ctx->device, ctx->vertex_memory, NULL);
        if (ctx->staging)
            vkDestroyBuffer(ctx->device, ctx->staging, NULL);
        if (ctx->staging_memory)
            vkFreeMemory(ctx->device, ctx->staging_memory, NULL);
        if (ctx->command_pool)
            vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
        vkDestroyDevice(ctx->device, NULL);
    }
    if (ctx->instance)
        vkDestroyInstance(ctx->instance, NULL);
}

static int check_external_format(struct vk_ctx *ctx, const struct drm_buffer *buf)
{
    VkPhysicalDeviceExternalImageFormatInfo ext_format = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceImageFormatInfo2 image_format = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &ext_format,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .type = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };
    VkExternalImageFormatProperties ext_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 image_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &ext_props,
    };
    VkResult res = vkGetPhysicalDeviceImageFormatProperties2(ctx->phys,
                                                             &image_format,
                                                             &image_props);
    printf("linear_b8g8r8a8_dma_buf_textured_format result=%d maxExtent=%ux%u features=0x%x compatible=0x%x\n",
           res,
           res == VK_SUCCESS ? image_props.imageFormatProperties.maxExtent.width : 0,
           res == VK_SUCCESS ? image_props.imageFormatProperties.maxExtent.height : 0,
           res == VK_SUCCESS ? ext_props.externalMemoryProperties.externalMemoryFeatures : 0,
           res == VK_SUCCESS ? ext_props.externalMemoryProperties.compatibleHandleTypes : 0);

    if (res != VK_SUCCESS)
        return -1;
    if (image_props.imageFormatProperties.maxExtent.width < buf->width ||
        image_props.imageFormatProperties.maxExtent.height < buf->height) {
        fprintf(stderr, "external image max extent is too small\n");
        return -1;
    }
    return 0;
}

static int import_buffer(struct vk_ctx *ctx, const struct drm_buffer *buf,
                         struct imported_image *out)
{
    int import_fd = -1;
    uint32_t memory_type = UINT32_MAX;
    VkMemoryPropertyFlags memory_flags = 0;

    memset(out, 0, sizeof(*out));
    out->width = buf->width;
    out->height = buf->height;
    out->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkExternalMemoryImageCreateInfo external_image = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_image,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = { .width = buf->width, .height = buf->height, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (vk_ok(vkCreateImage(ctx->device, &image_info, NULL, &out->image),
              "vkCreateImage(imported)") != 0)
        return -1;

    VkImageSubresource subresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .arrayLayer = 0,
    };
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(ctx->device, out->image, &subresource, &layout);
    printf("vulkan_layout fb=%u offset=%llu size=%llu rowPitch=%llu drm_pitch=%u drm_size=%llu\n",
           buf->fb_id,
           (unsigned long long)layout.offset,
           (unsigned long long)layout.size,
           (unsigned long long)layout.rowPitch,
           buf->pitch,
           (unsigned long long)buf->size);
    if (layout.offset != 0 || layout.rowPitch != buf->pitch || layout.size > buf->size) {
        fprintf(stderr, "linear layout does not match DRM dumb buffer\n");
        return -1;
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx->device, out->image, &req);
    VkMemoryFdPropertiesKHR fd_props = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
    };
    if (vk_ok(ctx->get_memory_fd_props(ctx->device,
                                       VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                       buf->prime_fd,
                                       &fd_props),
              "vkGetMemoryFdPropertiesKHR") != 0)
        return -1;

    uint32_t combined_bits = req.memoryTypeBits & fd_props.memoryTypeBits;
    printf("image_req fb=%u size=%llu type_bits=0x%x fd_type_bits=0x%x combined=0x%x\n",
           buf->fb_id,
           (unsigned long long)req.size,
           req.memoryTypeBits,
           fd_props.memoryTypeBits,
           combined_bits);
    if (req.size > buf->size) {
        fprintf(stderr, "image memory requirement larger than DRM dumb buffer\n");
        return -1;
    }
    if (find_memory_type_any(ctx->phys, combined_bits, &memory_type, &memory_flags) != 0) {
        fprintf(stderr, "no compatible memory type\n");
        return -1;
    }

    import_fd = dup(buf->prime_fd);
    if (import_fd < 0) {
        fprintf(stderr, "dup prime fd failed: %s\n", strerror(errno));
        return -1;
    }

    VkMemoryDedicatedAllocateInfo dedicated = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = out->image,
    };
    VkImportMemoryFdInfoKHR import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .pNext = &dedicated,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        .fd = import_fd,
    };
    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &import_info,
        .allocationSize = req.size,
        .memoryTypeIndex = memory_type,
    };
    if (vk_ok(vkAllocateMemory(ctx->device, &alloc, NULL, &out->memory),
              "vkAllocateMemory(import)") != 0) {
        close(import_fd);
        return -1;
    }
    import_fd = -1;

    if (vk_ok(vkBindImageMemory(ctx->device, out->image, out->memory, 0),
              "vkBindImageMemory") != 0)
        return -1;

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = out->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    if (vk_ok(vkCreateImageView(ctx->device, &view_info, NULL, &out->view),
              "vkCreateImageView(imported)") != 0)
        return -1;

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ctx->render_pass,
        .attachmentCount = 1,
        .pAttachments = &out->view,
        .width = buf->width,
        .height = buf->height,
        .layers = 1,
    };
    if (vk_ok(vkCreateFramebuffer(ctx->device, &framebuffer_info, NULL,
                                  &out->framebuffer),
              "vkCreateFramebuffer") != 0)
        return -1;

    printf("imported_fb=%u memory_type=%u flags=0x%x\n",
           buf->fb_id, memory_type, memory_flags);
    return 0;
}

static void destroy_imported(struct vk_ctx *ctx, struct imported_image *image)
{
    if (image->framebuffer)
        vkDestroyFramebuffer(ctx->device, image->framebuffer, NULL);
    if (image->view)
        vkDestroyImageView(ctx->device, image->view, NULL);
    if (image->image)
        vkDestroyImage(ctx->device, image->image, NULL);
    if (image->memory)
        vkFreeMemory(ctx->device, image->memory, NULL);
}

static int render_textured(struct vk_ctx *ctx, struct imported_image *image,
                           unsigned int frame)
{
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vk_ok(vkAllocateCommandBuffers(ctx->device, &cmd_alloc, &command_buffer),
              "vkAllocateCommandBuffers(render)") != 0)
        return -1;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (vk_ok(vkBeginCommandBuffer(command_buffer, &begin_info),
              "vkBeginCommandBuffer(render)") != 0)
        return -1;

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageMemoryBarrier to_color = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = image->initialized ? VK_ACCESS_HOST_READ_BIT : 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = image->layout,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         image->initialized ? VK_PIPELINE_STAGE_HOST_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_color);

    VkClearValue clear = {
        .color = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} },
    };
    VkRenderPassBeginInfo render_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = image->framebuffer,
        .renderArea = {
            .offset = { .x = 0, .y = 0 },
            .extent = { .width = image->width, .height = image->height },
        },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };

    vkCmdBeginRenderPass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ctx->pipeline_layout, 0, 1, &ctx->desc_set, 0, NULL);
    VkDeviceSize vertex_offset = 0;
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &ctx->vertex_buffer, &vertex_offset);
    vkCmdDraw(command_buffer, ctx->vertex_count, 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);

    VkImageMemoryBarrier to_host = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_host);

    if (vk_ok(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(render)") != 0)
        return -1;

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    if (vk_ok(vkQueueSubmit(ctx->queue, 1, &submit, VK_NULL_HANDLE),
              "vkQueueSubmit(render)") != 0)
        return -1;
    if (vk_ok(vkQueueWaitIdle(ctx->queue), "vkQueueWaitIdle(render)") != 0)
        return -1;

    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &command_buffer);
    image->layout = VK_IMAGE_LAYOUT_GENERAL;
    image->initialized = true;
    if (!service_mode || frame == 0 || (frame % 60u) == 0)
        printf("kgsl_glyph_text_render frame=%u vertices=%u\n", frame, ctx->vertex_count);
    return 0;
}

static int verify_glyph_samples(const struct drm_buffer *buf, unsigned int frame)
{
    const uint8_t white[4] = {0xff, 0xff, 0xff, 0xff};
    const uint8_t black[4] = {0x00, 0x00, 0x00, 0xff};
    const struct {
        const char *name;
        uint32_t x;
        uint32_t y;
        const uint8_t *expected;
    } samples[] = {
        {
            .name = "title_a_stroke",
            .x = TEXT_MARGIN_X + (2 * TEXT_SCALE) + (TEXT_SCALE / 2),
            .y = TEXT_MARGIN_Y + (1 * TEXT_SCALE) + (TEXT_SCALE / 2),
            .expected = white,
        },
        {
            .name = "title_a_hole",
            .x = TEXT_MARGIN_X + (3 * TEXT_SCALE) + (TEXT_SCALE / 2),
            .y = TEXT_MARGIN_Y + (3 * TEXT_SCALE) + (TEXT_SCALE / 2),
            .expected = black,
        },
        {
            .name = "outside_text",
            .x = TEXT_MARGIN_X / 2,
            .y = TEXT_MARGIN_Y / 2,
            .expected = black,
        },
    };
    uint32_t pass = 0;

    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ, "cpu_read_start");
    for (uint32_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        const uint8_t *px = (const uint8_t *)buf->map +
                            (size_t)samples[i].y * buf->pitch +
                            (size_t)samples[i].x * 4;
        bool ok = memcmp(px, samples[i].expected, 4) == 0;

        printf("sample frame=%u name=%s x=%u y=%u bgra=%02x %02x %02x %02x expected=%02x %02x %02x %02x ok=%s\n",
               frame, samples[i].name, samples[i].x, samples[i].y,
               px[0], px[1], px[2], px[3],
               samples[i].expected[0], samples[i].expected[1],
               samples[i].expected[2], samples[i].expected[3],
               ok ? "yes" : "no");
        if (ok)
            pass++;
    }
    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ, "cpu_read_end");

    printf("glyph_sample_pass=%u/%zu frame=%u\n",
           pass, sizeof(samples) / sizeof(samples[0]), frame);
    return pass == sizeof(samples) / sizeof(samples[0]) ? 0 : -1;
}

static void page_flip_handler(int fd, unsigned int frame,
                              unsigned int sec, unsigned int usec,
                              void *data)
{
    (void)fd;
    struct flip_state *state = data;

    state->seen++;
    state->waiting = 0;
    if (!service_mode || (state->seen % 60u) == 0)
        printf("page_flip_event frame=%u sec=%u usec=%u seen=%u\n",
               frame, sec, usec, state->seen);
}

static int wait_for_flip(int fd, struct flip_state *state)
{
    drmEventContext ev;
    memset(&ev, 0, sizeof(ev));
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = page_flip_handler;

    while (state->waiting) {
        fd_set fds;
        struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        int rc = select(fd + 1, &fds, NULL, NULL, &timeout);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "select failed: %s\n", strerror(errno));
            return -1;
        }
        if (rc == 0) {
            fprintf(stderr, "page flip wait timed out\n");
            return -1;
        }
        if (drmHandleEvent(fd, &ev) != 0) {
            fprintf(stderr, "drmHandleEvent failed: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *vert_path = "/data/experiments/alioth_vulkan_glyph_text.vert.spv";
    const char *frag_path = "/data/experiments/alioth_vulkan_glyph_text.frag.spv";
    unsigned int flips = 10;
    int shader_arg = 2;
    int fd = -1;
    int rc = 1;
    struct drm_target target;
    struct drm_buffer buffers[2];
    struct imported_image images[2];
    struct vk_ctx vk;
    struct flip_state flip = {0};
    struct input_ctx input;

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    memset(&target, 0, sizeof(target));
    memset(buffers, 0, sizeof(buffers));
    memset(images, 0, sizeof(images));
    memset(&vk, 0, sizeof(vk));
    memset(&input, 0, sizeof(input));
    buffers[0].prime_fd = -1;
    buffers[1].prime_fd = -1;

    if (argc >= 2) {
        if (strcmp(argv[1], "monitor") == 0 || strcmp(argv[1], "service") == 0) {
            service_mode = true;
            flips = 0;
            shader_arg = 2;
        } else {
            flips = (unsigned int)strtoul(argv[1], NULL, 10);
            shader_arg = 2;
        }
    }
    if (argc >= shader_arg + 1)
        vert_path = argv[shader_arg];
    if (argc >= shader_arg + 2)
        frag_path = argv[shader_arg + 1];
    if ((!service_mode && (flips < 1 || flips > 60)) || (argc >= 2 && flips == 0 && !service_mode)) {
        fprintf(stderr, "usage: %s [flips 1..60|monitor] [vert.spv] [frag.spv]\n", argv[0]);
        return 2;
    }

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    prepare_monitor_lines();

    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open /dev/dri/card0 failed: %s\n", strerror(errno));
        return 1;
    }
    if (find_target(fd, &target) != 0) {
        fprintf(stderr, "no connected DRM target\n");
        goto cleanup;
    }
    printf("drm_target connector=%u crtc=%u mode=%ux%u@%u flips=%u service_mode=%s\n",
           target.connector_id, target.crtc_id,
           target.mode.hdisplay, target.mode.vdisplay,
           target.mode.vrefresh, flips, service_mode ? "yes" : "no");

    if (create_drm_buffer(fd, &target, &buffers[0]) != 0)
        goto cleanup;
    if (create_drm_buffer(fd, &target, &buffers[1]) != 0)
        goto cleanup;

    if (init_vulkan(&vk) != 0)
        goto cleanup;
    if (check_external_format(&vk, &buffers[0]) != 0)
        goto cleanup;
    if (upload_texture(&vk) != 0)
        goto cleanup;
    if (create_descriptor_set(&vk) != 0)
        goto cleanup;
    if (create_text_vertices(&vk, buffers[0].width, buffers[0].height) != 0)
        goto cleanup;
    if (create_pipeline(&vk, buffers[0].width, buffers[0].height,
                        vert_path, frag_path) != 0)
        goto cleanup;
    if (import_buffer(&vk, &buffers[0], &images[0]) != 0)
        goto cleanup;
    if (import_buffer(&vk, &buffers[1], &images[1]) != 0)
        goto cleanup;

    if (render_textured(&vk, &images[0], 0) != 0)
        goto cleanup;
    if (verify_glyph_samples(&buffers[0], 0) != 0)
        goto cleanup;

    if (drmModeSetCrtc(fd, target.crtc_id, buffers[0].fb_id, 0, 0,
                       &target.connector_id, 1, &target.mode) != 0) {
        fprintf(stderr, "drmModeSetCrtc failed: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("initial_set_crtc=PASS fb=%u\n", buffers[0].fb_id);
    if (service_mode)
        open_input_devices(&input);

    for (unsigned int i = 0; service_mode ? !stop_requested : i < flips; i++) {
        unsigned int next = (i + 1) % 2;
        bool input_changed = service_mode ? poll_input_devices(&input) : false;

        if (service_mode && (input_changed || ((i + 1) % 5u) == 0)) {
            if (refresh_text_vertices_if_needed(&vk, buffers[next].width,
                                                buffers[next].height, i + 1) != 0)
                goto cleanup;
        }
        if (render_textured(&vk, &images[next], i + 1) != 0)
            goto cleanup;
        if (!service_mode && verify_glyph_samples(&buffers[next], i + 1) != 0)
            goto cleanup;

        flip.waiting = 1;
        if (drmModePageFlip(fd, target.crtc_id, buffers[next].fb_id,
                            DRM_MODE_PAGE_FLIP_EVENT, &flip) != 0) {
            fprintf(stderr, "drmModePageFlip failed flip=%u fb=%u errno=%d %s\n",
                    i, buffers[next].fb_id, errno, strerror(errno));
            goto cleanup;
        }
        if (!service_mode || ((i + 1) % 60u) == 0)
            printf("page_flip_submit index=%u fb=%u service_mode=%s\n",
                   i, buffers[next].fb_id, service_mode ? "yes" : "no");
        if (wait_for_flip(fd, &flip) != 0)
            goto cleanup;

        struct timespec pause = service_mode ?
            (struct timespec){ .tv_sec = 1, .tv_nsec = 0 } :
            (struct timespec){ .tv_sec = 0, .tv_nsec = 80000000L };
        nanosleep(&pause, NULL);
    }

    if (service_mode)
        printf("gpu_monitor_service_stop submitted=%u events=%u stop_requested=%d\n",
               flip.seen, flip.seen, stop_requested);
    else
        printf("drm_prime_kgsl_glyph_text_pageflip=PASS submitted=%u events=%u\n",
               flips, flip.seen);
    rc = 0;

cleanup:
    close_input_devices(&input);
    if (vk.device) {
        destroy_imported(&vk, &images[1]);
        destroy_imported(&vk, &images[0]);
    }
    destroy_vulkan(&vk);
    if (fd >= 0) {
        destroy_drm_buffer(fd, &buffers[1]);
        destroy_drm_buffer(fd, &buffers[0]);
        close(fd);
    }
    return rc;
}
