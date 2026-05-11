#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

static bool has_extension(const VkExtensionProperties *exts, uint32_t count, const char *name)
{
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, name) == 0)
            return true;
    }
    return false;
}

static void print_bool_ext(const VkExtensionProperties *exts, uint32_t count, const char *name)
{
    printf("device_ext %-38s %s\n", name, has_extension(exts, count, name) ? "yes" : "no");
}

static void query_external_image(VkPhysicalDevice phys,
                                 VkExternalMemoryHandleTypeFlagBits handle_type,
                                 const char *label)
{
    VkPhysicalDeviceExternalImageFormatInfo ext_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .handleType = handle_type,
    };
    VkPhysicalDeviceImageFormatInfo2 image_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &ext_info,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .type = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .flags = 0,
    };
    VkExternalImageFormatProperties ext_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &ext_props,
    };
    VkResult res = vkGetPhysicalDeviceImageFormatProperties2(phys, &image_info, &props);

    printf("external_image label=%s result=%d maxExtent=%ux%u features=0x%x exportFromImported=0x%x compatible=0x%x\n",
           label,
           res,
           res == VK_SUCCESS ? props.imageFormatProperties.maxExtent.width : 0,
           res == VK_SUCCESS ? props.imageFormatProperties.maxExtent.height : 0,
           res == VK_SUCCESS ? ext_props.externalMemoryProperties.externalMemoryFeatures : 0,
           res == VK_SUCCESS ? ext_props.externalMemoryProperties.exportFromImportedHandleTypes : 0,
           res == VK_SUCCESS ? ext_props.externalMemoryProperties.compatibleHandleTypes : 0);
}

int main(void)
{
    int rc = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    uint32_t phys_count = 0;
    uint32_t ext_count = 0;
    VkExtensionProperties *exts = NULL;

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-dmabuf-probe",
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

    CHECK_VK(vkEnumerateDeviceExtensionProperties(phys, NULL, &ext_count, NULL));
    exts = calloc(ext_count ? ext_count : 1, sizeof(*exts));
    if (!exts) {
        rc = 1;
        goto cleanup;
    }
    CHECK_VK(vkEnumerateDeviceExtensionProperties(phys, NULL, &ext_count, exts));
    printf("device_extension_count=%u\n", ext_count);
    print_bool_ext(exts, ext_count, "VK_KHR_external_memory");
    print_bool_ext(exts, ext_count, "VK_KHR_external_memory_fd");
    print_bool_ext(exts, ext_count, "VK_EXT_external_memory_dma_buf");
    print_bool_ext(exts, ext_count, "VK_EXT_image_drm_format_modifier");
    print_bool_ext(exts, ext_count, "VK_EXT_queue_family_foreign");
    print_bool_ext(exts, ext_count, "VK_KHR_bind_memory2");
    print_bool_ext(exts, ext_count, "VK_KHR_dedicated_allocation");
    print_bool_ext(exts, ext_count, "VK_KHR_swapchain");

    query_external_image(phys, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, "dma_buf_ext");
    query_external_image(phys, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, "opaque_fd");

cleanup:
    free(exts);
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    return rc;
}
