#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define EXPECTED_VALUE 0x5a17c0deu

#define CHECK_VK(expr) do { \
    VkResult _res = (expr); \
    if (_res != VK_SUCCESS) { \
        fprintf(stderr, "%s failed: %d\n", #expr, _res); \
        rc = 1; \
        goto cleanup; \
    } \
} while (0)

static int read_file(const char *path, uint32_t **words, size_t *word_count)
{
    FILE *fp = fopen(path, "rb");
    long size;
    uint32_t *buf;

    if (!fp) {
        perror(path);
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    size = ftell(fp);
    if (size <= 0 || (size % 4) != 0) {
        fclose(fp);
        fprintf(stderr, "invalid SPIR-V size: %ld\n", size);
        return -1;
    }
    rewind(fp);
    buf = malloc((size_t)size);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *words = buf;
    *word_count = (size_t)size / 4;
    return 0;
}

static int find_memory_type(VkPhysicalDevice phys, uint32_t type_bits,
                            VkMemoryPropertyFlags required, uint32_t *type_index)
{
    VkPhysicalDeviceMemoryProperties props;

    vkGetPhysicalDeviceMemoryProperties(phys, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & required) == required) {
            *type_index = i;
            return 0;
        }
    }
    return -1;
}

int main(int argc, char **argv)
{
    const char *spv_path = argc > 1 ? argv[1] : "/data/experiments/alioth_vulkan_compute_smoke.spv";
    int rc = 0;
    uint32_t *spv = NULL;
    size_t spv_words = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkShaderModule shader = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool = VK_NULL_HANDLE;
    VkDescriptorSet desc_set = VK_NULL_HANDLE;
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    uint32_t queue_family = UINT32_MAX;
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t memory_type = UINT32_MAX;
    void *mapped = NULL;

    if (read_file(spv_path, &spv, &spv_words) != 0) {
        fprintf(stderr, "failed to read SPIR-V: %s\n", spv_path);
        return 1;
    }
    printf("spirv_words=%zu\n", spv_words);

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-compute-smoke",
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

    vkGetPhysicalDeviceQueueFamilyProperties(phys, &queue_count, NULL);
    VkQueueFamilyProperties *queues = calloc(queue_count, sizeof(*queues));
    if (!queues) {
        rc = 1;
        goto cleanup;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &queue_count, queues);
    for (uint32_t i = 0; i < queue_count; i++) {
        printf("queue_family[%u] flags=0x%x count=%u\n", i, queues[i].queueFlags, queues[i].queueCount);
        if (queue_family == UINT32_MAX && queues[i].queueCount > 0 &&
            (queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            queue_family = i;
        }
    }
    free(queues);
    if (queue_family == UINT32_MAX) {
        fprintf(stderr, "no compute queue family\n");
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

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(uint32_t),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    CHECK_VK(vkCreateBuffer(device, &buffer_info, NULL, &buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buffer, &req);
    if (find_memory_type(phys, req.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         &memory_type) != 0) {
        fprintf(stderr, "no host-visible coherent memory type for buffer, type_bits=0x%x\n", req.memoryTypeBits);
        rc = 1;
        goto cleanup;
    }
    printf("buffer_size=%" PRIu64 " memory_type=%u\n", (uint64_t)req.size, memory_type);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memory_type,
    };
    CHECK_VK(vkAllocateMemory(device, &alloc_info, NULL, &memory));
    CHECK_VK(vkBindBufferMemory(device, buffer, memory, 0));
    CHECK_VK(vkMapMemory(device, memory, 0, sizeof(uint32_t), 0, &mapped));
    *(uint32_t *)mapped = 0;

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    CHECK_VK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &set_layout));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
    };
    CHECK_VK(vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout));

    VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spv_words * sizeof(uint32_t),
        .pCode = spv,
    };
    CHECK_VK(vkCreateShaderModule(device, &shader_info, NULL, &shader));

    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader,
        .pName = "main",
    };
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = pipeline_layout,
    };
    CHECK_VK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    CHECK_VK(vkCreateDescriptorPool(device, &desc_pool_info, NULL, &desc_pool));

    VkDescriptorSetAllocateInfo desc_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout,
    };
    CHECK_VK(vkAllocateDescriptorSets(device, &desc_alloc, &desc_set));

    VkDescriptorBufferInfo buffer_desc = {
        .buffer = buffer,
        .offset = 0,
        .range = sizeof(uint32_t),
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = desc_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_desc,
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_set, 0, NULL);
    vkCmdDispatch(cmd, 1, 1, 1);
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                         0, 1, &barrier, 0, NULL, 0, NULL);
    CHECK_VK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    CHECK_VK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    CHECK_VK(vkQueueWaitIdle(queue));
    CHECK_VK(vkDeviceWaitIdle(device));

    uint32_t value = *(uint32_t *)mapped;
    printf("compute_value=0x%08x expected=0x%08x\n", value, EXPECTED_VALUE);
    if (value != EXPECTED_VALUE) {
        fprintf(stderr, "compute value mismatch\n");
        rc = 1;
        goto cleanup;
    }
    printf("compute_smoke=PASS queue_family=%u\n", queue_family);

cleanup:
    if (device != VK_NULL_HANDLE) {
        if (mapped)
            vkUnmapMemory(device, memory);
        if (cmd_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, cmd_pool, NULL);
        if (desc_pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device, desc_pool, NULL);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device, pipeline, NULL);
        if (shader != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, shader, NULL);
        if (pipeline_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device, pipeline_layout, NULL);
        if (set_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, set_layout, NULL);
        if (buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(device, buffer, NULL);
        if (memory != VK_NULL_HANDLE)
            vkFreeMemory(device, memory, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    free(spv);
    return rc;
}
