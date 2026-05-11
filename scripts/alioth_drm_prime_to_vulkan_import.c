#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
#include <drm.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <vulkan/vulkan.h>

#define CHECK_VK(expr) do { \
    VkResult _res = (expr); \
    if (_res != VK_SUCCESS) { \
        fprintf(stderr, "%s failed: %d\n", #expr, _res); \
        rc = 1; \
        goto cleanup; \
    } \
} while (0)

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

static int create_drm_prime_fd(int *drm_fd_out, uint32_t *handle_out, int *prime_fd_out)
{
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    struct drm_mode_create_dumb create_req = {0};
    int prime_fd = -1;

    if (fd < 0) {
        fprintf(stderr, "open /dev/dri/card0 failed: %s\n", strerror(errno));
        return -1;
    }

    create_req.width = 64;
    create_req.height = 64;
    create_req.bpp = 32;
    if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        fprintf(stderr, "CREATE_DUMB failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    if (drmPrimeHandleToFD(fd, create_req.handle, DRM_CLOEXEC, &prime_fd) != 0) {
        fprintf(stderr, "drmPrimeHandleToFD failed: %s\n", strerror(errno));
        struct drm_mode_destroy_dumb destroy_req = { .handle = create_req.handle };
        ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        close(fd);
        return -1;
    }

    printf("drm_dumb width=64 height=64 bpp=32 pitch=%u size=%llu handle=%u\n",
           create_req.pitch, (unsigned long long)create_req.size, create_req.handle);
    *drm_fd_out = fd;
    *handle_out = create_req.handle;
    *prime_fd_out = prime_fd;
    return 0;
}

static void destroy_drm_dumb(int drm_fd, uint32_t handle)
{
    if (drm_fd >= 0 && handle) {
        struct drm_mode_destroy_dumb destroy_req = { .handle = handle };
        ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
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
    int rc = 0;
    int drm_fd = -1;
    uint32_t drm_handle = 0;
    int prime_fd = -1;
    int import_fd = -1;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t queue_family = UINT32_MAX;
    uint32_t ext_count = 0;
    VkExtensionProperties *exts = NULL;
    PFN_vkGetMemoryFdPropertiesKHR pfn_get_memory_fd_props = NULL;

    if (create_drm_prime_fd(&drm_fd, &drm_handle, &prime_fd) != 0)
        return 1;
    describe_fd(prime_fd);

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-drm-prime-to-vulkan-import",
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
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
        if (queue_family == UINT32_MAX && queues[i].queueCount > 0)
            queue_family = i;
    }
    free(queues);
    if (queue_family == UINT32_MAX) {
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
        .extent = { .width = 64, .height = 64, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
                                     prime_fd, &fd_props));
    printf("image_req size=%llu type_bits=0x%x fd_type_bits=0x%x\n",
           (unsigned long long)req.size, req.memoryTypeBits, fd_props.memoryTypeBits);

    import_fd = dup(prime_fd);
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
    printf("drm_prime_to_vulkan_import=PASS\n");

cleanup:
    if (import_fd >= 0)
        close(import_fd);
    if (prime_fd >= 0)
        close(prime_fd);
    free(exts);
    if (device != VK_NULL_HANDLE) {
        if (memory != VK_NULL_HANDLE)
            vkFreeMemory(device, memory, NULL);
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(device, image, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    destroy_drm_dumb(drm_fd, drm_handle);
    if (drm_fd >= 0)
        close(drm_fd);
    return rc;
}
