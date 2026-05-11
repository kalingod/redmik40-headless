#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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

static VkResult create_external_image(VkDevice device, VkImage *image)
{
    VkExternalMemoryImageCreateInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_info,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { .width = 64, .height = 64, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    return vkCreateImage(device, &image_info, NULL, image);
}

static void describe_fd(int fd)
{
    struct stat st;
    char proc_path[64];
    char target[256];
    ssize_t len;
    int flags;

    if (fstat(fd, &st) == 0) {
        printf("dmabuf_fd=%d mode=0%o size=%lld inode=%lld\n",
               fd, st.st_mode & 07777, (long long)st.st_size, (long long)st.st_ino);
    } else {
        printf("dmabuf_fd=%d fstat_error=%s\n", fd, strerror(errno));
    }
    flags = fcntl(fd, F_GETFD);
    if (flags >= 0)
        printf("dmabuf_fd_flags=0x%x cloexec=%s\n", flags, (flags & FD_CLOEXEC) ? "yes" : "no");
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
    len = readlink(proc_path, target, sizeof(target) - 1);
    if (len >= 0) {
        target[len] = '\0';
        printf("dmabuf_fd_target=%s\n", target);
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
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkImage export_image = VK_NULL_HANDLE;
    VkImage import_image = VK_NULL_HANDLE;
    VkDeviceMemory export_memory = VK_NULL_HANDLE;
    VkDeviceMemory import_memory = VK_NULL_HANDLE;
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t queue_family = UINT32_MAX;
    uint32_t ext_count = 0;
    VkExtensionProperties *exts = NULL;
    int exported_fd = -1;
    int import_fd = -1;
    PFN_vkGetMemoryFdKHR pfn_get_memory_fd = NULL;
    PFN_vkGetMemoryFdPropertiesKHR pfn_get_memory_fd_props = NULL;

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-dmabuf-export-import",
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
        fprintf(stderr, "no queue family\n");
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
    pfn_get_memory_fd = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR");
    pfn_get_memory_fd_props = (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR");
    if (!pfn_get_memory_fd || !pfn_get_memory_fd_props) {
        fprintf(stderr, "missing external memory fd function pointers\n");
        rc = 1;
        goto cleanup;
    }

    CHECK_VK(create_external_image(device, &export_image));
    CHECK_VK(create_external_image(device, &import_image));

    VkMemoryRequirements export_req;
    vkGetImageMemoryRequirements(device, export_image, &export_req);
    uint32_t export_type = UINT32_MAX;
    VkMemoryPropertyFlags export_flags = 0;
    if (find_memory_type(phys, export_req.memoryTypeBits,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         &export_type, &export_flags) != 0) {
        fprintf(stderr, "no memory type for export image, type_bits=0x%x\n", export_req.memoryTypeBits);
        rc = 1;
        goto cleanup;
    }
    printf("export_image_memory size=%llu type_bits=0x%x type=%u flags=0x%x\n",
           (unsigned long long)export_req.size, export_req.memoryTypeBits, export_type, export_flags);

    VkMemoryDedicatedAllocateInfo export_dedicated = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = export_image,
    };
    VkExportMemoryAllocateInfo export_alloc_ext = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .pNext = &export_dedicated,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkMemoryAllocateInfo export_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export_alloc_ext,
        .allocationSize = export_req.size,
        .memoryTypeIndex = export_type,
    };
    CHECK_VK(vkAllocateMemory(device, &export_alloc, NULL, &export_memory));
    CHECK_VK(vkBindImageMemory(device, export_image, export_memory, 0));

    VkMemoryGetFdInfoKHR fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = export_memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    CHECK_VK(pfn_get_memory_fd(device, &fd_info, &exported_fd));
    describe_fd(exported_fd);

    VkMemoryFdPropertiesKHR fd_props = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
    };
    CHECK_VK(pfn_get_memory_fd_props(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                     exported_fd, &fd_props));
    printf("fd_memory_type_bits=0x%x\n", fd_props.memoryTypeBits);

    import_fd = dup(exported_fd);
    if (import_fd < 0) {
        fprintf(stderr, "dup dmabuf fd failed: %s\n", strerror(errno));
        rc = 1;
        goto cleanup;
    }
    printf("import_fd=%d\n", import_fd);

    VkMemoryRequirements import_req;
    vkGetImageMemoryRequirements(device, import_image, &import_req);
    uint32_t import_type = UINT32_MAX;
    VkMemoryPropertyFlags import_flags = 0;
    uint32_t import_type_bits = import_req.memoryTypeBits & fd_props.memoryTypeBits;
    if (find_memory_type(phys, import_type_bits,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         &import_type, &import_flags) != 0) {
        fprintf(stderr, "no memory type for import image, type_bits=0x%x fd_bits=0x%x\n",
                import_req.memoryTypeBits, fd_props.memoryTypeBits);
        rc = 1;
        goto cleanup;
    }
    printf("import_image_memory size=%llu req_bits=0x%x combined_bits=0x%x type=%u flags=0x%x\n",
           (unsigned long long)import_req.size, import_req.memoryTypeBits,
           import_type_bits, import_type, import_flags);

    VkMemoryDedicatedAllocateInfo import_dedicated = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = import_image,
    };
    VkImportMemoryFdInfoKHR import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .pNext = &import_dedicated,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        .fd = import_fd,
    };
    VkMemoryAllocateInfo import_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &import_info,
        .allocationSize = import_req.size,
        .memoryTypeIndex = import_type,
    };
    CHECK_VK(vkAllocateMemory(device, &import_alloc, NULL, &import_memory));
    import_fd = -1;
    CHECK_VK(vkBindImageMemory(device, import_image, import_memory, 0));

    printf("dmabuf_export_import=PASS exported_fd=%d\n", exported_fd);

cleanup:
    if (import_fd >= 0)
        close(import_fd);
    if (exported_fd >= 0)
        close(exported_fd);
    free(exts);
    if (device != VK_NULL_HANDLE) {
        if (import_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, import_memory, NULL);
        if (export_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, export_memory, NULL);
        if (import_image != VK_NULL_HANDLE)
            vkDestroyImage(device, import_image, NULL);
        if (export_image != VK_NULL_HANDLE)
            vkDestroyImage(device, export_image, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    return rc;
}
