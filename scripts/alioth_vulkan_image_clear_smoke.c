#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define IMAGE_WIDTH 16u
#define IMAGE_HEIGHT 16u
#define BYTES_PER_PIXEL 4u

#define CHECK_VK(expr) do { \
    VkResult _res = (expr); \
    if (_res != VK_SUCCESS) { \
        fprintf(stderr, "%s failed: %d\n", #expr, _res); \
        rc = 1; \
        goto cleanup; \
    } \
} while (0)

static int find_memory_type(VkPhysicalDevice phys, uint32_t type_bits,
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

int main(void)
{
    const VkDeviceSize readback_size = IMAGE_WIDTH * IMAGE_HEIGHT * BYTES_PER_PIXEL;
    const uint8_t expected[BYTES_PER_PIXEL] = {0x00, 0xff, 0x00, 0xff};
    int rc = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory readback_memory = VK_NULL_HANDLE;
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    void *mapped = NULL;
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t queue_family = UINT32_MAX;
    uint32_t image_memory_type = UINT32_MAX;
    uint32_t readback_memory_type = UINT32_MAX;
    VkMemoryPropertyFlags image_memory_flags = 0;
    VkMemoryPropertyFlags readback_memory_flags = 0;

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-image-clear-smoke",
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

    VkPhysicalDeviceProperties dev_props;
    vkGetPhysicalDeviceProperties(phys, &dev_props);
    printf("physical_device=%s api=%u.%u.%u driver=%u\n",
           dev_props.deviceName,
           VK_VERSION_MAJOR(dev_props.apiVersion),
           VK_VERSION_MINOR(dev_props.apiVersion),
           VK_VERSION_PATCH(dev_props.apiVersion),
           dev_props.driverVersion);

    VkFormatProperties format_props;
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_R8G8B8A8_UNORM, &format_props);
    printf("format_optimal_features=0x%x\n", format_props.optimalTilingFeatures);
    if ((format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0 ||
        (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0) {
        fprintf(stderr, "R8G8B8A8_UNORM lacks optimal transfer src/dst support\n");
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
        if (queue_family == UINT32_MAX && queues[i].queueCount > 0 &&
            (queues[i].queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            queue_family = i;
        }
    }
    free(queues);
    if (queue_family == UINT32_MAX) {
        fprintf(stderr, "no transfer queue family\n");
        rc = 1;
        goto cleanup;
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
    };
    CHECK_VK(vkCreateDevice(phys, &device_info, NULL, &device));
    vkGetDeviceQueue(device, queue_family, 0, &queue);

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { .width = IMAGE_WIDTH, .height = IMAGE_HEIGHT, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    CHECK_VK(vkCreateImage(device, &image_info, NULL, &image));

    VkMemoryRequirements image_req;
    vkGetImageMemoryRequirements(device, image, &image_req);
    if (find_memory_type(phys, image_req.memoryTypeBits, 0,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         &image_memory_type, &image_memory_flags) != 0) {
        fprintf(stderr, "no memory type for image, type_bits=0x%x\n", image_req.memoryTypeBits);
        rc = 1;
        goto cleanup;
    }
    printf("image_memory_size=%" PRIu64 " type=%u flags=0x%x\n",
           (uint64_t)image_req.size, image_memory_type, image_memory_flags);
    VkMemoryAllocateInfo image_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = image_req.size,
        .memoryTypeIndex = image_memory_type,
    };
    CHECK_VK(vkAllocateMemory(device, &image_alloc, NULL, &image_memory));
    CHECK_VK(vkBindImageMemory(device, image, image_memory, 0));

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = readback_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    CHECK_VK(vkCreateBuffer(device, &buffer_info, NULL, &readback));

    VkMemoryRequirements readback_req;
    vkGetBufferMemoryRequirements(device, readback, &readback_req);
    if (find_memory_type(phys, readback_req.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         0, &readback_memory_type, &readback_memory_flags) != 0) {
        fprintf(stderr, "no host-visible coherent memory type for readback, type_bits=0x%x\n",
                readback_req.memoryTypeBits);
        rc = 1;
        goto cleanup;
    }
    printf("readback_size=%" PRIu64 " memory_size=%" PRIu64 " type=%u flags=0x%x\n",
           (uint64_t)readback_size, (uint64_t)readback_req.size,
           readback_memory_type, readback_memory_flags);
    VkMemoryAllocateInfo readback_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = readback_req.size,
        .memoryTypeIndex = readback_memory_type,
    };
    CHECK_VK(vkAllocateMemory(device, &readback_alloc, NULL, &readback_memory));
    CHECK_VK(vkBindBufferMemory(device, readback, readback_memory, 0));
    CHECK_VK(vkMapMemory(device, readback_memory, 0, readback_size, 0, &mapped));
    memset(mapped, 0xa5, (size_t)readback_size);

    VkCommandPoolCreateInfo cmd_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family,
    };
    CHECK_VK(vkCreateCommandPool(device, &cmd_pool_info, NULL, &cmd_pool));

    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CHECK_VK(vkAllocateCommandBuffers(device, &cmd_alloc, &cmd));

    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CHECK_VK(vkBeginCommandBuffer(cmd, &begin));

    VkImageSubresourceRange subresource = {
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
        .image = image,
        .subresourceRange = subresource,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, NULL, 0, NULL, 1, &to_dst);

    VkClearColorValue clear = {
        .float32 = {0.0f, 1.0f, 0.0f, 1.0f},
    };
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear, 1, &subresource);

    VkImageMemoryBarrier to_src = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresource,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, NULL, 0, NULL, 1, &to_src);

    VkBufferImageCopy copy = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { .x = 0, .y = 0, .z = 0 },
        .imageExtent = { .width = IMAGE_WIDTH, .height = IMAGE_HEIGHT, .depth = 1 },
    };
    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback, 1, &copy);

    VkBufferMemoryBarrier host_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = readback,
        .offset = 0,
        .size = readback_size,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0,
                         0, NULL, 1, &host_barrier, 0, NULL);
    CHECK_VK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    CHECK_VK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    CHECK_VK(vkQueueWaitIdle(queue));
    CHECK_VK(vkDeviceWaitIdle(device));

    const uint8_t *bytes = mapped;
    uint32_t mismatches = 0;
    uint64_t checksum = 1469598103934665603ull;
    for (VkDeviceSize i = 0; i < readback_size; i++) {
        checksum ^= bytes[i];
        checksum *= 1099511628211ull;
        if (bytes[i] != expected[i % BYTES_PER_PIXEL]) {
            if (mismatches < 8) {
                fprintf(stderr, "mismatch offset=%" PRIu64 " got=0x%02x expected=0x%02x\n",
                        (uint64_t)i, bytes[i], expected[i % BYTES_PER_PIXEL]);
            }
            mismatches++;
        }
    }
    printf("first_pixel=%02x %02x %02x %02x expected=%02x %02x %02x %02x\n",
           bytes[0], bytes[1], bytes[2], bytes[3],
           expected[0], expected[1], expected[2], expected[3]);
    printf("image_clear_checksum=0x%016" PRIx64 " mismatches=%u bytes=%" PRIu64 "\n",
           checksum, mismatches, (uint64_t)readback_size);
    if (mismatches != 0) {
        rc = 1;
        goto cleanup;
    }
    printf("image_clear_smoke=PASS queue_family=%u\n", queue_family);

cleanup:
    if (device != VK_NULL_HANDLE) {
        if (mapped)
            vkUnmapMemory(device, readback_memory);
        if (cmd_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, cmd_pool, NULL);
        if (readback != VK_NULL_HANDLE)
            vkDestroyBuffer(device, readback, NULL);
        if (readback_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, readback_memory, NULL);
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(device, image, NULL);
        if (image_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, image_memory, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    return rc;
}
