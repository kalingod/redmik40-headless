#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define CHECK_VK(expr) do { \
    VkResult _res = (expr); \
    if (_res != VK_SUCCESS) { \
        fprintf(stderr, "%s failed: %d\n", #expr, _res); \
        rc = 1; \
        goto cleanup; \
    } \
} while (0)

static const char *device_type_name(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "OTHER";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "INTEGRATED_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "DISCRETE_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "VIRTUAL_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
    default: return "UNKNOWN";
    }
}

int main(void)
{
    int rc = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t queue_family = UINT32_MAX;

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-noop-submit",
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
        fprintf(stderr, "no Vulkan physical devices\n");
        rc = 1;
        goto cleanup;
    }

    VkPhysicalDevice *phys_list = calloc(phys_count, sizeof(*phys_list));
    if (!phys_list) {
        fprintf(stderr, "calloc physical devices failed\n");
        rc = 1;
        goto cleanup;
    }
    VkResult enum_res = vkEnumeratePhysicalDevices(instance, &phys_count, phys_list);
    if (enum_res != VK_SUCCESS) {
        fprintf(stderr, "vkEnumeratePhysicalDevices(list) failed: %d\n", enum_res);
        free(phys_list);
        rc = 1;
        goto cleanup;
    }
    phys = phys_list[0];
    free(phys_list);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);
    printf("physical_device=%s api=%u.%u.%u driver=%u type=%s\n",
           props.deviceName,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion),
           props.driverVersion,
           device_type_name(props.deviceType));

    vkGetPhysicalDeviceQueueFamilyProperties(phys, &queue_count, NULL);
    if (queue_count == 0) {
        fprintf(stderr, "no queue families\n");
        rc = 1;
        goto cleanup;
    }
    VkQueueFamilyProperties *queues = calloc(queue_count, sizeof(*queues));
    if (!queues) {
        fprintf(stderr, "calloc queue families failed\n");
        rc = 1;
        goto cleanup;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &queue_count, queues);
    for (uint32_t i = 0; i < queue_count; i++) {
        printf("queue_family[%u] flags=0x%x count=%u\n",
               i, queues[i].queueFlags, queues[i].queueCount);
        if (queue_family == UINT32_MAX && queues[i].queueCount > 0 &&
            (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            queue_family = i;
        }
    }
    if (queue_family == UINT32_MAX) {
        for (uint32_t i = 0; i < queue_count; i++) {
            if (queues[i].queueCount > 0) {
                queue_family = i;
                break;
            }
        }
    }
    free(queues);
    if (queue_family == UINT32_MAX) {
        fprintf(stderr, "no usable queue family\n");
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
    if (queue == VK_NULL_HANDLE) {
        fprintf(stderr, "vkGetDeviceQueue returned NULL\n");
        rc = 1;
        goto cleanup;
    }

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family,
    };
    CHECK_VK(vkCreateCommandPool(device, &pool_info, NULL, &pool));

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CHECK_VK(vkAllocateCommandBuffers(device, &alloc_info, &cmd));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CHECK_VK(vkBeginCommandBuffer(cmd, &begin_info));
    CHECK_VK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    CHECK_VK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    CHECK_VK(vkQueueWaitIdle(queue));
    CHECK_VK(vkDeviceWaitIdle(device));

    printf("noop_submit=PASS queue_family=%u\n", queue_family);

cleanup:
    if (device != VK_NULL_HANDLE) {
        if (pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, pool, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    return rc;
}
