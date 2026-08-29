// Native Metal backend for the gfx primitive layer (iOS) -- no raylib.
//
// Immediate-mode design: each frame the gfx_* calls append triangles (colored,
// or textured from a font atlas) to a CPU vertex list in pixel coordinates;
// gfx_end_frame uploads them and issues one draw. A single pipeline handles both
// solid fills (sampling a white texel in the atlas) and text (sampling glyph
// coverage), so there is one shader and one draw call per frame.
//
// Playing cards need more than rectangles: the rounded card bodies, the circular
// club lobes, and the parametric heart/spade/diamond fans all bottom out in the
// same triangle push, so every primitive below is built from tri_solid.
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <vector>
#include <string.h>
#include <math.h>

#import "gfx.h"
#import "gfx_metal.h"
#include "font_atlas.h"  // bundled Nunito font, baked (generated), for parity

// --- Vertex layout ----------------------------------------------------------
struct GVert { float x, y, u, v, r, g, b, a; }; // 32 bytes; matches packed MSL

static id<MTLDevice>          s_device;
static id<MTLCommandQueue>    s_queue;
static CAMetalLayer*          s_layer;
static id<MTLRenderPipelineState> s_pipeline;
static id<MTLSamplerState>    s_sampler;
static id<MTLTexture>         s_atlas;
static int                    s_glyph_index[256]; // codepoint -> ok_font_glyphs index

static std::vector<GVert> s_verts;
static float s_cr = 0, s_cg = 0, s_cb = 0, s_ca = 1; // clear colour
static int   s_fw = 1, s_fh = 1;                      // full drawable (px)
static int   s_ox = 0, s_oy = 0;                      // safe-area origin (px)

// Triple-buffered vertex buffers, reused across frames (grown on demand) instead
// of allocating one per frame; the semaphore stops us overwriting a buffer the
// GPU is still reading.
#define OK_INFLIGHT 3
static id<MTLBuffer>        s_vbuf[OK_INFLIGHT];
static NSUInteger           s_vcap[OK_INFLIGHT];
static int                  s_frame_idx = 0;
static dispatch_semaphore_t s_inflight;

static const char* kShader = R"(
#include <metal_stdlib>
using namespace metal;
struct Vertex   { packed_float2 pos; packed_float2 uv; packed_float4 color; };
struct Uniforms { float2 viewport; float2 origin; };
struct VOut     { float4 position [[position]]; float2 uv; float4 color; };
vertex VOut v_main(uint vid [[vertex_id]],
                   const device Vertex* verts [[buffer(0)]],
                   constant Uniforms& u [[buffer(1)]]) {
    Vertex v = verts[vid];
    float2 p = v.pos + u.origin; // shift the game's (0,0) to the safe-area corner
    float2 ndc = float2(p.x / u.viewport.x * 2.0 - 1.0,
                        1.0 - p.y / u.viewport.y * 2.0);
    VOut o; o.position = float4(ndc, 0.0, 1.0); o.uv = v.uv; o.color = v.color; return o;
}
fragment float4 f_main(VOut in [[stage_in]],
                       texture2d<float> atlas [[texture(0)]],
                       sampler samp [[sampler(0)]]) {
    float a = atlas.sample(samp, in.uv).r; // single-channel (R8) coverage
    return float4(in.color.rgb, in.color.a * a);
}
)";

// --- Setup ------------------------------------------------------------------
static void build_font_atlas(void) {
    // Upload the baked Nunito alpha atlas as a single-channel texture. The
    // generator already forced the bottom-right 8x8 block opaque, which is the
    // "white block" solid primitives sample (see uv_white).
    static unsigned char atlas[OK_FONT_ATLAS_W * OK_FONT_ATLAS_H];
    memcpy(atlas, ok_font_atlas_alpha, sizeof(atlas));

    MTLTextureDescriptor* td =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                           width:OK_FONT_ATLAS_W
                                                          height:OK_FONT_ATLAS_H mipmapped:NO];
    s_atlas = [s_device newTextureWithDescriptor:td];
    [s_atlas replaceRegion:MTLRegionMake2D(0, 0, OK_FONT_ATLAS_W, OK_FONT_ATLAS_H)
               mipmapLevel:0 withBytes:atlas bytesPerRow:OK_FONT_ATLAS_W];

    // Codepoint -> glyph-array index (fallback '?').
    for (int i = 0; i < 256; i++) s_glyph_index[i] = -1;
    for (int i = 0; i < OK_FONT_GLYPH_COUNT; i++) {
        int v = ok_font_glyphs[i].value;
        if (v >= 0 && v < 256) s_glyph_index[v] = i;
    }
}

// Glyph index for a codepoint, falling back to '?' then 0.
static int glyph_of(int cp) {
    int gi = (cp >= 0 && cp < 256) ? s_glyph_index[cp] : -1;
    if (gi < 0) gi = s_glyph_index[(unsigned char)'?'];
    if (gi < 0) gi = 0;
    return gi;
}

void gfx_metal_attach(CAMetalLayer* layer) {
    s_device = MTLCreateSystemDefaultDevice();
    s_queue  = [s_device newCommandQueue];
    s_inflight = dispatch_semaphore_create(OK_INFLIGHT);
    layer.device          = s_device;
    layer.pixelFormat     = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    s_layer  = layer;

    NSError* err = nil;
    id<MTLLibrary> lib = [s_device newLibraryWithSource:[NSString stringWithUTF8String:kShader]
                                                options:nil error:&err];
    MTLRenderPipelineDescriptor* pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction   = [lib newFunctionWithName:@"v_main"];
    pd.fragmentFunction = [lib newFunctionWithName:@"f_main"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pd.colorAttachments[0].blendingEnabled = YES;
    pd.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
    pd.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
    pd.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
    pd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    s_pipeline = [s_device newRenderPipelineStateWithDescriptor:pd error:&err];

    MTLSamplerDescriptor* sd = [[MTLSamplerDescriptor alloc] init];
    sd.minFilter = MTLSamplerMinMagFilterLinear; // smooth Nunito, matching raylib's bilinear
    sd.magFilter = MTLSamplerMinMagFilterLinear;
    s_sampler = [s_device newSamplerStateWithDescriptor:sd];

    build_font_atlas();
}

void gfx_metal_set_viewport(int full_w, int full_h, int origin_x, int origin_y) {
    s_fw = full_w > 0 ? full_w : 1;
    s_fh = full_h > 0 ? full_h : 1;
    s_ox = origin_x;
    s_oy = origin_y;
}

// --- Vertex helpers ---------------------------------------------------------
static inline void uv_white(float* u, float* v) {
    // Centre of the forced-opaque bottom-right 8x8 block. Sampling 4px in from
    // the corner keeps the LINEAR footprint entirely inside the white block, so
    // solid fills read coverage 1.0 (a single texel would bleed under linear).
    *u = (OK_FONT_ATLAS_W - 4.0f) / (float)OK_FONT_ATLAS_W;
    *v = (OK_FONT_ATLAS_H - 4.0f) / (float)OK_FONT_ATLAS_H;
}

static inline void push(float x, float y, float u, float v, Color c) {
    GVert g = { x, y, u, v, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
    s_verts.push_back(g);
}

// Solid triangle (uv fixed to the white texel). The render encoder never enables
// face culling, so winding is irrelevant -- which is what lets gfx_triangle be
// winding-independent without emitting the triangle twice as raylib must.
static void tri_solid(float x0, float y0, float x1, float y1, float x2, float y2, Color c) {
    float u, v; uv_white(&u, &v);
    push(x0, y0, u, v, c); push(x1, y1, u, v, c); push(x2, y2, u, v, c);
}

static void quad_solid(float x, float y, float w, float h, Color c) {
    tri_solid(x, y, x + w, y, x + w, y + h, c);
    tri_solid(x, y, x + w, y + h, x, y + h, c);
}

// A 1px-wide quad along a segment: the stroke unit every outline is built from.
static void seg_stroke(float x1, float y1, float x2, float y2, Color c) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) { quad_solid(x1, y1, 1, 1, c); return; }
    float nx = -dy / len * 0.5f, ny = dx / len * 0.5f; // half-thickness normal (1px)
    tri_solid(x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, c);
    tri_solid(x1 + nx, y1 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny, c);
}

// Stroke a polyline, optionally closing it back to the first point.
static void stroke_path(const float* xs, const float* ys, int n, bool closed, Color c) {
    for (int i = 0; i + 1 < n; i++) seg_stroke(xs[i], ys[i], xs[i + 1], ys[i + 1], c);
    if (closed && n > 1) seg_stroke(xs[n - 1], ys[n - 1], xs[0], ys[0], c);
}

// --- Frame lifecycle --------------------------------------------------------
void gfx_begin_frame(void) {
    s_verts.clear();
    s_cr = s_cg = s_cb = 0; s_ca = 1;
}

void gfx_clear(Color c) {
    s_cr = c.r / 255.0f; s_cg = c.g / 255.0f; s_cb = c.b / 255.0f; s_ca = c.a / 255.0f;
}

void gfx_end_frame(void) {
    id<CAMetalDrawable> drawable = [s_layer nextDrawable];
    if (!drawable) return;

    dispatch_semaphore_wait(s_inflight, DISPATCH_TIME_FOREVER);
    int fi = s_frame_idx;
    s_frame_idx = (s_frame_idx + 1) % OK_INFLIGHT;

    id<MTLCommandBuffer> cmd = [s_queue commandBuffer];
    __block dispatch_semaphore_t sem = s_inflight;
    [cmd addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull b) { (void)b; dispatch_semaphore_signal(sem); }];

    MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture     = drawable.texture;
    rp.colorAttachments[0].loadAction  = MTLLoadActionClear;
    rp.colorAttachments[0].clearColor  = MTLClearColorMake(s_cr, s_cg, s_cb, s_ca);
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:rp];

    if (!s_verts.empty() && s_pipeline) {
        NSUInteger need = s_verts.size() * sizeof(GVert);
        if (s_vcap[fi] < need) {
            s_vbuf[fi] = [s_device newBufferWithLength:need options:MTLResourceStorageModeShared];
            s_vcap[fi] = need;
        }
        memcpy(s_vbuf[fi].contents, s_verts.data(), need);
        float uni[4] = { (float)s_fw, (float)s_fh, (float)s_ox, (float)s_oy };
        [enc setRenderPipelineState:s_pipeline];
        [enc setVertexBuffer:s_vbuf[fi] offset:0 atIndex:0];
        [enc setVertexBytes:uni length:sizeof(uni) atIndex:1];
        [enc setFragmentTexture:s_atlas atIndex:0];
        [enc setFragmentSamplerState:s_sampler atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:s_verts.size()];
    }
    [enc endEncoding];
    [cmd presentDrawable:drawable];
    [cmd commit];
}

// --- Primitives -------------------------------------------------------------
void gfx_rect(int x, int y, int w, int h, Color c) { quad_solid(x, y, w, h, c); }

void gfx_rect_lines(int x, int y, int w, int h, Color c) {
    quad_solid(x, y, w, 1, c);             // top
    quad_solid(x, y + h - 1, w, 1, c);     // bottom
    quad_solid(x, y, 1, h, c);             // left
    quad_solid(x + w - 1, y, 1, h, c);     // right
}

void gfx_line(int x1, int y1, int x2, int y2, Color c) {
    seg_stroke((float)x1, (float)y1, (float)x2, (float)y2, c);
}

void gfx_triangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    tri_solid(a.x, a.y, b.x, b.y, c.x, c.y, color);
}

// Segments per 90-degree corner arc, and per full circle. Matched to what the
// raylib backend asks for (GFX_ROUND_SEGMENTS = 6) so the two silhouettes agree.
#define ARC_SEGMENTS   6
#define CIRCLE_SEGMENTS 24

// raylib's roundness: the corner radius is roundness * min(w,h) / 2, so a card
// keeps the same relative corner at any scale.
static float corner_radius(int w, int h, float roundness) {
    float shortest = (w < h) ? (float)w : (float)h;
    float r = shortest * roundness * 0.5f;
    if (r < 0.0f) r = 0.0f;
    if (r > shortest * 0.5f) r = shortest * 0.5f;
    return r;
}

// Trace a rounded rectangle's perimeter into xs/ys. Returns the point count.
// Order: top edge, top-right arc, right edge, bottom-right arc, bottom edge,
// bottom-left arc, left edge, top-left arc. Capacity must be >= 4*(ARC+1)+4.
static int rounded_path(float x, float y, float w, float h, float r,
                        float* xs, float* ys) {
    struct { float cx, cy, a0; } corners[4] = {
        { x + w - r, y + r,     270.0f },  // top-right
        { x + w - r, y + h - r,   0.0f },  // bottom-right
        { x + r,     y + h - r,  90.0f },  // bottom-left
        { x + r,     y + r,     180.0f },  // top-left
    };
    int n = 0;
    xs[n] = x + r; ys[n] = y; n++;         // start of the top edge
    for (int ci = 0; ci < 4; ci++) {
        for (int s = 0; s <= ARC_SEGMENTS; s++) {
            float a = (corners[ci].a0 + 90.0f * s / ARC_SEGMENTS) * (float)M_PI / 180.0f;
            xs[n] = corners[ci].cx + cosf(a) * r;
            ys[n] = corners[ci].cy + sinf(a) * r;
            n++;
        }
    }
    return n;
}

void gfx_rect_rounded(int x, int y, int w, int h, float roundness, Color c) {
    if (w <= 0 || h <= 0) return;
    float r = corner_radius(w, h, roundness);
    if (r < 0.5f) { quad_solid(x, y, w, h, c); return; }
    float fx = (float)x, fy = (float)y, fw = (float)w, fh = (float)h;

    // Three bands cover everything except the four corner arcs.
    quad_solid(fx + r, fy, fw - 2 * r, fh, c);
    quad_solid(fx, fy + r, r, fh - 2 * r, c);
    quad_solid(fx + fw - r, fy + r, r, fh - 2 * r, c);

    // Corner arcs as fans from each corner's centre.
    float xs[4 * (ARC_SEGMENTS + 1) + 1], ys[4 * (ARC_SEGMENTS + 1) + 1];
    rounded_path(fx, fy, fw, fh, r, xs, ys);
    struct { float cx, cy; } cen[4] = {
        { fx + fw - r, fy + r }, { fx + fw - r, fy + fh - r },
        { fx + r,      fy + fh - r }, { fx + r,  fy + r },
    };
    int p = 1;   // xs[0] is the top-edge start; the arcs follow in order
    for (int ci = 0; ci < 4; ci++) {
        for (int s = 0; s < ARC_SEGMENTS; s++, p++)
            tri_solid(cen[ci].cx, cen[ci].cy, xs[p], ys[p], xs[p + 1], ys[p + 1], c);
        p++;     // skip the arc's closing point; the next arc starts fresh
    }
}

void gfx_rect_rounded_lines(int x, int y, int w, int h, float roundness, Color c) {
    if (w <= 0 || h <= 0) return;
    float r = corner_radius(w, h, roundness);
    if (r < 0.5f) { gfx_rect_lines(x, y, w, h, c); return; }
    float xs[4 * (ARC_SEGMENTS + 1) + 1], ys[4 * (ARC_SEGMENTS + 1) + 1];
    int n = rounded_path((float)x, (float)y, (float)w, (float)h, r, xs, ys);
    stroke_path(xs, ys, n, true, c);
}

void gfx_circle(float cx, float cy, float radius, Color c) {
    if (radius <= 0.0f) return;
    float prev_x = cx + radius, prev_y = cy;
    for (int i = 1; i <= CIRCLE_SEGMENTS; i++) {
        float a = 2.0f * (float)M_PI * i / CIRCLE_SEGMENTS;
        float px = cx + cosf(a) * radius, py = cy + sinf(a) * radius;
        tri_solid(cx, cy, prev_x, prev_y, px, py, c);
        prev_x = px; prev_y = py;
    }
}

void gfx_circle_lines(float cx, float cy, float radius, Color c) {
    if (radius <= 0.0f) return;
    float prev_x = cx + radius, prev_y = cy;
    for (int i = 1; i <= CIRCLE_SEGMENTS; i++) {
        float a = 2.0f * (float)M_PI * i / CIRCLE_SEGMENTS;
        float px = cx + cosf(a) * radius, py = cy + sinf(a) * radius;
        seg_stroke(prev_x, prev_y, px, py, c);
        prev_x = px; prev_y = py;
    }
}

// Text: port of raylib's DrawTextEx with the bundled Nunito font -- scaleFactor =
// fontSize/baseSize, and the same proportional tracking (fontSize*0.05) the
// raylib backend uses, so the two platforms lay out identically regardless of
// each atlas's bake size. Draws each glyph's atlas rect at its offset.
void gfx_text(const char* text, int x, int y, int font_size, Color c) {
    float scale = (float)font_size / OK_FONT_BASE_SIZE;
    float spacing = font_size * 0.05f;
    float pen = (float)x;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
        int cp = *p;
        OKGlyph g = ok_font_glyphs[glyph_of(cp)];
        if (cp != ' ') {
            float gx = pen + g.ox * scale, gy = y + g.oy * scale;
            float gw = g.rw * scale,       gh = g.rh * scale;
            float u0 = g.rx / (float)OK_FONT_ATLAS_W, v0 = g.ry / (float)OK_FONT_ATLAS_H;
            float u1 = (g.rx + g.rw) / (float)OK_FONT_ATLAS_W;
            float v1 = (g.ry + g.rh) / (float)OK_FONT_ATLAS_H;
            push(gx,      gy,      u0, v0, c); push(gx + gw, gy,      u1, v0, c); push(gx + gw, gy + gh, u1, v1, c);
            push(gx,      gy,      u0, v0, c); push(gx + gw, gy + gh, u1, v1, c); push(gx,      gy + gh, u0, v1, c);
        }
        float adv = (g.adv != 0) ? (float)g.adv : g.rw; // DrawTextEx uses recs.width when advanceX==0
        pen += adv * scale + spacing;
    }
}

// MeasureTextEx: sum advances (recs.width + offsetX when advanceX==0), scaled,
// plus inter-glyph spacing. Matches gfx_text's tracking so centering is correct.
int gfx_measure_text(const char* text, int font_size) {
    float spacing = font_size * 0.05f;
    float scale = (float)font_size / OK_FONT_BASE_SIZE;
    float tw = 0.0f;
    int count = 0;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
        OKGlyph g = ok_font_glyphs[glyph_of(*p)];
        tw += (g.adv != 0) ? (float)g.adv : (g.rw + g.ox);
        count++;
    }
    return (int)(tw * scale + (count > 0 ? (count - 1) : 0) * spacing);
}

// The atlas is built in gfx_metal_attach(); nothing to lazily load here.
void gfx_font_init(void) {}
