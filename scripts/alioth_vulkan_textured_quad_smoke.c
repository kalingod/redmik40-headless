#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define IMAGE_WIDTH 64u
#define IMAGE_HEIGHT 64u
#define TEXTURE_WIDTH 64u
#define TEXTURE_HEIGHT 64u
#define STRIPE_WIDTH 8u
#define BYTES_PER_PIXEL 4u

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
        fprintf(stderr, "invalid SPIR-V size: %ld\n", size);
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

static int create_buffer(VkPhysicalDevice phys, VkDevice device,
                         VkDeviceSize size, VkBufferUsageFlags usage,
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
    VkResult res = vkCreateBuffer(device, &buffer_info, NULL, buffer);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkCreateBuffer failed: %d\n", res);
        return -1;
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, *buffer, &req);
    uint32_t memory_type = UINT32_MAX;
    if (find_memory_type(phys, req.memoryTypeBits, required, preferred,
                         &memory_type, memory_flags) != 0) {
        fprintf(stderr, "no memory type for buffer type_bits=0x%x\n", req.memoryTypeBits);
        return -1;
    }
    printf("buffer size=%" PRIu64 " req_size=%" PRIu64 " type=%u flags=0x%x\n",
           (uint64_t)size, (uint64_t)req.size, memory_type, *memory_flags);

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memory_type,
    };
    res = vkAllocateMemory(device, &alloc, NULL, memory);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkAllocateMemory(buffer) failed: %d\n", res);
        return -1;
    }
    res = vkBindBufferMemory(device, *buffer, *memory, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkBindBufferMemory failed: %d\n", res);
        return -1;
    }

    return 0;
}

static int create_image(VkPhysicalDevice phys, VkDevice device,
                        uint32_t width, uint32_t height,
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
    VkResult res = vkCreateImage(device, &image_info, NULL, image);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkCreateImage failed: %d\n", res);
        return -1;
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, *image, &req);
    uint32_t memory_type = UINT32_MAX;
    if (find_memory_type(phys, req.memoryTypeBits, 0,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         &memory_type, memory_flags) != 0) {
        fprintf(stderr, "no memory type for image type_bits=0x%x\n", req.memoryTypeBits);
        return -1;
    }
    printf("image %ux%u usage=0x%x req_size=%" PRIu64 " type=%u flags=0x%x\n",
           width, height, usage, (uint64_t)req.size, memory_type, *memory_flags);

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memory_type,
    };
    res = vkAllocateMemory(device, &alloc, NULL, memory);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkAllocateMemory(image) failed: %d\n", res);
        return -1;
    }
    res = vkBindImageMemory(device, *image, *memory, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkBindImageMemory failed: %d\n", res);
        return -1;
    }

    return 0;
}

static uint8_t expected_component(uint32_t x, uint32_t component)
{
    uint32_t tex_x = (x * TEXTURE_WIDTH) / IMAGE_WIDTH;
    uint32_t stripe = (tex_x / STRIPE_WIDTH) & 1u;

    if (component == 3)
        return 0xff;
    if (stripe == 0)
        return component == 0 ? 0xff : 0x00;
    return component == 1 ? 0xff : 0x00;
}

static void fill_texture(uint8_t *dst)
{
    for (uint32_t y = 0; y < TEXTURE_HEIGHT; y++) {
        for (uint32_t x = 0; x < TEXTURE_WIDTH; x++) {
            uint8_t *px = dst + ((size_t)y * TEXTURE_WIDTH + x) * BYTES_PER_PIXEL;

            px[0] = expected_component(x, 0);
            px[1] = expected_component(x, 1);
            px[2] = expected_component(x, 2);
            px[3] = expected_component(x, 3);
        }
    }
}

int main(int argc, char **argv)
{
    const char *vert_path = argc > 1 ? argv[1] : "/data/experiments/alioth_vulkan_textured_quad.vert.spv";
    const char *frag_path = argc > 2 ? argv[2] : "/data/experiments/alioth_vulkan_textured_quad.frag.spv";
    const VkDeviceSize texture_size = TEXTURE_WIDTH * TEXTURE_HEIGHT * BYTES_PER_PIXEL;
    const VkDeviceSize readback_size = IMAGE_WIDTH * IMAGE_HEIGHT * BYTES_PER_PIXEL;
    int rc = 0;
    uint32_t *vert_spv = NULL;
    uint32_t *frag_spv = NULL;
    size_t vert_words = 0;
    size_t frag_words = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    VkImage texture = VK_NULL_HANDLE;
    VkDeviceMemory texture_memory = VK_NULL_HANDLE;
    VkImageView texture_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkImage color = VK_NULL_HANDLE;
    VkDeviceMemory color_memory = VK_NULL_HANDLE;
    VkImageView color_view = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool = VK_NULL_HANDLE;
    VkDescriptorSet desc_set = VK_NULL_HANDLE;
    VkShaderModule vert_shader = VK_NULL_HANDLE;
    VkShaderModule frag_shader = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory readback_memory = VK_NULL_HANDLE;
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    void *staging_mapped = NULL;
    void *readback_mapped = NULL;
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t queue_family = UINT32_MAX;
    uint32_t staging_memory_type = UINT32_MAX;
    uint32_t readback_memory_type = UINT32_MAX;
    VkMemoryPropertyFlags staging_memory_flags = 0;
    VkMemoryPropertyFlags readback_memory_flags = 0;
    VkMemoryPropertyFlags texture_memory_flags = 0;
    VkMemoryPropertyFlags color_memory_flags = 0;

    (void)staging_memory_type;
    (void)readback_memory_type;

    if (read_file(vert_path, &vert_spv, &vert_words) != 0 ||
        read_file(frag_path, &frag_spv, &frag_words) != 0) {
        fprintf(stderr, "failed to read SPIR-V inputs\n");
        return 1;
    }
    printf("vert_words=%zu frag_words=%zu\n", vert_words, frag_words);

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-textured-quad-smoke",
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
    if ((format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0 ||
        (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0 ||
        (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0 ||
        (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
        fprintf(stderr, "R8G8B8A8_UNORM lacks sampled/transfer/color support\n");
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
            (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            queue_family = i;
        }
    }
    free(queues);
    if (queue_family == UINT32_MAX) {
        fprintf(stderr, "no graphics queue family\n");
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

    if (create_buffer(phys, device, texture_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      0, &staging, &staging_memory, &staging_memory_flags) != 0) {
        rc = 1;
        goto cleanup;
    }
    CHECK_VK(vkMapMemory(device, staging_memory, 0, texture_size, 0, &staging_mapped));
    fill_texture(staging_mapped);
    printf("texture_pattern=vertical_stripes stripe_width=%u bytes=%" PRIu64 "\n",
           STRIPE_WIDTH, (uint64_t)texture_size);

    if (create_image(phys, device, TEXTURE_WIDTH, TEXTURE_HEIGHT,
                     VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     &texture, &texture_memory, &texture_memory_flags) != 0) {
        rc = 1;
        goto cleanup;
    }

    if (create_image(phys, device, IMAGE_WIDTH, IMAGE_HEIGHT,
                     VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     &color, &color_memory, &color_memory_flags) != 0) {
        rc = 1;
        goto cleanup;
    }

    VkImageViewCreateInfo texture_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture,
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
    CHECK_VK(vkCreateImageView(device, &texture_view_info, NULL, &texture_view));

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .compareEnable = VK_FALSE,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    CHECK_VK(vkCreateSampler(device, &sampler_info, NULL, &sampler));

    VkImageViewCreateInfo color_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = color,
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
    CHECK_VK(vkCreateImageView(device, &color_view_info, NULL, &color_view));

    VkAttachmentDescription attachment = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
            .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
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
    CHECK_VK(vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass));

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &color_view,
        .width = IMAGE_WIDTH,
        .height = IMAGE_HEIGHT,
        .layers = 1,
    };
    CHECK_VK(vkCreateFramebuffer(device, &framebuffer_info, NULL, &framebuffer));

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
    CHECK_VK(vkCreateDescriptorSetLayout(device, &desc_layout_info, NULL, &desc_layout));

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
    CHECK_VK(vkCreateDescriptorPool(device, &desc_pool_info, NULL, &desc_pool));

    VkDescriptorSetAllocateInfo desc_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_layout,
    };
    CHECK_VK(vkAllocateDescriptorSets(device, &desc_alloc, &desc_set));

    VkDescriptorImageInfo image_info = {
        .sampler = sampler,
        .imageView = texture_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = desc_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

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
    CHECK_VK(vkCreateShaderModule(device, &vert_info, NULL, &vert_shader));
    CHECK_VK(vkCreateShaderModule(device, &frag_info, NULL, &frag_shader));

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader,
            .pName = "main",
        },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)IMAGE_WIDTH,
        .height = (float)IMAGE_HEIGHT,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width = IMAGE_WIDTH, .height = IMAGE_HEIGHT },
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
        .pSetLayouts = &desc_layout,
    };
    CHECK_VK(vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout));

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
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };
    CHECK_VK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));

    if (create_buffer(phys, device, readback_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      0, &readback, &readback_memory, &readback_memory_flags) != 0) {
        rc = 1;
        goto cleanup;
    }
    CHECK_VK(vkMapMemory(device, readback_memory, 0, readback_size, 0, &readback_mapped));
    memset(readback_mapped, 0xa5, (size_t)readback_size);

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

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageMemoryBarrier texture_to_dst = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, NULL, 0, NULL, 1, &texture_to_dst);

    VkBufferImageCopy texture_copy = {
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
    vkCmdCopyBufferToImage(cmd, staging, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &texture_copy);

    VkImageMemoryBarrier texture_to_sample = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, NULL, 0, NULL, 1, &texture_to_sample);

    VkClearValue clear = {
        .color = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} },
    };
    VkRenderPassBeginInfo render_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = { .x = 0, .y = 0 },
            .extent = { .width = IMAGE_WIDTH, .height = IMAGE_HEIGHT },
        },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    vkCmdBeginRenderPass(cmd, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            0, 1, &desc_set, 0, NULL);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    VkBufferImageCopy readback_copy = {
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
    vkCmdCopyImageToBuffer(cmd, color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback, 1, &readback_copy);

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

    const uint8_t *bytes = readback_mapped;
    uint32_t red_pixels = 0;
    uint32_t green_pixels = 0;
    uint32_t other_pixels = 0;
    uint64_t checksum = 1469598103934665603ull;

    for (uint32_t i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        const uint8_t *p = &bytes[i * BYTES_PER_PIXEL];

        for (uint32_t c = 0; c < BYTES_PER_PIXEL; c++) {
            checksum ^= p[c];
            checksum *= 1099511628211ull;
        }
        if (p[0] == 0xff && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0xff) {
            red_pixels++;
        } else if (p[0] == 0x00 && p[1] == 0xff && p[2] == 0x00 && p[3] == 0xff) {
            green_pixels++;
        } else {
            if (other_pixels < 8) {
                fprintf(stderr, "other pixel index=%u rgba=%02x %02x %02x %02x\n",
                        i, p[0], p[1], p[2], p[3]);
            }
            other_pixels++;
        }
    }

    const uint32_t sample_points[][2] = {
        {4, 4},
        {12, 4},
        {20, 32},
        {28, 32},
        {36, 48},
        {44, 48},
        {52, 60},
        {60, 60},
    };
    uint32_t sample_pass = 0;
    for (uint32_t i = 0; i < sizeof(sample_points) / sizeof(sample_points[0]); i++) {
        uint32_t x = sample_points[i][0];
        uint32_t y = sample_points[i][1];
        const uint8_t *p = bytes + ((size_t)y * IMAGE_WIDTH + x) * BYTES_PER_PIXEL;
        uint8_t exp[4] = {
            expected_component(x, 0),
            expected_component(x, 1),
            expected_component(x, 2),
            expected_component(x, 3),
        };
        int ok = memcmp(p, exp, 4) == 0;

        printf("sample[%u] x=%u y=%u rgba=%02x %02x %02x %02x expected=%02x %02x %02x %02x ok=%s\n",
               i, x, y, p[0], p[1], p[2], p[3],
               exp[0], exp[1], exp[2], exp[3], ok ? "yes" : "no");
        if (ok)
            sample_pass++;
    }

    printf("texture_pixels red=%u green=%u other=%u total=%u checksum=0x%016" PRIx64 "\n",
           red_pixels, green_pixels, other_pixels, IMAGE_WIDTH * IMAGE_HEIGHT, checksum);
    printf("texture_sample_pass=%u/%zu\n",
           sample_pass, sizeof(sample_points) / sizeof(sample_points[0]));
    if (red_pixels != 2048 || green_pixels != 2048 || other_pixels != 0 ||
        sample_pass != sizeof(sample_points) / sizeof(sample_points[0])) {
        fprintf(stderr, "sampled texture validation failed\n");
        rc = 1;
        goto cleanup;
    }

    printf("textured_quad_smoke=PASS queue_family=%u\n", queue_family);

cleanup:
    if (device != VK_NULL_HANDLE) {
        if (readback_mapped)
            vkUnmapMemory(device, readback_memory);
        if (staging_mapped)
            vkUnmapMemory(device, staging_memory);
        if (cmd_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, cmd_pool, NULL);
        if (readback != VK_NULL_HANDLE)
            vkDestroyBuffer(device, readback, NULL);
        if (readback_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, readback_memory, NULL);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device, pipeline, NULL);
        if (pipeline_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device, pipeline_layout, NULL);
        if (frag_shader != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, frag_shader, NULL);
        if (vert_shader != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, vert_shader, NULL);
        if (desc_pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device, desc_pool, NULL);
        if (desc_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, desc_layout, NULL);
        if (framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(device, framebuffer, NULL);
        if (render_pass != VK_NULL_HANDLE)
            vkDestroyRenderPass(device, render_pass, NULL);
        if (color_view != VK_NULL_HANDLE)
            vkDestroyImageView(device, color_view, NULL);
        if (color != VK_NULL_HANDLE)
            vkDestroyImage(device, color, NULL);
        if (color_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, color_memory, NULL);
        if (sampler != VK_NULL_HANDLE)
            vkDestroySampler(device, sampler, NULL);
        if (texture_view != VK_NULL_HANDLE)
            vkDestroyImageView(device, texture_view, NULL);
        if (texture != VK_NULL_HANDLE)
            vkDestroyImage(device, texture, NULL);
        if (texture_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, texture_memory, NULL);
        if (staging != VK_NULL_HANDLE)
            vkDestroyBuffer(device, staging, NULL);
        if (staging_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, staging_memory, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    free(vert_spv);
    free(frag_spv);
    return rc;
}
