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

struct drm_target {
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
};

struct dumb_buffer {
    uint32_t handle;
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint64_t size;
    void *map;
};

struct flip_state {
    int waiting;
    unsigned int seen;
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

static void fill_pattern(struct dumb_buffer *buf, uint32_t color_a, uint32_t color_b)
{
    uint8_t *base = buf->map;
    const uint32_t stripe = buf->height > 0 ? buf->height / 12 : 1;

    if (stripe == 0)
        return;

    for (uint32_t y = 0; y < buf->height; y++) {
        uint32_t *row = (uint32_t *)(base + (size_t)y * buf->pitch);
        uint32_t color = ((y / stripe) % 2) ? color_a : color_b;
        for (uint32_t x = 0; x < buf->width; x++)
            row[x] = color;
    }
}

static int create_buffer(int fd, const struct drm_target *target,
                         struct dumb_buffer *buf,
                         uint32_t color_a, uint32_t color_b)
{
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};

    memset(buf, 0, sizeof(*buf));

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

    fill_pattern(buf, color_a, color_b);

    uint32_t handles[4] = {buf->handle, 0, 0, 0};
    uint32_t pitches[4] = {buf->pitch, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    if (drmModeAddFB2(fd, buf->width, buf->height, DRM_FORMAT_XRGB8888,
                      handles, pitches, offsets, &buf->fb_id, 0) != 0) {
        fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(errno));
        return -1;
    }

    printf("buffer handle=%u fb=%u width=%u height=%u pitch=%u size=%llu\n",
           buf->handle, buf->fb_id, buf->width, buf->height, buf->pitch,
           (unsigned long long)buf->size);
    return 0;
}

static void destroy_buffer(int fd, struct dumb_buffer *buf)
{
    if (buf->fb_id)
        drmModeRmFB(fd, buf->fb_id);
    if (buf->map)
        munmap(buf->map, buf->size);
    if (buf->handle) {
        struct drm_mode_destroy_dumb destroy_req = { .handle = buf->handle };
        ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
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
    unsigned int flips = 24;
    int fd = -1;
    int rc = 1;
    struct drm_target target;
    struct dumb_buffer buffers[2];
    struct flip_state state = {0};

    memset(&target, 0, sizeof(target));
    memset(buffers, 0, sizeof(buffers));

    if (argc >= 2)
        flips = (unsigned int)strtoul(argv[1], NULL, 10);
    if (flips < 1 || flips > 120) {
        fprintf(stderr, "usage: %s [flips 1..120]\n", argv[0]);
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

    if (create_buffer(fd, &target, &buffers[0], 0x0000ff00, 0x00002000) != 0)
        goto cleanup;
    if (create_buffer(fd, &target, &buffers[1], 0x000000ff, 0x00000040) != 0)
        goto cleanup;

    if (drmModeSetCrtc(fd, target.crtc_id, buffers[0].fb_id, 0, 0,
                       &target.connector_id, 1, &target.mode) != 0) {
        fprintf(stderr, "drmModeSetCrtc failed: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("initial_set_crtc=PASS fb=%u\n", buffers[0].fb_id);

    for (unsigned int i = 0; i < flips; i++) {
        uint32_t fb = buffers[(i + 1) % 2].fb_id;

        state.waiting = 1;
        if (drmModePageFlip(fd, target.crtc_id, fb,
                            DRM_MODE_PAGE_FLIP_EVENT, &state) != 0) {
            fprintf(stderr, "drmModePageFlip failed flip=%u fb=%u errno=%d %s\n",
                    i, fb, errno, strerror(errno));
            goto cleanup;
        }
        printf("page_flip_submit index=%u fb=%u\n", i, fb);
        if (wait_for_flip(fd, &state) != 0)
            goto cleanup;

        struct timespec pause = { .tv_sec = 0, .tv_nsec = 100000000L };
        nanosleep(&pause, NULL);
    }

    printf("drm_pageflip_smoke=PASS submitted=%u events=%u\n", flips, state.seen);
    rc = 0;

cleanup:
    destroy_buffer(fd, &buffers[1]);
    destroy_buffer(fd, &buffers[0]);
    if (fd >= 0)
        close(fd);
    return rc;
}
