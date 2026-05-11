#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
#include <drm.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <linux/dma-buf.h>
#include <vulkan/vulkan.h>

#define WIDTH 64u
#define HEIGHT 64u

#define CHECK_VK(expr) do { \
    VkResult _res = (expr); \
    if (_res != VK_SUCCESS) { \
        fprintf(stderr, "%s failed: %d\n", #expr, _res); \
        rc = 1; \
        goto cleanup; \
    } \
} while (0)

struct drm_buffer {
    int fd;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    void *map;
    int prime_fd;
};

static bool has_extension(const VkExtensionProperties *exts, uint32_t count, const char *name)
{
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, name) == 0)
            return true;
    }
    return false;
}

static int find_memory_type(VkPhysicalDevice phys, uint32_t type_bits,
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

static void describe_fd(int fd)
{
    struct stat st;
    char proc_path[64];
    char target[256];
    ssize_t len;

    if (fstat(fd, &st) == 0) {
        printf("prime_fd=%d mode=0%o size=%lld inode=%lld\n",
               fd, st.st_mode & 07777, (long long)st.st_size, (long long)st.st_ino);
    } else {
        printf("prime_fd=%d fstat_error=%s\n", fd, strerror(errno));
    }
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
    len = readlink(proc_path, target, sizeof(target) - 1);
    if (len >= 0) {
        target[len] = '\0';
        printf("prime_fd_target=%s\n", target);
    }
}

static int create_drm_buffer(struct drm_buffer *buf)
{
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};

    memset(buf, 0, sizeof(*buf));
    buf->fd = -1;
    buf->prime_fd = -1;

    buf->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (buf->fd < 0) {
        fprintf(stderr, "open /dev/dri/card0 failed: %s\n", strerror(errno));
        return -1;
    }

    create_req.width = WIDTH;
    create_req.height = HEIGHT;
    create_req.bpp = 32;
    if (ioctl(buf->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        fprintf(stderr, "CREATE_DUMB failed: %s\n", strerror(errno));
        return -1;
    }
    buf->handle = create_req.handle;
    buf->pitch = create_req.pitch;
    buf->size = create_req.size;

    map_req.handle = buf->handle;
    if (ioctl(buf->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        fprintf(stderr, "MAP_DUMB failed: %s\n", strerror(errno));
        return -1;
    }
    buf->map = mmap(NULL, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    buf->fd, map_req.offset);
    if (buf->map == MAP_FAILED) {
        fprintf(stderr, "mmap dumb buffer failed: %s\n", strerror(errno));
        buf->map = NULL;
        return -1;
    }

    if (drmPrimeHandleToFD(buf->fd, buf->handle, DRM_CLOEXEC, &buf->prime_fd) != 0) {
        fprintf(stderr, "drmPrimeHandleToFD failed: %s\n", strerror(errno));
        return -1;
    }

    printf("drm_dumb width=%u height=%u bpp=32 pitch=%u size=%llu handle=%u\n",
           WIDTH, HEIGHT, buf->pitch, (unsigned long long)buf->size, buf->handle);
    describe_fd(buf->prime_fd);

    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE, "cpu_zero_start");
    memset(buf->map, 0, buf->size);
    if (msync(buf->map, buf->size, MS_SYNC) != 0)
        printf("msync_zero=skip errno=%d %s\n", errno, strerror(errno));
    else
        printf("msync_zero=ok\n");
    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE, "cpu_zero_end");
    return 0;
}

static void destroy_drm_buffer(struct drm_buffer *buf)
{
    if (buf->map)
        munmap(buf->map, buf->size);
    if (buf->prime_fd >= 0)
        close(buf->prime_fd);
    if (buf->fd >= 0 && buf->handle) {
        struct drm_mode_destroy_dumb destroy_req = { .handle = buf->handle };
        ioctl(buf->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
    if (buf->fd >= 0)
        close(buf->fd);
}

static int verify_red_pixels(const struct drm_buffer *buf)
{
    const uint8_t expected[4] = {0x00, 0x00, 0xff, 0xff};
    const uint8_t *base = (const uint8_t *)buf->map;
    uint32_t red = 0;
    uint32_t zero = 0;
    uint32_t other = 0;
    uint64_t checksum = 1469598103934665603ULL;

    printf("first_16_bytes=");
    for (uint32_t i = 0; i < 16 && i < buf->size; i++)
        printf("%02x%s", base[i], i == 15 ? "\n" : " ");

    for (uint32_t y = 0; y < HEIGHT; y++) {
        const uint8_t *row = base + y * buf->pitch;
        for (uint32_t x = 0; x < WIDTH; x++) {
            const uint8_t *px = row + x * 4;

            for (uint32_t b = 0; b < 4; b++) {
                checksum ^= px[b];
                checksum *= 1099511628211ULL;
            }
            if (memcmp(px, expected, 4) == 0) {
                red++;
            } else if (px[0] == 0 && px[1] == 0 && px[2] == 0 && px[3] == 0) {
                zero++;
            } else {
                other++;
            }
        }
    }

    printf("readback_pixels red=%u zero=%u other=%u total=%u checksum=0x%016llx\n",
           red, zero, other, WIDTH * HEIGHT, (unsigned long long)checksum);
    if (red == WIDTH * HEIGHT && zero == 0 && other == 0) {
        printf("drm_prime_kgsl_clear_readback=PASS\n");
        return 0;
    }
    fprintf(stderr, "unexpected readback pattern\n");
    return -1;
}

int main(void)
{
    const char *required_exts[] = {
        "VK_KHR_external_memory",
        "VK_KHR_external_memory_fd",
        "VK_KHR_dedicated_allocation",
        "VK_KHR_bind_memory2",
        "VK_EXT_external_memory_dma_buf",
    };
    struct drm_buffer drm_buf;
    int rc = 0;
    int import_fd = -1;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t queue_family = UINT32_MAX;
    uint32_t ext_count = 0;
    VkExtensionProperties *exts = NULL;
    PFN_vkGetMemoryFdPropertiesKHR pfn_get_memory_fd_props = NULL;

    if (create_drm_buffer(&drm_buf) != 0)
        return 1;

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-drm-prime-vulkan-clear-readback",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    CHECK_VK(vkCreateInstance(&instance_info, NULL, &instance));
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &phys_count, NULL));
    if (phys_count == 0) {
        fprintf(stderr, "no physical devices\n");
        rc = 1;
        goto cleanup;
    }
    VkPhysicalDevice *phys_list = calloc(phys_count, sizeof(*phys_list));
    if (!phys_list) {
        rc = 1;
        goto cleanup;
    }
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &phys_count, phys_list));
    phys = phys_list[0];
    free(phys_list);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);
    printf("physical_device=%s api=%u.%u.%u driver=%u\n",
           props.deviceName,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion),
           props.driverVersion);

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
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    };
    VkExternalImageFormatProperties ext_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 image_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &ext_props,
    };
    VkResult fmt_res = vkGetPhysicalDeviceImageFormatProperties2(phys, &image_format, &image_props);
    printf("linear_b8g8r8a8_dma_buf_format result=%d maxExtent=%ux%u features=0x%x compatible=0x%x\n",
           fmt_res,
           fmt_res == VK_SUCCESS ? image_props.imageFormatProperties.maxExtent.width : 0,
           fmt_res == VK_SUCCESS ? image_props.imageFormatProperties.maxExtent.height : 0,
           fmt_res == VK_SUCCESS ? ext_props.externalMemoryProperties.externalMemoryFeatures : 0,
           fmt_res == VK_SUCCESS ? ext_props.externalMemoryProperties.compatibleHandleTypes : 0);
    if (fmt_res != VK_SUCCESS) {
        rc = 1;
        goto cleanup;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(phys, &queue_count, NULL);
    VkQueueFamilyProperties *queues = calloc(queue_count, sizeof(*queues));
    if (!queues) {
        rc = 1;
        goto cleanup;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &queue_count, queues);
    for (uint32_t i = 0; i < queue_count; i++) {
        printf("queue_family[%u] flags=0x%x count=%u\n",
               i, queues[i].queueFlags, queues[i].queueCount);
        if (queue_family == UINT32_MAX &&
            queues[i].queueCount > 0 &&
            (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            queue_family = i;
    }
    free(queues);
    if (queue_family == UINT32_MAX) {
        fprintf(stderr, "no graphics-capable queue family\n");
        rc = 1;
        goto cleanup;
    }

    CHECK_VK(vkEnumerateDeviceExtensionProperties(phys, NULL, &ext_count, NULL));
    exts = calloc(ext_count ? ext_count : 1, sizeof(*exts));
    if (!exts) {
        rc = 1;
        goto cleanup;
    }
    CHECK_VK(vkEnumerateDeviceExtensionProperties(phys, NULL, &ext_count, exts));
    for (uint32_t i = 0; i < sizeof(required_exts) / sizeof(required_exts[0]); i++) {
        bool present = has_extension(exts, ext_count, required_exts[i]);
        printf("required_ext %-32s %s\n", required_exts[i], present ? "yes" : "no");
        if (!present) {
            rc = 1;
            goto cleanup;
        }
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family,
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
    CHECK_VK(vkCreateDevice(phys, &device_info, NULL, &device));
    vkGetDeviceQueue(device, queue_family, 0, &queue);
    pfn_get_memory_fd_props = (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR");
    if (!pfn_get_memory_fd_props) {
        fprintf(stderr, "missing vkGetMemoryFdPropertiesKHR\n");
        rc = 1;
        goto cleanup;
    }

    VkExternalMemoryImageCreateInfo external_image = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_image,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = { .width = WIDTH, .height = HEIGHT, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    CHECK_VK(vkCreateImage(device, &image_info, NULL, &image));

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, image, &req);
    VkMemoryFdPropertiesKHR fd_props = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
    };
    CHECK_VK(pfn_get_memory_fd_props(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                     drm_buf.prime_fd, &fd_props));
    printf("image_req size=%llu type_bits=0x%x fd_type_bits=0x%x\n",
           (unsigned long long)req.size, req.memoryTypeBits, fd_props.memoryTypeBits);

    import_fd = dup(drm_buf.prime_fd);
    if (import_fd < 0) {
        fprintf(stderr, "dup prime fd failed: %s\n", strerror(errno));
        rc = 1;
        goto cleanup;
    }
    uint32_t memory_type = UINT32_MAX;
    VkMemoryPropertyFlags memory_flags = 0;
    uint32_t combined_bits = req.memoryTypeBits & fd_props.memoryTypeBits;
    if (find_memory_type(phys, combined_bits, 0, &memory_type, &memory_flags) != 0) {
        fprintf(stderr, "no memory type for import combined_bits=0x%x\n", combined_bits);
        rc = 1;
        goto cleanup;
    }
    printf("import_memory_type=%u flags=0x%x combined_bits=0x%x import_fd=%d\n",
           memory_type, memory_flags, combined_bits, import_fd);

    VkMemoryDedicatedAllocateInfo dedicated = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = image,
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
    CHECK_VK(vkAllocateMemory(device, &alloc, NULL, &memory));
    import_fd = -1;
    CHECK_VK(vkBindImageMemory(device, image, memory, 0));

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_family,
    };
    CHECK_VK(vkCreateCommandPool(device, &pool_info, NULL, &command_pool));
    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CHECK_VK(vkAllocateCommandBuffers(device, &cmd_alloc, &command_buffer));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CHECK_VK(vkBeginCommandBuffer(command_buffer, &begin_info));
    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageMemoryBarrier to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_transfer);
    VkClearColorValue red = { .float32 = {1.0f, 0.0f, 0.0f, 1.0f} };
    vkCmdClearColorImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &red, 1, &range);
    VkImageMemoryBarrier to_host = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_host);
    CHECK_VK(vkEndCommandBuffer(command_buffer));

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    CHECK_VK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    CHECK_VK(vkQueueWaitIdle(queue));
    printf("kgsl_clear_submit=PASS queue_family=%u\n", queue_family);

    dma_buf_sync_fd(drm_buf.prime_fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ, "cpu_read_start");
    if (verify_red_pixels(&drm_buf) != 0)
        rc = 1;
    dma_buf_sync_fd(drm_buf.prime_fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ, "cpu_read_end");

cleanup:
    if (import_fd >= 0)
        close(import_fd);
    free(exts);
    if (device != VK_NULL_HANDLE) {
        if (command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, command_pool, NULL);
        if (memory != VK_NULL_HANDLE)
            vkFreeMemory(device, memory, NULL);
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(device, image, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    destroy_drm_buffer(&drm_buf);
    return rc;
}
