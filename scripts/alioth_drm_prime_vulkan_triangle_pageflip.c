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
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <linux/dma-buf.h>
#include <vulkan/vulkan.h>

struct drm_target {
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
};

struct drm_buffer {
    uint32_t handle;
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint64_t size;
    void *map;
    int prime_fd;
};

struct vk_ctx {
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family;
    VkCommandPool command_pool;
    PFN_vkGetMemoryFdPropertiesKHR get_memory_fd_props;
    VkRenderPass render_pass;
    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
};

struct imported_image {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkFramebuffer framebuffer;
    uint32_t width;
    uint32_t height;
    VkImageLayout layout;
    bool initialized;
};

struct flip_state {
    int waiting;
    unsigned int seen;
};

static int vk_ok(VkResult res, const char *expr)
{
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s failed: %d\n", expr, res);
        return -1;
    }
    return 0;
}

static int read_file(const char *path, uint32_t **words, size_t *word_count)
{
    FILE *fp = fopen(path, "rb");
    long size;
    uint32_t *buf;

    if (!fp) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    size = ftell(fp);
    if (size <= 0 || (size % 4) != 0) {
        fprintf(stderr, "invalid SPIR-V size for %s: %ld\n", path, size);
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
        fprintf(stderr, "read %s failed\n", path);
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    *words = buf;
    *word_count = (size_t)size / 4;
    return 0;
}

static uint32_t choose_crtc(drmModeRes *res, drmModeEncoder *enc)
{
    for (int i = 0; i < res->count_crtcs; i++) {
        if (enc->possible_crtcs & (1u << i))
            return res->crtcs[i];
    }
    return 0;
}

static int find_target(int fd, struct drm_target *target)
{
    drmModeRes *res = drmModeGetResources(fd);

    if (!res) {
        fprintf(stderr, "drmModeGetResources failed: %s\n", strerror(errno));
        return -1;
    }

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        uint32_t crtc_id = 0;

        if (!conn)
            continue;
        if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
            drmModeFreeConnector(conn);
            continue;
        }

        if (conn->encoder_id) {
            drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc) {
                crtc_id = enc->crtc_id ? enc->crtc_id : choose_crtc(res, enc);
                drmModeFreeEncoder(enc);
            }
        }
        if (!crtc_id) {
            for (int j = 0; j < conn->count_encoders; j++) {
                drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[j]);
                if (!enc)
                    continue;
                crtc_id = enc->crtc_id ? enc->crtc_id : choose_crtc(res, enc);
                drmModeFreeEncoder(enc);
                if (crtc_id)
                    break;
            }
        }
        if (!crtc_id && res->count_crtcs > 0)
            crtc_id = res->crtcs[0];

        if (crtc_id) {
            target->connector_id = conn->connector_id;
            target->crtc_id = crtc_id;
            target->mode = conn->modes[0];
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            return 0;
        }

        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
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

static void describe_prime_fd(int fd)
{
    char proc_path[64];
    char target[256];
    ssize_t len;

    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
    len = readlink(proc_path, target, sizeof(target) - 1);
    if (len >= 0) {
        target[len] = '\0';
        printf("prime_fd=%d target=%s\n", fd, target);
    }
}

static int create_drm_buffer(int fd, const struct drm_target *target,
                             struct drm_buffer *buf)
{
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};

    memset(buf, 0, sizeof(*buf));
    buf->prime_fd = -1;

    create_req.width = target->mode.hdisplay;
    create_req.height = target->mode.vdisplay;
    create_req.bpp = 32;
    if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) != 0) {
        fprintf(stderr, "CREATE_DUMB failed: %s\n", strerror(errno));
        return -1;
    }

    buf->handle = create_req.handle;
    buf->width = create_req.width;
    buf->height = create_req.height;
    buf->pitch = create_req.pitch;
    buf->size = create_req.size;

    map_req.handle = buf->handle;
    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) != 0) {
        fprintf(stderr, "MAP_DUMB failed: %s\n", strerror(errno));
        return -1;
    }

    buf->map = mmap(NULL, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, map_req.offset);
    if (buf->map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        buf->map = NULL;
        return -1;
    }

    if (drmPrimeHandleToFD(fd, buf->handle, DRM_CLOEXEC, &buf->prime_fd) != 0) {
        fprintf(stderr, "drmPrimeHandleToFD failed: %s\n", strerror(errno));
        return -1;
    }

    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE, "cpu_zero_start");
    memset(buf->map, 0, buf->size);
    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE, "cpu_zero_end");

    uint32_t handles[4] = {buf->handle, 0, 0, 0};
    uint32_t pitches[4] = {buf->pitch, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    if (drmModeAddFB2(fd, buf->width, buf->height, DRM_FORMAT_XRGB8888,
                      handles, pitches, offsets, &buf->fb_id, 0) != 0) {
        fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(errno));
        return -1;
    }

    printf("drm_buffer handle=%u fb=%u width=%u height=%u pitch=%u size=%llu\n",
           buf->handle, buf->fb_id, buf->width, buf->height, buf->pitch,
           (unsigned long long)buf->size);
    describe_prime_fd(buf->prime_fd);
    return 0;
}

static void destroy_drm_buffer(int fd, struct drm_buffer *buf)
{
    if (buf->fb_id)
        drmModeRmFB(fd, buf->fb_id);
    if (buf->map)
        munmap(buf->map, buf->size);
    if (buf->prime_fd >= 0)
        close(buf->prime_fd);
    if (buf->handle) {
        struct drm_mode_destroy_dumb destroy_req = { .handle = buf->handle };
        ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
}

static bool has_extension(const VkExtensionProperties *exts, uint32_t count,
                          const char *name)
{
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, name) == 0)
            return true;
    }
    return false;
}

static int find_memory_type(VkPhysicalDevice phys, uint32_t type_bits,
                            uint32_t *type_index,
                            VkMemoryPropertyFlags *type_flags)
{
    VkPhysicalDeviceMemoryProperties props;

    vkGetPhysicalDeviceMemoryProperties(phys, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if (!(type_bits & (1u << i)))
            continue;
        *type_index = i;
        *type_flags = props.memoryTypes[i].propertyFlags;
        return 0;
    }

    return -1;
}

static int init_vulkan(struct vk_ctx *ctx)
{
    const char *required_exts[] = {
        "VK_KHR_external_memory",
        "VK_KHR_external_memory_fd",
        "VK_KHR_dedicated_allocation",
        "VK_KHR_bind_memory2",
        "VK_EXT_external_memory_dma_buf",
    };
    uint32_t phys_count = 0;
    uint32_t queue_count = 0;
    uint32_t ext_count = 0;
    VkExtensionProperties *exts = NULL;

    memset(ctx, 0, sizeof(*ctx));
    ctx->queue_family = UINT32_MAX;

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "alioth-drm-prime-vulkan-triangle-pageflip",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    if (vk_ok(vkCreateInstance(&instance_info, NULL, &ctx->instance),
              "vkCreateInstance") != 0)
        return -1;

    if (vk_ok(vkEnumeratePhysicalDevices(ctx->instance, &phys_count, NULL),
              "vkEnumeratePhysicalDevices(count)") != 0)
        return -1;
    if (phys_count == 0) {
        fprintf(stderr, "no physical devices\n");
        return -1;
    }

    VkPhysicalDevice *phys_list = calloc(phys_count, sizeof(*phys_list));
    if (!phys_list)
        return -1;
    int rc = vk_ok(vkEnumeratePhysicalDevices(ctx->instance, &phys_count, phys_list),
                   "vkEnumeratePhysicalDevices(list)");
    if (rc == 0)
        ctx->phys = phys_list[0];
    free(phys_list);
    if (rc != 0)
        return -1;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ctx->phys, &props);
    printf("physical_device=%s api=%u.%u.%u driver=%u\n",
           props.deviceName,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion),
           props.driverVersion);

    vkGetPhysicalDeviceQueueFamilyProperties(ctx->phys, &queue_count, NULL);
    VkQueueFamilyProperties *queues = calloc(queue_count, sizeof(*queues));
    if (!queues)
        return -1;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->phys, &queue_count, queues);
    for (uint32_t i = 0; i < queue_count; i++) {
        printf("queue_family[%u] flags=0x%x count=%u\n",
               i, queues[i].queueFlags, queues[i].queueCount);
        if (ctx->queue_family == UINT32_MAX &&
            queues[i].queueCount > 0 &&
            (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            ctx->queue_family = i;
    }
    free(queues);
    if (ctx->queue_family == UINT32_MAX) {
        fprintf(stderr, "no graphics-capable queue family\n");
        return -1;
    }

    if (vk_ok(vkEnumerateDeviceExtensionProperties(ctx->phys, NULL, &ext_count, NULL),
              "vkEnumerateDeviceExtensionProperties(count)") != 0)
        return -1;
    exts = calloc(ext_count ? ext_count : 1, sizeof(*exts));
    if (!exts)
        return -1;
    rc = vk_ok(vkEnumerateDeviceExtensionProperties(ctx->phys, NULL, &ext_count, exts),
               "vkEnumerateDeviceExtensionProperties(list)");
    if (rc != 0) {
        free(exts);
        return -1;
    }
    for (uint32_t i = 0; i < sizeof(required_exts) / sizeof(required_exts[0]); i++) {
        bool present = has_extension(exts, ext_count, required_exts[i]);
        printf("required_ext %-32s %s\n", required_exts[i], present ? "yes" : "no");
        if (!present) {
            free(exts);
            return -1;
        }
    }
    free(exts);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->queue_family,
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
    if (vk_ok(vkCreateDevice(ctx->phys, &device_info, NULL, &ctx->device),
              "vkCreateDevice") != 0)
        return -1;
    vkGetDeviceQueue(ctx->device, ctx->queue_family, 0, &ctx->queue);

    ctx->get_memory_fd_props =
        (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(ctx->device,
                                                            "vkGetMemoryFdPropertiesKHR");
    if (!ctx->get_memory_fd_props) {
        fprintf(stderr, "missing vkGetMemoryFdPropertiesKHR\n");
        return -1;
    }

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = ctx->queue_family,
    };
    if (vk_ok(vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->command_pool),
              "vkCreateCommandPool") != 0)
        return -1;

    return 0;
}

static int create_pipeline(struct vk_ctx *ctx, uint32_t width, uint32_t height,
                           const char *vert_path, const char *frag_path)
{
    uint32_t *vert_spv = NULL;
    uint32_t *frag_spv = NULL;
    size_t vert_words = 0;
    size_t frag_words = 0;
    int rc = -1;

    if (read_file(vert_path, &vert_spv, &vert_words) != 0 ||
        read_file(frag_path, &frag_spv, &frag_words) != 0)
        goto cleanup;
    printf("shader_words vert=%zu frag=%zu\n", vert_words, frag_words);

    VkAttachmentDescription attachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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
    if (vk_ok(vkCreateRenderPass(ctx->device, &render_pass_info, NULL, &ctx->render_pass),
              "vkCreateRenderPass") != 0)
        goto cleanup;

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
    if (vk_ok(vkCreateShaderModule(ctx->device, &vert_info, NULL, &ctx->vert_shader),
              "vkCreateShaderModule(vert)") != 0)
        goto cleanup;
    if (vk_ok(vkCreateShaderModule(ctx->device, &frag_info, NULL, &ctx->frag_shader),
              "vkCreateShaderModule(frag)") != 0)
        goto cleanup;

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = ctx->vert_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = ctx->frag_shader,
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
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width = width, .height = height },
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
    if (vk_ok(vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL,
                                     &ctx->pipeline_layout),
              "vkCreatePipelineLayout") != 0)
        goto cleanup;

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
        .layout = ctx->pipeline_layout,
        .renderPass = ctx->render_pass,
        .subpass = 0,
    };
    if (vk_ok(vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, NULL, &ctx->pipeline),
              "vkCreateGraphicsPipelines") != 0)
        goto cleanup;

    rc = 0;

cleanup:
    free(vert_spv);
    free(frag_spv);
    return rc;
}

static void destroy_vulkan(struct vk_ctx *ctx)
{
    if (ctx->device) {
        if (ctx->pipeline)
            vkDestroyPipeline(ctx->device, ctx->pipeline, NULL);
        if (ctx->pipeline_layout)
            vkDestroyPipelineLayout(ctx->device, ctx->pipeline_layout, NULL);
        if (ctx->frag_shader)
            vkDestroyShaderModule(ctx->device, ctx->frag_shader, NULL);
        if (ctx->vert_shader)
            vkDestroyShaderModule(ctx->device, ctx->vert_shader, NULL);
        if (ctx->render_pass)
            vkDestroyRenderPass(ctx->device, ctx->render_pass, NULL);
        if (ctx->command_pool)
            vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
        vkDestroyDevice(ctx->device, NULL);
    }
    if (ctx->instance)
        vkDestroyInstance(ctx->instance, NULL);
}

static int check_external_format(struct vk_ctx *ctx, const struct drm_buffer *buf)
{
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
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };
    VkExternalImageFormatProperties ext_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 image_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &ext_props,
    };
    VkResult res = vkGetPhysicalDeviceImageFormatProperties2(ctx->phys,
                                                             &image_format,
                                                             &image_props);
    printf("linear_b8g8r8a8_dma_buf_color_attachment_format result=%d maxExtent=%ux%u features=0x%x compatible=0x%x\n",
           res,
           res == VK_SUCCESS ? image_props.imageFormatProperties.maxExtent.width : 0,
           res == VK_SUCCESS ? image_props.imageFormatProperties.maxExtent.height : 0,
           res == VK_SUCCESS ? ext_props.externalMemoryProperties.externalMemoryFeatures : 0,
           res == VK_SUCCESS ? ext_props.externalMemoryProperties.compatibleHandleTypes : 0);

    if (res != VK_SUCCESS)
        return -1;
    if (image_props.imageFormatProperties.maxExtent.width < buf->width ||
        image_props.imageFormatProperties.maxExtent.height < buf->height) {
        fprintf(stderr, "external image max extent is too small\n");
        return -1;
    }
    return 0;
}

static int import_buffer(struct vk_ctx *ctx, const struct drm_buffer *buf,
                         struct imported_image *out)
{
    int import_fd = -1;
    uint32_t memory_type = UINT32_MAX;
    VkMemoryPropertyFlags memory_flags = 0;

    memset(out, 0, sizeof(*out));
    out->width = buf->width;
    out->height = buf->height;
    out->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkExternalMemoryImageCreateInfo external_image = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_image,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = { .width = buf->width, .height = buf->height, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (vk_ok(vkCreateImage(ctx->device, &image_info, NULL, &out->image),
              "vkCreateImage") != 0)
        return -1;

    VkImageSubresource subresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .arrayLayer = 0,
    };
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(ctx->device, out->image, &subresource, &layout);
    printf("vulkan_layout fb=%u offset=%llu size=%llu rowPitch=%llu drm_pitch=%u drm_size=%llu\n",
           buf->fb_id,
           (unsigned long long)layout.offset,
           (unsigned long long)layout.size,
           (unsigned long long)layout.rowPitch,
           buf->pitch,
           (unsigned long long)buf->size);
    if (layout.offset != 0 || layout.rowPitch != buf->pitch || layout.size > buf->size) {
        fprintf(stderr, "linear layout does not match DRM dumb buffer\n");
        return -1;
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx->device, out->image, &req);
    VkMemoryFdPropertiesKHR fd_props = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
    };
    if (vk_ok(ctx->get_memory_fd_props(ctx->device,
                                       VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                       buf->prime_fd,
                                       &fd_props),
              "vkGetMemoryFdPropertiesKHR") != 0)
        return -1;

    uint32_t combined_bits = req.memoryTypeBits & fd_props.memoryTypeBits;
    printf("image_req fb=%u size=%llu type_bits=0x%x fd_type_bits=0x%x combined=0x%x\n",
           buf->fb_id,
           (unsigned long long)req.size,
           req.memoryTypeBits,
           fd_props.memoryTypeBits,
           combined_bits);
    if (req.size > buf->size) {
        fprintf(stderr, "image memory requirement larger than DRM dumb buffer\n");
        return -1;
    }
    if (find_memory_type(ctx->phys, combined_bits, &memory_type, &memory_flags) != 0) {
        fprintf(stderr, "no compatible memory type\n");
        return -1;
    }

    import_fd = dup(buf->prime_fd);
    if (import_fd < 0) {
        fprintf(stderr, "dup prime fd failed: %s\n", strerror(errno));
        return -1;
    }

    VkMemoryDedicatedAllocateInfo dedicated = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = out->image,
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
    if (vk_ok(vkAllocateMemory(ctx->device, &alloc, NULL, &out->memory),
              "vkAllocateMemory(import)") != 0) {
        close(import_fd);
        return -1;
    }
    import_fd = -1;

    if (vk_ok(vkBindImageMemory(ctx->device, out->image, out->memory, 0),
              "vkBindImageMemory") != 0)
        return -1;

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = out->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    if (vk_ok(vkCreateImageView(ctx->device, &view_info, NULL, &out->view),
              "vkCreateImageView") != 0)
        return -1;

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ctx->render_pass,
        .attachmentCount = 1,
        .pAttachments = &out->view,
        .width = buf->width,
        .height = buf->height,
        .layers = 1,
    };
    if (vk_ok(vkCreateFramebuffer(ctx->device, &framebuffer_info, NULL,
                                  &out->framebuffer),
              "vkCreateFramebuffer") != 0)
        return -1;

    printf("imported_fb=%u memory_type=%u flags=0x%x\n",
           buf->fb_id, memory_type, memory_flags);
    return 0;
}

static void destroy_imported(struct vk_ctx *ctx, struct imported_image *image)
{
    if (image->framebuffer)
        vkDestroyFramebuffer(ctx->device, image->framebuffer, NULL);
    if (image->view)
        vkDestroyImageView(ctx->device, image->view, NULL);
    if (image->image)
        vkDestroyImage(ctx->device, image->image, NULL);
    if (image->memory)
        vkFreeMemory(ctx->device, image->memory, NULL);
}

static int render_triangle(struct vk_ctx *ctx, struct imported_image *image,
                           unsigned int frame)
{
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vk_ok(vkAllocateCommandBuffers(ctx->device, &cmd_alloc, &command_buffer),
              "vkAllocateCommandBuffers") != 0)
        return -1;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (vk_ok(vkBeginCommandBuffer(command_buffer, &begin_info),
              "vkBeginCommandBuffer") != 0)
        return -1;

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkImageMemoryBarrier to_color = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = image->initialized ? VK_ACCESS_HOST_READ_BIT : 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = image->layout,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         image->initialized ? VK_PIPELINE_STAGE_HOST_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_color);

    VkClearValue clear = {
        .color = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} },
    };
    VkRenderPassBeginInfo render_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = image->framebuffer,
        .renderArea = {
            .offset = { .x = 0, .y = 0 },
            .extent = {
                .width = image->width,
                .height = image->height,
            },
        },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };

    vkCmdBeginRenderPass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);

    VkImageMemoryBarrier to_host = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_host);

    if (vk_ok(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer") != 0)
        return -1;

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    if (vk_ok(vkQueueSubmit(ctx->queue, 1, &submit, VK_NULL_HANDLE),
              "vkQueueSubmit") != 0)
        return -1;
    if (vk_ok(vkQueueWaitIdle(ctx->queue), "vkQueueWaitIdle") != 0)
        return -1;

    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &command_buffer);
    image->layout = VK_IMAGE_LAYOUT_GENERAL;
    image->initialized = true;
    printf("kgsl_triangle_render frame=%u\n", frame);
    return 0;
}

static bool pixel_is(const uint8_t *px, const uint8_t expected[4])
{
    return memcmp(px, expected, 4) == 0;
}

static int verify_triangle_samples(const struct drm_buffer *buf, unsigned int frame)
{
    const uint8_t red[4] = {0x00, 0x00, 0xff, 0xff};
    const uint8_t black[4] = {0x00, 0x00, 0x00, 0xff};
    const uint32_t samples[][3] = {
        {buf->width / 2, buf->height / 2, 1},
        {buf->width / 20, buf->height / 2, 0},
        {(buf->width * 19) / 20, buf->height / 2, 0},
        {buf->width / 2, buf->height / 20, 0},
        {buf->width / 2, (buf->height * 19) / 20, 0},
    };
    uint32_t pass = 0;

    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ, "cpu_read_start");
    for (uint32_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        uint32_t x = samples[i][0];
        uint32_t y = samples[i][1];
        bool want_red = samples[i][2] != 0;
        const uint8_t *expected = want_red ? red : black;
        const uint8_t *px = (const uint8_t *)buf->map + (size_t)y * buf->pitch + (size_t)x * 4;
        bool ok = pixel_is(px, expected);

        printf("sample frame=%u index=%u x=%u y=%u bytes=%02x %02x %02x %02x expected=%02x %02x %02x %02x ok=%s\n",
               frame, i, x, y,
               px[0], px[1], px[2], px[3],
               expected[0], expected[1], expected[2], expected[3],
               ok ? "yes" : "no");
        if (ok)
            pass++;
    }
    dma_buf_sync_fd(buf->prime_fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ, "cpu_read_end");

    printf("triangle_sample_pass=%u/%zu frame=%u\n",
           pass, sizeof(samples) / sizeof(samples[0]), frame);
    return pass == sizeof(samples) / sizeof(samples[0]) ? 0 : -1;
}

static void page_flip_handler(int fd, unsigned int frame,
                              unsigned int sec, unsigned int usec,
                              void *data)
{
    (void)fd;
    struct flip_state *state = data;

    state->waiting = 0;
    state->seen++;
    printf("page_flip_event frame=%u sec=%u usec=%u seen=%u\n",
           frame, sec, usec, state->seen);
}

static int wait_for_flip(int fd, struct flip_state *state)
{
    drmEventContext ev;
    memset(&ev, 0, sizeof(ev));
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = page_flip_handler;

    while (state->waiting) {
        fd_set fds;
        struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        int rc = select(fd + 1, &fds, NULL, NULL, &timeout);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "select failed: %s\n", strerror(errno));
            return -1;
        }
        if (rc == 0) {
            fprintf(stderr, "page flip wait timed out\n");
            return -1;
        }
        if (drmHandleEvent(fd, &ev) != 0) {
            fprintf(stderr, "drmHandleEvent failed: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *vert_path = "/data/experiments/alioth_vulkan_offscreen_triangle.vert.spv";
    const char *frag_path = "/data/experiments/alioth_vulkan_offscreen_triangle.frag.spv";
    unsigned int flips = 18;
    int fd = -1;
    int rc = 1;
    struct drm_target target;
    struct drm_buffer buffers[2];
    struct imported_image images[2];
    struct vk_ctx vk;
    struct flip_state flip = {0};

    memset(&target, 0, sizeof(target));
    memset(buffers, 0, sizeof(buffers));
    memset(images, 0, sizeof(images));
    memset(&vk, 0, sizeof(vk));
    buffers[0].prime_fd = -1;
    buffers[1].prime_fd = -1;

    if (argc >= 2)
        flips = (unsigned int)strtoul(argv[1], NULL, 10);
    if (argc >= 3)
        vert_path = argv[2];
    if (argc >= 4)
        frag_path = argv[3];
    if (flips < 1 || flips > 60) {
        fprintf(stderr, "usage: %s [flips 1..60] [vert.spv] [frag.spv]\n", argv[0]);
        return 2;
    }

    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open /dev/dri/card0 failed: %s\n", strerror(errno));
        return 1;
    }
    if (find_target(fd, &target) != 0) {
        fprintf(stderr, "no connected DRM target\n");
        goto cleanup;
    }
    printf("drm_target connector=%u crtc=%u mode=%ux%u@%u flips=%u\n",
           target.connector_id, target.crtc_id,
           target.mode.hdisplay, target.mode.vdisplay,
           target.mode.vrefresh, flips);

    if (create_drm_buffer(fd, &target, &buffers[0]) != 0)
        goto cleanup;
    if (create_drm_buffer(fd, &target, &buffers[1]) != 0)
        goto cleanup;

    if (init_vulkan(&vk) != 0)
        goto cleanup;
    if (check_external_format(&vk, &buffers[0]) != 0)
        goto cleanup;
    if (create_pipeline(&vk, buffers[0].width, buffers[0].height,
                        vert_path, frag_path) != 0)
        goto cleanup;
    if (import_buffer(&vk, &buffers[0], &images[0]) != 0)
        goto cleanup;
    if (import_buffer(&vk, &buffers[1], &images[1]) != 0)
        goto cleanup;

    if (render_triangle(&vk, &images[0], 0) != 0)
        goto cleanup;
    if (verify_triangle_samples(&buffers[0], 0) != 0)
        goto cleanup;

    if (drmModeSetCrtc(fd, target.crtc_id, buffers[0].fb_id, 0, 0,
                       &target.connector_id, 1, &target.mode) != 0) {
        fprintf(stderr, "drmModeSetCrtc failed: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("initial_set_crtc=PASS fb=%u\n", buffers[0].fb_id);

    for (unsigned int i = 0; i < flips; i++) {
        unsigned int next = (i + 1) % 2;

        if (render_triangle(&vk, &images[next], i + 1) != 0)
            goto cleanup;
        if (verify_triangle_samples(&buffers[next], i + 1) != 0)
            goto cleanup;

        flip.waiting = 1;
        if (drmModePageFlip(fd, target.crtc_id, buffers[next].fb_id,
                            DRM_MODE_PAGE_FLIP_EVENT, &flip) != 0) {
            fprintf(stderr, "drmModePageFlip failed flip=%u fb=%u errno=%d %s\n",
                    i, buffers[next].fb_id, errno, strerror(errno));
            goto cleanup;
        }
        printf("page_flip_submit index=%u fb=%u\n", i, buffers[next].fb_id);
        if (wait_for_flip(fd, &flip) != 0)
            goto cleanup;

        struct timespec pause = { .tv_sec = 0, .tv_nsec = 80000000L };
        nanosleep(&pause, NULL);
    }

    printf("drm_prime_kgsl_triangle_pageflip=PASS submitted=%u events=%u\n",
           flips, flip.seen);
    rc = 0;

cleanup:
    if (vk.device) {
        destroy_imported(&vk, &images[1]);
        destroy_imported(&vk, &images[0]);
    }
    destroy_vulkan(&vk);
    if (fd >= 0) {
        destroy_drm_buffer(fd, &buffers[1]);
        destroy_drm_buffer(fd, &buffers[0]);
        close(fd);
    }
    return rc;
}
