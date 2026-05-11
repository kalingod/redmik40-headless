#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm.h>
#include <drm_fourcc.h>
#include <drm_mode.h>

struct drm_target {
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
};

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

static uint32_t *read_raw(const char *path, uint32_t width, uint32_t height)
{
    size_t pixels = (size_t)width * height;
    size_t bytes = pixels * sizeof(uint32_t);
    uint32_t *buf = malloc(bytes);
    FILE *fp;

    if (!buf)
        return NULL;
    fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        free(buf);
        return NULL;
    }
    if (fread(buf, 1, bytes, fp) != bytes) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", bytes, path);
        fclose(fp);
        free(buf);
        return NULL;
    }
    fclose(fp);
    return buf;
}

static void fill_screen(uint8_t *map, uint32_t pitch, uint32_t width, uint32_t height,
                        const uint32_t *src, uint32_t src_w, uint32_t src_h)
{
    uint32_t box = width * 3 / 5;
    uint32_t max_h = height * 2 / 5;

    if (box > max_h)
        box = max_h;
    if (box < src_w)
        box = src_w;
    if (box > width)
        box = width;
    if (box > height)
        box = height;

    uint32_t start_x = (width - box) / 2;
    uint32_t start_y = (height - box) / 2;
    uint32_t border = box > 64 ? 6 : 2;

    for (uint32_t y = 0; y < height; y++) {
        uint32_t *row = (uint32_t *)(void *)(map + (size_t)y * pitch);
        for (uint32_t x = 0; x < width; x++)
            row[x] = 0x0008141f;
    }

    for (uint32_t y = 0; y < box; y++) {
        uint32_t *row = (uint32_t *)(void *)(map + (size_t)(start_y + y) * pitch);
        for (uint32_t x = 0; x < box; x++) {
            if (x < border || y < border || x >= box - border || y >= box - border) {
                row[start_x + x] = 0x0000ff90;
            } else {
                uint32_t sx = (x - border) * src_w / (box - border * 2);
                uint32_t sy = (y - border) * src_h / (box - border * 2);
                row[start_x + x] = src[(size_t)sy * src_w + sx];
            }
        }
    }
}

static void sleep_seconds(unsigned int seconds)
{
    struct timespec req = { .tv_sec = (time_t)seconds, .tv_nsec = 0 };

    while (nanosleep(&req, &req) < 0 && errno == EINTR) {
    }
}

int main(int argc, char **argv)
{
    const char *raw_path;
    uint32_t raw_w;
    uint32_t raw_h;
    unsigned int duration;
    uint32_t *raw = NULL;
    int fd = -1;
    struct drm_target target;
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};
    struct drm_mode_destroy_dumb destroy_req = {0};
    uint32_t handles[4] = {0};
    uint32_t pitches[4] = {0};
    uint32_t offsets[4] = {0};
    uint32_t fb_id = 0;
    uint8_t *map = MAP_FAILED;
    int rc = 1;

    if (argc < 5) {
        fprintf(stderr, "usage: %s RAW_XRGB WIDTH HEIGHT DURATION_SEC\n", argv[0]);
        return 2;
    }
    raw_path = argv[1];
    raw_w = (uint32_t)strtoul(argv[2], NULL, 10);
    raw_h = (uint32_t)strtoul(argv[3], NULL, 10);
    duration = (unsigned int)strtoul(argv[4], NULL, 10);
    if (!raw_w || !raw_h || duration < 1 || duration > 10) {
        fprintf(stderr, "invalid width/height/duration\n");
        return 2;
    }

    raw = read_raw(raw_path, raw_w, raw_h);
    if (!raw)
        return 1;

    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("/dev/dri/card0");
        goto cleanup;
    }
    if (find_target(fd, &target) != 0) {
        fprintf(stderr, "no connected DRM target\n");
        goto cleanup;
    }
    printf("drm_target connector=%u crtc=%u mode=%ux%u@%u\n",
           target.connector_id, target.crtc_id,
           target.mode.hdisplay, target.mode.vdisplay, target.mode.vrefresh);

    memset(&create_req, 0, sizeof(create_req));
    create_req.width = target.mode.hdisplay;
    create_req.height = target.mode.vdisplay;
    create_req.bpp = 32;
    if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        fprintf(stderr, "CREATE_DUMB failed: %s\n", strerror(errno));
        goto cleanup;
    }

    handles[0] = create_req.handle;
    pitches[0] = create_req.pitch;
    if (drmModeAddFB2(fd, create_req.width, create_req.height, DRM_FORMAT_XRGB8888,
                      handles, pitches, offsets, &fb_id, 0) != 0) {
        fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(errno));
        goto cleanup;
    }

    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = create_req.handle;
    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        fprintf(stderr, "MAP_DUMB failed: %s\n", strerror(errno));
        goto cleanup;
    }
    map = mmap(NULL, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)map_req.offset);
    if (map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        goto cleanup;
    }

    fill_screen(map, create_req.pitch, create_req.width, create_req.height, raw, raw_w, raw_h);
    if (drmModeSetCrtc(fd, target.crtc_id, fb_id, 0, 0, &target.connector_id, 1, &target.mode) != 0) {
        fprintf(stderr, "drmModeSetCrtc failed: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("present_raw_xrgb=PASS fb=%u pitch=%u duration=%u\n", fb_id, create_req.pitch, duration);
    sleep_seconds(duration);
    rc = 0;

cleanup:
    if (map != MAP_FAILED)
        munmap(map, create_req.size);
    if (fd >= 0 && fb_id)
        drmModeRmFB(fd, fb_id);
    if (fd >= 0 && create_req.handle) {
        memset(&destroy_req, 0, sizeof(destroy_req));
        destroy_req.handle = create_req.handle;
        ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
    if (fd >= 0)
        close(fd);
    free(raw);
    return rc;
}
