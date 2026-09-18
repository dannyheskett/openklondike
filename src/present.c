#include "present.h"
#include "gfx.h"
#include "platform.h"
#include "recorder.h"

#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_WEB) && !defined(PLATFORM_IOS)
#define PRESENT_RECORDS 1
#include <raylib.h>
#include <rlgl.h>

#define SS 2   // capture supersampling factor

static RenderTexture2D s_rec_canvas;   // REC_W x REC_H, what the encoder reads
static RenderTexture2D s_rec_super;    // SS x that, drawn into then minified
static bool s_rec_ready = false;
#endif

void present_init(void) {
#ifdef PRESENT_RECORDS
    s_rec_canvas = LoadRenderTexture(REC_W, REC_H);
    s_rec_super  = LoadRenderTexture(SS * REC_W, SS * REC_H);
    SetTextureFilter(s_rec_super.texture, TEXTURE_FILTER_BILINEAR);
    s_rec_ready = true;
#endif
}

void present_cleanup(void) {
#ifdef PRESENT_RECORDS
    if (s_rec_ready) {
        UnloadRenderTexture(s_rec_canvas);
        UnloadRenderTexture(s_rec_super);
        s_rec_ready = false;
    }
#endif
}

void present(SceneFn scene, void* ctx) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    gfx_begin_frame();
    scene(ctx, w, h);
    if (recorder_active()) {
        // A small REC mark in the window's top-left corner while recording.
        // Window only: the capture below never shows it.
        int ref = (w > h) ? w : h;
        int fs = ref / 64, r = fs / 3;
        gfx_circle((float)(fs / 2 + r), (float)(fs / 2 + fs / 2), (float)r, (Color){ 230, 41, 55, 255 });
        gfx_text("REC", fs / 2 + r * 3, fs / 2, fs, (Color){ 230, 41, 55, 255 });
    }
    gfx_end_frame();

#ifdef PRESENT_RECORDS
    if (!recorder_active() || !s_rec_ready) return;

    // 1) The scene at SS x the capture size. Blend colour normally but keep the
    //    target opaque: with the default blend every translucent draw lowers the
    //    texture's alpha, and the minify below then composites it over nothing,
    //    so the video would show those pixels faded.
    BeginTextureMode(s_rec_super);
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
                              RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlPushMatrix();
    rlScalef((float)SS, (float)SS, 1.0f);
    scene(ctx, REC_W, REC_H);
    rlPopMatrix();
    EndBlendMode();
    EndTextureMode();

    // 2) Minify into the encoder canvas with bilinear filtering (the AA). The
    //    negative source height flips the bottom-up render texture upright.
    BeginTextureMode(s_rec_canvas);
    Rectangle src = { 0, 0, (float)(SS * REC_W), -(float)(SS * REC_H) };
    Rectangle dst = { 0, 0, (float)REC_W, (float)REC_H };
    DrawTexturePro(s_rec_super.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndTextureMode();

    recorder_capture(&s_rec_canvas);
#endif
}
