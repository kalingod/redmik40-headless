#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <xf86drm.h>
#include <libdrm/drm.h>
#include <libdrm/msm_drm.h>

struct param_case {
    uint32_t param;
    const char *name;
};

static const struct param_case param_cases[] = {
    { MSM_PARAM_GPU_ID, "GPU_ID" },
    { MSM_PARAM_GMEM_SIZE, "GMEM_SIZE" },
    { MSM_PARAM_CHIP_ID, "CHIP_ID" },
    { MSM_PARAM_MAX_FREQ, "MAX_FREQ" },
    { MSM_PARAM_TIMESTAMP, "TIMESTAMP" },
    { MSM_PARAM_GMEM_BASE, "GMEM_BASE" },
    { MSM_PARAM_NR_RINGS, "NR_RINGS" },
};

static void print_drm_version(int fd)
{
    drmVersionPtr version = drmGetVersion(fd);
    if (!version) {
        printf("drm_version=FAIL errno=%d %s\n", errno, strerror(errno));
        return;
    }

    printf("drm_version name=%.*s version=%d.%d.%d date=%.*s desc=%.*s\n",
           version->name_len, version->name ? version->name : "",
           version->version_major, version->version_minor,
           version->version_patchlevel,
           version->date_len, version->date ? version->date : "",
           version->desc_len, version->desc ? version->desc : "");
    drmFreeVersion(version);
}

static void probe_param(int fd, uint32_t pipe, const struct param_case *pc)
{
    struct drm_msm_param req;
    memset(&req, 0, sizeof(req));
    req.pipe = pipe;
    req.param = pc->param;

    errno = 0;
    int rc = ioctl(fd, DRM_IOCTL_MSM_GET_PARAM, &req);
    if (rc == 0) {
        printf("get_param pipe=0x%x %-10s rc=0 value=0x%llx (%llu)\n",
               pipe, pc->name,
               (unsigned long long)req.value,
               (unsigned long long)req.value);
    } else {
        printf("get_param pipe=0x%x %-10s rc=%d errno=%d %s\n",
               pipe, pc->name, rc, errno, strerror(errno));
    }
}

static void probe_gem(int fd)
{
    struct drm_msm_gem_new gem;
    memset(&gem, 0, sizeof(gem));
    gem.size = 4096;
    gem.flags = MSM_BO_WC;

    errno = 0;
    int rc = ioctl(fd, DRM_IOCTL_MSM_GEM_NEW, &gem);
    if (rc != 0) {
        printf("gem_new size=4096 flags=MSM_BO_WC rc=%d errno=%d %s\n",
               rc, errno, strerror(errno));
        return;
    }

    printf("gem_new size=4096 flags=MSM_BO_WC rc=0 handle=%u\n", gem.handle);

    struct drm_msm_gem_info info;
    memset(&info, 0, sizeof(info));
    info.handle = gem.handle;
    info.flags = MSM_INFO_IOVA;

    errno = 0;
    rc = ioctl(fd, DRM_IOCTL_MSM_GEM_INFO, &info);
    if (rc == 0) {
        printf("gem_info IOVA rc=0 offset=0x%llx\n",
               (unsigned long long)info.offset);
    } else {
        printf("gem_info IOVA rc=%d errno=%d %s\n",
               rc, errno, strerror(errno));
    }

    struct drm_gem_close close_req;
    memset(&close_req, 0, sizeof(close_req));
    close_req.handle = gem.handle;
    errno = 0;
    rc = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close_req);
    printf("gem_close handle=%u rc=%d errno=%d %s\n",
           gem.handle, rc, errno, rc == 0 ? "OK" : strerror(errno));
}

static void probe_node(const char *path)
{
    printf("node=%s\n", path);
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        printf("open=FAIL errno=%d %s\n\n", errno, strerror(errno));
        return;
    }

    printf("open=OK fd=%d\n", fd);
    print_drm_version(fd);

    const uint32_t pipes[] = { MSM_PIPE_3D0, MSM_PIPE_NONE };
    for (size_t p = 0; p < sizeof(pipes) / sizeof(pipes[0]); ++p) {
        for (size_t i = 0; i < sizeof(param_cases) / sizeof(param_cases[0]); ++i) {
            probe_param(fd, pipes[p], &param_cases[i]);
        }
    }

    probe_gem(fd);
    close(fd);
    printf("\n");
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            probe_node(argv[i]);
        }
        return 0;
    }

    probe_node("/dev/dri/renderD128");
    probe_node("/dev/dri/card0");
    return 0;
}
