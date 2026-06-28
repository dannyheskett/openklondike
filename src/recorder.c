// _POSIX_C_SOURCE: exposes localtime_r for the auto-named output file.
// _FILE_OFFSET_BITS: make off_t/fseeko 64-bit on 32-bit POSIX targets.
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include "recorder.h"

#include "minih264e.h"  // declarations only (implementation is in encode_h264.c)
#include "minimp4.h"    // declarations only (implementation is in encode_mux.c)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Canvas dimensions (must be multiples of 16 for the H.264 encoder).
// These match render.h MIN_W/MIN_H (both 704 = 44*16): regardless of the live
// window size, the game is re-rendered at this fixed size for capture.
#define VID_W      704
#define VID_H      704
#define FPS        60
#define TIMESCALE  90000
#define FRAME_DUR  (TIMESCALE / FPS)   // 1500 ticks per frame
#define QP         18                  // near-lossless; lower = bigger/slower

static bool s_active = false;
static char s_path[512];

static H264E_persist_t* s_enc = NULL;
static H264E_scratch_t* s_scr = NULL;
static unsigned char*    s_yuv = NULL;
static H264E_io_yuv_t    s_io;

static MP4E_mux_t*       s_mux = NULL;
static mp4_h26x_writer_t s_writer;
static FILE*             s_fout = NULL;
static int               s_frame = 0;

// BT.601 limited-range RGB -> I420. No padding (640x480 are /16). Reused per
// frame against the persistent s_yuv planes.
static void rgba_to_i420(const unsigned char* rgba) {
    unsigned char* y = s_io.yuv[0];
    unsigned char* u = s_io.yuv[1];
    unsigned char* v = s_io.yuv[2];

    for (int j = 0; j < VID_H; j++) {
        const unsigned char* row = rgba + (size_t)j * VID_W * 4;
        unsigned char* yrow = y + (size_t)j * VID_W;
        for (int i = 0; i < VID_W; i++) {
            int r = row[4*i + 0], g = row[4*i + 1], b = row[4*i + 2];
            int yv = 16 + ((66*r + 129*g + 25*b + 128) >> 8);
            yrow[i] = (unsigned char)(yv < 0 ? 0 : yv > 255 ? 255 : yv);
        }
    }
    for (int j = 0; j < VID_H; j += 2) {
        const unsigned char* row0 = rgba + (size_t)j * VID_W * 4;
        const unsigned char* row1 = rgba + (size_t)(j + 1) * VID_W * 4;
        unsigned char* urow = u + (size_t)(j/2) * (VID_W/2);
        unsigned char* vrow = v + (size_t)(j/2) * (VID_W/2);
        for (int i = 0; i < VID_W; i += 2) {
            int r = (row0[4*i+0] + row0[4*(i+1)+0] + row1[4*i+0] + row1[4*(i+1)+0]) >> 2;
            int g = (row0[4*i+1] + row0[4*(i+1)+1] + row1[4*i+1] + row1[4*(i+1)+1]) >> 2;
            int b = (row0[4*i+2] + row0[4*(i+1)+2] + row1[4*i+2] + row1[4*(i+1)+2]) >> 2;
            int uv = 128 + ((-38*r - 74*g + 112*b + 128) >> 8);
            int vv = 128 + (( 112*r - 94*g - 18*b + 128) >> 8);
            urow[i/2] = (unsigned char)(uv < 0 ? 0 : uv > 255 ? 255 : uv);
            vrow[i/2] = (unsigned char)(vv < 0 ? 0 : vv > 255 ? 255 : vv);
        }
    }
}

// 64-bit file seek: MP4 output can exceed 4 GiB, so the muxer's int64_t offset
// must not be truncated to a 32-bit long.
#ifdef _WIN32
#  define seek64(f, off) _fseeki64((f), (off), SEEK_SET)
#else
#  define seek64(f, off) fseeko((f), (off), SEEK_SET)
#endif

static int mp4_write_cb(int64_t off, const void* buf, size_t sz, void* tok) {
    FILE* f = (FILE*)tok;
    if (seek64(f, off) != 0) return 1;
    return fwrite(buf, 1, sz, f) != sz;
}

static bool file_exists(const char* p) {
    FILE* f = fopen(p, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

static void auto_name(char* out, size_t cap) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char base[256];
    strftime(base, sizeof base, "openklondike-%Y%m%d-%H%M%S", &tmv);
    // Timestamps are second-resolution, so disambiguate restarts within the
    // same second (and any pre-existing file) with a -N suffix.
    snprintf(out, cap, "%s.mp4", base);
    for (int n = 2; file_exists(out); n++) {
        snprintf(out, cap, "%s-%d.mp4", base, n);
    }
}

static void cleanup(void) {
    if (s_mux) { mp4_h26x_write_close(&s_writer); MP4E_close(s_mux); s_mux = NULL; }
    if (s_fout) { fclose(s_fout); s_fout = NULL; }
    free(s_enc); s_enc = NULL;
    free(s_scr); s_scr = NULL;
    free(s_yuv); s_yuv = NULL;
    s_frame = 0;
}

bool recorder_start(const char* path) {
    if (s_active) return false;

    if (path && path[0]) {
        snprintf(s_path, sizeof s_path, "%s", path);
    } else {
        auto_name(s_path, sizeof s_path);
    }

    H264E_create_param_t cp = {0};
    cp.width = VID_W;
    cp.height = VID_H;
    cp.gop = FPS;              // a keyframe every second
    cp.const_input_flag = 1;   // don't trash our input buffer

    int persist_size = 0, scratch_size = 0;
    if (H264E_sizeof(&cp, &persist_size, &scratch_size) != 0) return false;

    s_enc = malloc((size_t)persist_size);
    s_scr = malloc((size_t)scratch_size);
    s_yuv = malloc((size_t)VID_W * VID_H + 2 * (size_t)(VID_W/2) * (VID_H/2));
    if (!s_enc || !s_scr || !s_yuv) { cleanup(); return false; }

    if (H264E_init(s_enc, &cp) != 0) { cleanup(); return false; }

    s_io.yuv[0] = s_yuv;
    s_io.yuv[1] = s_yuv + (size_t)VID_W * VID_H;
    s_io.yuv[2] = s_io.yuv[1] + (size_t)(VID_W/2) * (VID_H/2);
    s_io.stride[0] = VID_W;
    s_io.stride[1] = VID_W / 2;
    s_io.stride[2] = VID_W / 2;

    s_fout = fopen(s_path, "wb");
    if (!s_fout) { cleanup(); return false; }
    s_mux = MP4E_open(0 /*sequential*/, 0 /*fragmented*/, s_fout, mp4_write_cb);
    if (!s_mux || mp4_h26x_write_init(&s_writer, s_mux, VID_W, VID_H, 0 /*is_hevc*/) != 0) {
        cleanup();
        return false;
    }

    s_frame = 0;
    s_active = true;
    return true;
}

void recorder_stop(void) {
    if (!s_active) return;
    s_active = false;
    cleanup();
}

bool recorder_toggle(void) {
    if (s_active) recorder_stop();
    else recorder_start(NULL);
    return s_active;
}

bool recorder_active(void) { return s_active; }

void recorder_capture(const RenderTexture2D* canvas) {
    if (!s_active || !canvas) return;

    Image img = LoadImageFromTexture(canvas->texture);
    if (img.data == NULL) return;
    ImageFlipVertical(&img); // render textures are stored bottom-up
    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    if (img.width != VID_W || img.height != VID_H) { UnloadImage(img); return; }

    rgba_to_i420((const unsigned char*)img.data);
    UnloadImage(img);

    H264E_run_param_t rp = {0};
    rp.encode_speed = H264E_SPEED_FASTEST; // keep recording close to real-time
    rp.frame_type = (s_frame == 0) ? H264E_FRAME_TYPE_KEY : H264E_FRAME_TYPE_DEFAULT;
    rp.qp_min = QP;
    rp.qp_max = QP;

    unsigned char* coded = NULL;
    int coded_sz = 0;
    if (H264E_encode(s_enc, s_scr, &rp, &s_io, &coded, &coded_sz) != 0) return;

    if (mp4_h26x_write_nal(&s_writer, coded, coded_sz, FRAME_DUR) != MP4E_STATUS_OK) {
        // A write/mux failure (e.g. disk full) would otherwise be silently
        // dropped while the recorder kept running on a truncated file.
        recorder_stop();
        return;
    }
    s_frame++;
}
