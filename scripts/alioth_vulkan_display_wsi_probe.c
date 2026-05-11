#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

static bool has_extension(const VkExtensionProperties *exts, uint32_t count, const char *name)
{
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, name) == 0)
            return true;
    }
    return false;
}

static void print_vk_result(const char *label, VkResult res)
{
    printf("%s=%d\n", label, res);
}

int main(void)
{
    const char *wanted_instance[] = {
        "VK_KHR_surface",
        "VK_KHR_display",
        "VK_KHR_get_display_properties2",
        "VK_EXT_direct_mode_display",
        "VK_EXT_acquire_drm_display",
    };
    const char *enabled_instance[5];
    uint32_t enabled_instance_count = 0;
    uint32_t instance_ext_count = 0;
    VkExtensionProperties *instance_exts = NULL;
    VkInstance instance = VK_NULL_HANDLE;
    uint32_t phys_count = 0;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkResult res;
    int rc = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    res = vkEnumerateInstanceExtensionProperties(NULL, &instance_ext_count, NULL);
    print_vk_result("vkEnumerateInstanceExtensionProperties_count", res);
    if (res != VK_SUCCESS)
        return 1;
    instance_exts = calloc(instance_ext_count ? instance_ext_count : 1, sizeof(*instance_exts));
    if (!instance_exts)
        return 1;
    res = vkEnumerateInstanceExtensionProperties(NULL, &instance_ext_count, instance_exts);
    print_vk_result("vkEnumerateInstanceExtensionProperties_list", res);
    if (res != VK_SUCCESS) {
        rc = 1;
        goto cleanup;
    }

    printf("instance_extension_count=%u\n", instance_ext_count);
    for (uint32_t i = 0; i < sizeof(wanted_instance) / sizeof(wanted_instance[0]); i++) {
        bool present = has_extension(instance_exts, instance_ext_count, wanted_instance[i]);
        printf("instance_ext %-34s %s\n", wanted_instance[i], present ? "yes" : "no");
        if (present &&
            (strcmp(wanted_instance[i], "VK_KHR_surface") == 0 ||
             strcmp(wanted_instance[i], "VK_KHR_display") == 0)) {
            enabled_instance[enabled_instance_count++] = wanted_instance[i];
        }
    }
    if (!has_extension(instance_exts, instance_ext_count, "VK_KHR_display")) {
        printf("display_probe=SKIP no VK_KHR_display\n");
        goto cleanup;
    }

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-display-wsi-probe",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
        .enabledExtensionCount = enabled_instance_count,
        .ppEnabledExtensionNames = enabled_instance,
    };
    printf("enabled_instance_count=%u\n", enabled_instance_count);
    for (uint32_t i = 0; i < enabled_instance_count; i++)
        printf("enabled_instance[%u]=%s\n", i, enabled_instance[i]);
    printf("calling_vkCreateInstance\n");
    res = vkCreateInstance(&instance_info, NULL, &instance);
    print_vk_result("vkCreateInstance", res);
    if (res != VK_SUCCESS) {
        rc = 1;
        goto cleanup;
    }

    printf("calling_vkEnumeratePhysicalDevices_count\n");
    res = vkEnumeratePhysicalDevices(instance, &phys_count, NULL);
    print_vk_result("vkEnumeratePhysicalDevices_count", res);
    if (res != VK_SUCCESS || phys_count == 0) {
        rc = 1;
        goto cleanup;
    }
    VkPhysicalDevice *phys_list = calloc(phys_count, sizeof(*phys_list));
    if (!phys_list) {
        rc = 1;
        goto cleanup;
    }
    printf("calling_vkEnumeratePhysicalDevices_list\n");
    res = vkEnumeratePhysicalDevices(instance, &phys_count, phys_list);
    print_vk_result("vkEnumeratePhysicalDevices_list", res);
    if (res != VK_SUCCESS) {
        free(phys_list);
        rc = 1;
        goto cleanup;
    }
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

    uint32_t device_ext_count = 0;
    res = vkEnumerateDeviceExtensionProperties(phys, NULL, &device_ext_count, NULL);
    print_vk_result("vkEnumerateDeviceExtensionProperties_count", res);
    if (res == VK_SUCCESS) {
        VkExtensionProperties *device_exts = calloc(device_ext_count ? device_ext_count : 1, sizeof(*device_exts));
        if (device_exts) {
            res = vkEnumerateDeviceExtensionProperties(phys, NULL, &device_ext_count, device_exts);
            print_vk_result("vkEnumerateDeviceExtensionProperties_list", res);
            if (res == VK_SUCCESS) {
                printf("device_extension_count=%u\n", device_ext_count);
                printf("device_ext %-34s %s\n", "VK_KHR_swapchain",
                       has_extension(device_exts, device_ext_count, "VK_KHR_swapchain") ? "yes" : "no");
                printf("device_ext %-34s %s\n", "VK_EXT_image_drm_format_modifier",
                       has_extension(device_exts, device_ext_count, "VK_EXT_image_drm_format_modifier") ? "yes" : "no");
            }
            free(device_exts);
        }
    }

    printf("loading_khr_display_pfns\n");
    PFN_vkGetPhysicalDeviceDisplayPropertiesKHR get_display_props =
        (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR");
    PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR get_plane_props =
        (PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
    PFN_vkGetDisplayModePropertiesKHR get_mode_props =
        (PFN_vkGetDisplayModePropertiesKHR)vkGetInstanceProcAddr(instance, "vkGetDisplayModePropertiesKHR");
    PFN_vkGetDisplayPlaneSupportedDisplaysKHR get_plane_supported =
        (PFN_vkGetDisplayPlaneSupportedDisplaysKHR)vkGetInstanceProcAddr(instance, "vkGetDisplayPlaneSupportedDisplaysKHR");

    printf("pfn_vkGetPhysicalDeviceDisplayPropertiesKHR=%s\n", get_display_props ? "yes" : "no");
    printf("pfn_vkGetPhysicalDeviceDisplayPlanePropertiesKHR=%s\n", get_plane_props ? "yes" : "no");
    printf("pfn_vkGetDisplayModePropertiesKHR=%s\n", get_mode_props ? "yes" : "no");
    printf("pfn_vkGetPhysicalDeviceDisplayPlaneSupportedDisplaysKHR=%s\n", get_plane_supported ? "yes" : "no");
    if (!get_display_props || !get_plane_props || !get_mode_props || !get_plane_supported) {
        rc = 1;
        goto cleanup;
    }

    uint32_t display_count = 0;
    printf("calling_vkGetPhysicalDeviceDisplayPropertiesKHR_count\n");
    res = get_display_props(phys, &display_count, NULL);
    print_vk_result("vkGetPhysicalDeviceDisplayPropertiesKHR_count", res);
    printf("display_count=%u\n", display_count);
    if (res != VK_SUCCESS) {
        rc = 1;
        goto cleanup;
    }
    VkDisplayPropertiesKHR *displays = calloc(display_count ? display_count : 1, sizeof(*displays));
    if (!displays) {
        rc = 1;
        goto cleanup;
    }
    printf("calling_vkGetPhysicalDeviceDisplayPropertiesKHR_list\n");
    res = get_display_props(phys, &display_count, displays);
    print_vk_result("vkGetPhysicalDeviceDisplayPropertiesKHR_list", res);
    if (res != VK_SUCCESS) {
        free(displays);
        rc = 1;
        goto cleanup;
    }
    for (uint32_t i = 0; i < display_count; i++) {
        printf("display[%u] name=%s physical=%ux%u physicalResolution=%ux%u transforms=0x%x planeReorder=%s persistent=%s\n",
               i,
               displays[i].displayName ? displays[i].displayName : "(null)",
               displays[i].physicalDimensions.width,
               displays[i].physicalDimensions.height,
               displays[i].physicalResolution.width,
               displays[i].physicalResolution.height,
               displays[i].supportedTransforms,
               displays[i].planeReorderPossible ? "yes" : "no",
               displays[i].persistentContent ? "yes" : "no");

        uint32_t mode_count = 0;
        printf("calling_vkGetDisplayModePropertiesKHR_count display=%u\n", i);
        res = get_mode_props(phys, displays[i].display, &mode_count, NULL);
        printf("display[%u]_mode_count_result=%d count=%u\n", i, res, mode_count);
        if (res == VK_SUCCESS && mode_count) {
            VkDisplayModePropertiesKHR *modes = calloc(mode_count, sizeof(*modes));
            if (modes) {
                printf("calling_vkGetDisplayModePropertiesKHR_list display=%u\n", i);
                res = get_mode_props(phys, displays[i].display, &mode_count, modes);
                printf("display[%u]_mode_list_result=%d count=%u\n", i, res, mode_count);
                if (res == VK_SUCCESS) {
                    for (uint32_t m = 0; m < mode_count; m++) {
                        printf("display[%u].mode[%u] visible=%ux%u refresh=%u\n",
                               i, m,
                               modes[m].parameters.visibleRegion.width,
                               modes[m].parameters.visibleRegion.height,
                               modes[m].parameters.refreshRate);
                    }
                }
                free(modes);
            }
        }
    }

    uint32_t plane_count = 0;
    printf("calling_vkGetPhysicalDeviceDisplayPlanePropertiesKHR_count\n");
    res = get_plane_props(phys, &plane_count, NULL);
    print_vk_result("vkGetPhysicalDeviceDisplayPlanePropertiesKHR_count", res);
    printf("plane_count=%u\n", plane_count);
    if (res == VK_SUCCESS && plane_count) {
        VkDisplayPlanePropertiesKHR *planes = calloc(plane_count, sizeof(*planes));
        if (planes) {
            printf("calling_vkGetPhysicalDeviceDisplayPlanePropertiesKHR_list\n");
            res = get_plane_props(phys, &plane_count, planes);
            print_vk_result("vkGetPhysicalDeviceDisplayPlanePropertiesKHR_list", res);
            if (res == VK_SUCCESS) {
                for (uint32_t p = 0; p < plane_count; p++) {
                    printf("plane[%u] currentDisplay=%p currentStackIndex=%u\n",
                           p, (void *)planes[p].currentDisplay, planes[p].currentStackIndex);
                    uint32_t supported_count = 0;
                    printf("calling_vkGetDisplayPlaneSupportedDisplaysKHR_count plane=%u\n", p);
                    res = get_plane_supported(phys, p, &supported_count, NULL);
                    printf("plane[%u]_supported_count_result=%d count=%u\n", p, res, supported_count);
                    if (res == VK_SUCCESS && supported_count) {
                        VkDisplayKHR *supported = calloc(supported_count, sizeof(*supported));
                        if (supported) {
                            printf("calling_vkGetDisplayPlaneSupportedDisplaysKHR_list plane=%u\n", p);
                            res = get_plane_supported(phys, p, &supported_count, supported);
                            printf("plane[%u]_supported_list_result=%d count=%u\n", p, res, supported_count);
                            free(supported);
                        }
                    }
                }
            }
            free(planes);
        }
    }
    free(displays);

    printf("display_wsi_probe=PASS\n");

cleanup:
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    free(instance_exts);
    return rc;
}
