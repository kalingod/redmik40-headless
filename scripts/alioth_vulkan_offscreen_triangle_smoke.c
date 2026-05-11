#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define IMAGE_WIDTH 64u
#define IMAGE_HEIGHT 64u
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

int main(int argc, char **argv)
{
    const char *vert_path = argc > 1 ? argv[1] : "/data/experiments/alioth_vulkan_offscreen_triangle.vert.spv";
    const char *frag_path = argc > 2 ? argv[2] : "/data/experiments/alioth_vulkan_offscreen_triangle.frag.spv";
    const char *xrgb_out_path = argc > 3 ? argv[3] : NULL;
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
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkShaderModule vert_shader = VK_NULL_HANDLE;
    VkShaderModule frag_shader = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
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

    if (read_file(vert_path, &vert_spv, &vert_words) != 0 ||
        read_file(frag_path, &frag_spv, &frag_words) != 0) {
        fprintf(stderr, "failed to read SPIR-V inputs\n");
        return 1;
    }
    printf("vert_words=%zu frag_words=%zu\n", vert_words, frag_words);

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-vulkan-offscreen-triangle-smoke",
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
    if ((format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0 ||
        (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
        fprintf(stderr, "R8G8B8A8_UNORM lacks color attachment or transfer-src support\n");
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

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { .width = IMAGE_WIDTH, .height = IMAGE_HEIGHT, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    CHECK_VK(vkCreateImage(device, &image_info, NULL, &image));

    VkMemoryRequirements image_req;
    vkGetImageMemoryRequirements(device, image, &image_req);
    if (find_memory_type(phys, image_req.memoryTypeBits, 0,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         &image_memory_type, &image_memory_flags) != 0) {
        fprintf(stderr, "no memory type for color image, type_bits=0x%x\n", image_req.memoryTypeBits);
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

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
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
    CHECK_VK(vkCreateImageView(device, &view_info, NULL, &image_view));

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
            .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
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
        .pAttachments = &image_view,
        .width = IMAGE_WIDTH,
        .height = IMAGE_HEIGHT,
        .layers = 1,
    };
    CHECK_VK(vkCreateFramebuffer(device, &framebuffer_info, NULL, &framebuffer));

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
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

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
    uint32_t red_pixels = 0;
    uint32_t black_pixels = 0;
    uint32_t other_pixels = 0;
    uint64_t checksum = 1469598103934665603ull;
    for (uint32_t i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        const uint8_t *p = &bytes[i * BYTES_PER_PIXEL];

        for (uint32_t c = 0; c < BYTES_PER_PIXEL; c++) {
            checksum ^= p[c];
            checksum *= 1099511628211ull;
        }
        if (p[0] >= 200 && p[1] <= 30 && p[2] <= 30 && p[3] >= 200) {
            red_pixels++;
        } else if (p[0] <= 30 && p[1] <= 30 && p[2] <= 30 && p[3] >= 200) {
            black_pixels++;
        } else {
            if (other_pixels < 8) {
                fprintf(stderr, "other pixel index=%u rgba=%02x %02x %02x %02x\n",
                        i, p[0], p[1], p[2], p[3]);
            }
            other_pixels++;
        }
    }
    uint32_t center = ((IMAGE_HEIGHT / 2) * IMAGE_WIDTH + (IMAGE_WIDTH / 2)) * BYTES_PER_PIXEL;
    printf("center_pixel=%02x %02x %02x %02x\n",
           bytes[center + 0], bytes[center + 1], bytes[center + 2], bytes[center + 3]);
    printf("triangle_pixels red=%u black=%u other=%u total=%u checksum=0x%016" PRIx64 "\n",
           red_pixels, black_pixels, other_pixels, IMAGE_WIDTH * IMAGE_HEIGHT, checksum);
    if (red_pixels < 256 || black_pixels < 256 || other_pixels > 128) {
        fprintf(stderr, "triangle pixel validation failed\n");
        rc = 1;
        goto cleanup;
    }
    if (xrgb_out_path) {
        FILE *out = fopen(xrgb_out_path, "wb");
        if (!out) {
            perror(xrgb_out_path);
            rc = 1;
            goto cleanup;
        }
        for (uint32_t i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
            const uint8_t *p = &bytes[i * BYTES_PER_PIXEL];
            uint32_t xrgb = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];

            if (fwrite(&xrgb, sizeof(xrgb), 1, out) != 1) {
                perror("write xrgb");
                fclose(out);
                rc = 1;
                goto cleanup;
            }
        }
        if (fclose(out) != 0) {
            perror("close xrgb");
            rc = 1;
            goto cleanup;
        }
        printf("xrgb_dump=%s bytes=%u\n", xrgb_out_path, IMAGE_WIDTH * IMAGE_HEIGHT * BYTES_PER_PIXEL);
    }
    printf("offscreen_triangle_smoke=PASS queue_family=%u\n", queue_family);

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
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device, pipeline, NULL);
        if (pipeline_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device, pipeline_layout, NULL);
        if (frag_shader != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, frag_shader, NULL);
        if (vert_shader != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, vert_shader, NULL);
        if (framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(device, framebuffer, NULL);
        if (render_pass != VK_NULL_HANDLE)
            vkDestroyRenderPass(device, render_pass, NULL);
        if (image_view != VK_NULL_HANDLE)
            vkDestroyImageView(device, image_view, NULL);
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(device, image, NULL);
        if (image_memory != VK_NULL_HANDLE)
            vkFreeMemory(device, image_memory, NULL);
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, NULL);
    free(vert_spv);
    free(frag_spv);
    return rc;
}
