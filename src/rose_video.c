/* Rose Recompiled presentation and experimental horizontal-room expansion.
 * The NES keeps running its native 256x240 picture.  This module adds a
 * 16:9 presentation, reconstructs the loaded level at both sides, and asks
 * the original object factory to activate ordinary enemies in that viewport.
 */
#include "cyc_core.h"
#include "cyc_host_extras.h"
#include "cyc_mod.h"
#include "cyc_render.h"
#include "cyc_run.h"
#include "cyc_video.h"
#include "hw.h"
#include "hw_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VIEW_ORIGINAL, VIEW_STRETCH, VIEW_WIDE };
enum { NATIVE_W = 256, WIDE_W = 426, FRAME_H = 240, MAP_H = 192, HUD_Y = 176, ENEMY_LIFT = 4 };

typedef struct {
    int view, scene, cam_x, cam_y, wide_x, origin, map_w, map_h;
    unsigned long long frames, wide_frames, fallback_frames;
    unsigned long long spawn_attempts, spawned, rendered_objects;
    unsigned long long bg_compared, bg_mismatch;
    unsigned long long bg_best_mismatch;
    int bg_best_dx, bg_best_dy;
    unsigned long long signature_failures, isolate_failures;
    char stats_path[1024];
} RoseVideo;

static RoseVideo s;
static uint32_t stretch_canvas[WIDE_W * FRAME_H];
static uint8_t wide_coverage[WIDE_W * FRAME_H];
static uint32_t reconstructed[WIDE_W * FRAME_H];
static uint8_t captured_chr[0x2000];
static uint32_t captured_color[32];
static int capture_valid;

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }

static int wrap_delta(int value, int reference, int modulus)
{
    int d = (value - reference) % modulus;
    if (d < -modulus / 2) d += modulus;
    if (d >= modulus / 2) d -= modulus;
    return d;
}

static const uint8_t *prg_bank_ptr(unsigned bank, uint16_t address, size_t need)
{
    size_t at = (size_t)bank * 0x2000u + (address & 0x1fffu);
    if (!hw_cart.prg || at > hw_cart.prg_len || need > hw_cart.prg_len - at) return NULL;
    return hw_cart.prg + at;
}

static bool supported_rom(void)
{
    /* US PRG: fixed-bank bytes at the two routines this module relies on. */
    static const uint8_t onpos[] = { 0xbd,0x74,0x60,0x38,0xe5,0xdd,0x85,0x6a };
    static const uint8_t spawn[] = { 0x84,0x03,0xa0,0x08,0xa5,0x03,0xd9,0x7f,0x05 };
    const uint8_t *a = prg_bank_ptr(62, 0xd9bb, sizeof onpos);
    const uint8_t *b = prg_bank_ptr(63, 0xe9bb, sizeof spawn);
    return hw_cart.prg_len == 524288u && a && b &&
           !memcmp(a, onpos, sizeof onpos) && !memcmp(b, spawn, sizeof spawn);
}

static void capture_video_banks(void)
{
    for (unsigned i = 0; i < sizeof captured_chr; ++i)
        captured_chr[i] = cyc_render_chr((uint16_t)i);
    for (int i = 0; i < 32; ++i) captured_color[i] = cyc_render_color(i);
    capture_valid = 1;
}

static void draw_captured_tile(uint32_t *out, int x, int y, uint16_t pattern,
                               int palette, unsigned flags, uint8_t *coverage)
{
    for (int r = 0; r < 8; ++r) {
        int py = y + r;
        if (py < 0 || py >= FRAME_H) continue;
        int row = (flags & CYC_TILE_VFLIP) ? 7 - r : r;
        uint8_t lo = captured_chr[(pattern + row) & 0x1fff];
        uint8_t hi = captured_chr[(pattern + row + 8) & 0x1fff];
        for (int c = 0; c < 8; ++c) {
            int px = x + c;
            if (px < 0 || px >= WIDE_W) continue;
            int bit = (flags & CYC_TILE_HFLIP) ? c : 7 - c;
            int v = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
            if (!v) continue;
            size_t at = (size_t)py * WIDE_W + (size_t)px;
            if ((flags & CYC_TILE_BEHIND) && wide_coverage[at]) continue;
            out[at] = captured_color[(palette * 4 + v) & 31];
            if (coverage) coverage[at] = 1;
        }
    }
}

static void request_view(void)
{
    cyc_video_request_width(s.view == VIEW_ORIGINAL ? NATIVE_W : WIDE_W);
}

static bool refresh_scene(void)
{
    const uint8_t *ram = cyc_cpu_ram();
    size_t wn = 0;
    const uint8_t *wram = cyc_cart_ram(&wn);
    s.scene = 0;
    if (s.view != VIEW_WIDE || !supported_rom() || !wram || wn < 0x2000) return false;
    if (ram[0x51e] != 1 || ram[0x37] != 0xff) return false;
    int mw = wram[0x7ee], mh = wram[0x7ef];
    if (mw < 2 || mw > 16 || mh != 1) return false;
    int cx = le16(&ram[0xdd]);
    int cy = le16(&ram[0xe0]);
    cx += wrap_delta(cyc_render_line_scroll_x(8), cx & 511, 512);
    cy += wrap_delta(cyc_render_line_scroll_y(8), cy % 480, 480);
    int max_x = mw * 256 - WIDE_W;
    int wx = cx - (WIDE_W - NATIVE_W) / 2;
    if (wx < 0) wx = 0;
    if (wx > max_x) wx = max_x;
    /* The room renderer reserves eight scanlines above its 192-pixel map;
       camera_y therefore corresponds to map row 8 at display row 0. */
    s.cam_x = cx; s.cam_y = cy - 8; s.wide_x = wx; s.origin = cx - wx;
    s.map_w = mw; s.map_h = mh; s.scene = 1;
    return true;
}

static void apply_culling(void)
{
    cyc_mod_clear_rom_read_overrides();
    if (!s.scene) return;
    int left = 16 + s.origin;
    int right = 16 + (WIDE_W - NATIVE_W - s.origin);
    cyc_mod_set_rom_read_override(0xd9d3, (uint8_t)(0x100 - left));
    cyc_mod_set_rom_read_override(0xd9dd, (uint8_t)right);
}

static void commit_differences(const uint8_t *rb, const uint8_t *ra,
                               const uint8_t *wb, const uint8_t *wa)
{
    for (unsigned a = 0x70; a <= 0xdb; ++a)
        if (rb[a] != ra[a]) cyc_mod_poke((uint16_t)a, ra[a]);
    for (unsigned a = 0x57f; a <= 0x587; ++a)
        if (rb[a] != ra[a]) cyc_mod_poke((uint16_t)a, ra[a]);
    for (unsigned o = 0x32; o <= 0x35; ++o)
        if (wb[o] != wa[o]) cyc_mod_poke((uint16_t)(0x6000 + o), wa[o]);
    for (unsigned o = 0x74; o <= 0xff; ++o)
        if (wb[o] != wa[o]) cyc_mod_poke((uint16_t)(0x6000 + o), wa[o]);
    for (unsigned o = 0x128; o <= 0x3ff; ++o)
        if (wb[o] != wa[o]) cyc_mod_poke((uint16_t)(0x6000 + o), wa[o]);
}

static bool spawn_record(unsigned record, unsigned screen)
{
    const uint8_t *ram = cyc_cpu_ram();
    size_t wn = 0;
    const uint8_t *wram = cyc_cart_ram(&wn);
    if (!wram || wn < 0x2000) return false;
    uint8_t rb[0x800], ra[0x800], wb[0x800], wa[0x800];
    memcpy(rb, ram, sizeof rb); memcpy(wb, wram, sizeof wb);
    if (!cyc_mod_isolate_begin()) { s.isolate_failures++; return false; }
    cyc_mod_poke(0x4b, (uint8_t)screen);
    cyc_mod_poke(0x4c, 0);
    CycModRegs bank = { ram[0x57d], 0, 0, 0xfd, 0x24, 0 };
    bool ok = cyc_mod_call(0xf052, &bank);
    CycModRegs call = { 0, 0, (uint8_t)record, 0xfd, 0x24, 0 };
    ok = ok && cyc_mod_call(0xe9bb, &call);
    memcpy(ra, cyc_cpu_ram(), sizeof ra);
    memcpy(wa, cyc_cart_ram(&wn), sizeof wa);
    cyc_mod_isolate_end();
    if (!ok) { s.isolate_failures++; return false; }
    int created = 0;
    for (unsigned slot = 9; slot < 18; ++slot)
        if (ra[0x576 + slot] == record && (wa[0x128 + slot] & 0x80) == 0 &&
            (rb[0x576 + slot] != record || (wb[0x128 + slot] & 0x80))) created = 1;
    if (!created) return false;
    commit_differences(rb, ra, wb, wa);
    return true;
}

static void activate_wide_objects(void)
{
    if (!s.scene) return;
    const uint8_t *ram = cyc_cpu_ram();
    uint8_t bank = ram[0x57d];
    unsigned screens = ram[0x57e];
    uint16_t counts_a = le16(&ram[0xed]);
    uint16_t pos_a = le16(&ram[0xef]);
    uint16_t type_a = le16(&ram[0xf1]);
    if (!screens || screens > 16 || screens != (unsigned)(type_a - pos_a ? screens : 0)) return;
    const uint8_t *counts = prg_bank_ptr(bank, counts_a, screens);
    if (!counts) return;
    unsigned total = counts[screens - 1];
    const uint8_t *positions = prg_bank_ptr(bank, pos_a, total);
    const uint8_t *types = prg_bank_ptr(bank, type_a, total);
    if (!positions || !types) return;
    for (unsigned i = 0, screen = 0; i < total; ++i) {
        while (screen < screens && i >= counts[screen]) ++screen;
        if (screen >= screens || types[i] >= 0x17) continue;
        int x = (int)screen * 256 + (positions[i] >> 4) * 16 + 8;
        int in_wide = x >= s.wide_x - 16 && x < s.wide_x + WIDE_W + 16;
        int in_native = x >= s.cam_x - 16 && x < s.cam_x + NATIVE_W + 16;
        if (!in_wide || in_native) continue;
        s.spawn_attempts++;
        if (spawn_record(i, screen)) s.spawned++;
        break; /* one factory transaction per frame is enough */
    }
}

static void draw_background(uint32_t *out)
{
    size_t wn = 0;
    const uint8_t *w = cyc_cart_ram(&wn);
    if (!w || wn < 0x2000) return;
    memset(wide_coverage, 0, sizeof wide_coverage);
    uint32_t backdrop = captured_color[0];
    for (int i = 0; i < WIDE_W * HUD_Y; ++i) out[i] = backdrop;
    int x0 = (s.wide_x / 8) * 8;
    int y0 = (s.cam_y / 8) * 8;
    uint16_t table = cyc_render_line_bg_table(8);
    for (int wy = y0; wy < s.cam_y + HUD_Y + 8; wy += 8) {
        if (wy < 0 || wy >= s.map_h * MAP_H) continue;
        for (int wx = x0; wx < s.wide_x + WIDE_W + 8; wx += 8) {
            if (wx < 0 || wx >= s.map_w * 256) continue;
            int screen_index = (wy / MAP_H) * s.map_w + wx / 256;
            uint8_t screen_id = w[0x7f6 + screen_index];
            size_t map_at = 0x8c8u + (size_t)screen_id * 192u +
                            (size_t)((wy % MAP_H) / 16) * 16u + (size_t)((wx & 255) / 16);
            if (map_at >= wn) continue;
            uint8_t meta = w[map_at];
            size_t q = ((wy & 8) ? 0x1c00u : 0x1a00u) + ((wx & 8) ? 0x100u : 0u);
            uint8_t tile = w[q + meta];
            int pal = w[0x1e00u + meta] & 3;
            draw_captured_tile(out, wx - s.wide_x, wy - s.cam_y,
                               (uint16_t)(table + tile * 16u), pal, 0, wide_coverage);
        }
    }
}

static void draw_object_tile(uint32_t *out, int x, int y, uint8_t tile, uint8_t attr)
{
    unsigned common = (attr & 0x40 ? CYC_TILE_HFLIP : 0) |
                      (attr & 0x20 ? CYC_TILE_BEHIND : 0);
    uint16_t base = (uint16_t)(((tile & 1) ? 0x1000 : 0) + (tile & 0xfe) * 16);
    if (attr & 0x80) {
        draw_captured_tile(out, x, y, base + 16, 4 + (attr & 3), common | CYC_TILE_VFLIP, NULL);
        draw_captured_tile(out, x, y + 8, base, 4 + (attr & 3), common | CYC_TILE_VFLIP, NULL);
    } else {
        draw_captured_tile(out, x, y, base, 4 + (attr & 3), common, NULL);
        draw_captured_tile(out, x, y + 8, base + 16, 4 + (attr & 3), common, NULL);
    }
}

static void draw_wide_objects(uint32_t *out)
{
    size_t wn = 0;
    const uint8_t *w = cyc_cart_ram(&wn);
    if (!w || wn < 0x2000) return;
    for (int slot = 17; slot >= 9; --slot) {
        if ((w[0x128 + slot] & 0x80) || w[0x2c6 + slot] == 0xff || w[0x2fc + slot] >= 0x80) continue;
        if (w[0x182 + slot] != 0x89 || w[0x194 + slot] != 0xda) continue;
        unsigned bank = w[0x2fc + slot];
        uint16_t table = (uint16_t)(w[0x2d8 + slot] | (uint16_t)w[0x2ea + slot] << 8);
        uint8_t pose = w[0x2c6 + slot];
        const uint8_t *pp = prg_bank_ptr(bank, (uint16_t)(table + pose * 2u), 2);
        if (!pp) continue;
        uint16_t map = le16(pp);
        const uint8_t *p = prg_bank_ptr(bank, map, 1);
        if (!p || p[0] > 32) continue;
        size_t need = 1u + (size_t)p[0] * 4u;
        p = prg_bank_ptr(bank, map, need);
        if (!p) continue;
        int world_x = w[0x74 + slot] | (int)w[0x86 + slot] << 8;
        int world_y = w[0x98 + slot] | (int)w[0xaa + slot] << 8;
        /* The background map has an eight-line display bias. Object screen
           coordinates already subtract the unadjusted game camera. */
        int base_x = world_x - s.wide_x, base_y = world_y - (s.cam_y + 8);
        for (unsigned n = 0; n < p[0]; ++n) {
            const uint8_t *e = &p[1 + n * 4];
            draw_object_tile(out, base_x + (int8_t)e[0], base_y + (int8_t)e[1] + 1 - ENEMY_LIFT,
                             e[2] ^ 1, e[3]);
        }
        s.rendered_objects++;
    }
}

static int oam_is_default_enemy(const uint8_t *o)
{
    size_t wn = 0;
    const uint8_t *w = cyc_cart_ram(&wn);
    if (!w || wn < 0x2000) return 0;
    for (int slot = 9; slot < 18; ++slot) {
        if ((w[0x128 + slot] & 0x80) || w[0x2c6 + slot] == 0xff || w[0x2fc + slot] >= 0x80) continue;
        if (w[0x182 + slot] != 0x89 || w[0x194 + slot] != 0xda) continue;
        unsigned bank = w[0x2fc + slot];
        uint16_t table = (uint16_t)(w[0x2d8 + slot] | (uint16_t)w[0x2ea + slot] << 8);
        const uint8_t *pp = prg_bank_ptr(bank, (uint16_t)(table + w[0x2c6 + slot] * 2u), 2);
        if (!pp) continue;
        uint16_t map = le16(pp);
        const uint8_t *p = prg_bank_ptr(bank, map, 1);
        if (!p || p[0] > 32) continue;
        p = prg_bank_ptr(bank, map, 1u + (size_t)p[0] * 4u);
        if (!p) continue;
        int world_x = w[0x74 + slot] | (int)w[0x86 + slot] << 8;
        int world_y = w[0x98 + slot] | (int)w[0xaa + slot] << 8;
        int bx = world_x - s.cam_x, by = world_y - (s.cam_y + 8);
        for (unsigned n = 0; n < p[0]; ++n) {
            const uint8_t *e = &p[1 + n * 4];
            if (o[3] == (uint8_t)(bx + (int8_t)e[0]) &&
                o[0] == (uint8_t)(by + (int8_t)e[1]) &&
                o[1] == (uint8_t)(e[2] ^ 1) && o[2] == e[3]) return 1;
        }
    }
    return 0;
}

static void draw_native_oam(uint32_t *out)
{
    const uint8_t *oam = cyc_render_oam();
    for (int slot = 63; slot >= 0; --slot) {
        const uint8_t *o = &oam[slot * 4];
        int y = o[0] + 1;
        if (o[0] >= 0xef || !(cyc_render_line_mask(y < FRAME_H ? y : FRAME_H - 1) & 0x10)) continue;
        if (oam_is_default_enemy(o)) continue;
        int x = s.origin + o[3];
        unsigned flags = (o[2] & 0x40 ? CYC_TILE_HFLIP : 0) |
                         (o[2] & 0x80 ? CYC_TILE_VFLIP : 0) |
                         (o[2] & 0x20 ? CYC_TILE_BEHIND : 0);
        if (cyc_render_line_sprite16(y < FRAME_H ? y : FRAME_H - 1)) {
            draw_object_tile(out, x, y, o[1], o[2]);
        } else {
            uint16_t pattern = (uint16_t)(cyc_render_line_sprite_table(y) + o[1] * 16u);
            draw_captured_tile(out, x, y, pattern, 4 + (o[2] & 3), flags, NULL);
        }
    }
}

static int compose(uint32_t *out, int width, int height, int native_x0,
                   const uint32_t *native, void *ctx)
{
    (void)ctx; (void)native_x0;
    if (width != WIDE_W || height != FRAME_H) return 0;
    if (s.view == VIEW_STRETCH) {
        for (int y = 0; y < FRAME_H; ++y)
            for (int x = 0; x < WIDE_W; ++x)
                out[y * WIDE_W + x] = native[y * NATIVE_W + x * NATIVE_W / WIDE_W];
        return 1;
    }
    if (s.view != VIEW_WIDE || !capture_valid || !refresh_scene()) { s.fallback_frames++; return 0; }
    draw_background(reconstructed);
    uint32_t native_bg[NATIVE_W * FRAME_H];
    if (cyc_render_background(native_bg)) {
        for (int y = 8; y < HUD_Y; ++y) for (int x = 0; x < NATIVE_W; ++x) {
            s.bg_compared++;
            if (reconstructed[y * WIDE_W + s.origin + x] != native_bg[y * NATIVE_W + x]) s.bg_mismatch++;
        }
        unsigned long long best = ~(unsigned long long)0;
        for (int dy = -8; dy <= 8; ++dy) for (int dx = -8; dx <= 8; ++dx) {
            unsigned long long bad = 0, total = 0;
            for (int y = 16; y < HUD_Y - 8; ++y) for (int x = 8; x < NATIVE_W - 8; ++x) {
                int sx = s.origin + x + dx, sy = y + dy;
                if (sx < 0 || sx >= WIDE_W || sy < 0 || sy >= HUD_Y) continue;
                total++;
                if (reconstructed[sy * WIDE_W + sx] != native_bg[y * NATIVE_W + x]) bad++;
            }
            if (bad < best) { best = bad; s.bg_best_mismatch = bad; s.bg_best_dx = dx; s.bg_best_dy = dy; }
            (void)total;
        }
    }
    draw_wide_objects(reconstructed);
    draw_native_oam(reconstructed);
    /* Keep the native HUD proportions, while extending its edge colors so
       it does not form two black vertical bars below the wide playfield. */
    for (int y = HUD_Y; y < FRAME_H; ++y) {
        /* Native columns 0..7 are the NES left-edge mask and are blue on
           this HUD. Start at column 8 so that mask cannot become a bar. */
        uint32_t left = native[y * NATIVE_W + 8], right = native[y * NATIVE_W + NATIVE_W - 1];
        for (int x = 0; x < s.origin + 8; ++x) reconstructed[y * WIDE_W + x] = left;
        memcpy(&reconstructed[y * WIDE_W + s.origin + 8], &native[y * NATIVE_W + 8],
               (NATIVE_W - 8) * sizeof *native);
        for (int x = s.origin + NATIVE_W; x < WIDE_W; ++x) reconstructed[y * WIDE_W + x] = right;
    }
    memcpy(out, reconstructed, sizeof reconstructed);
    cyc_render_set_native_origin(s.origin);
    s.wide_frames++;
    return 1;
}

static const uint32_t *present(void *ctx, int *width, int *height)
{
    (void)ctx;
    return cyc_render_present(width, height);
}

static int get_view(void *ctx) { (void)ctx; return s.view; }

static bool set_view(void *ctx, int mode)
{
    (void)ctx;
    if (mode < VIEW_ORIGINAL || mode > VIEW_WIDE) return false;
    s.view = mode; request_view(); refresh_scene(); apply_culling();
    return true;
}

static void report(void)
{
    if (!s.stats_path[0]) return;
    FILE *f = fopen(s.stats_path, "wb");
    if (!f) return;
    fprintf(f, "{\n  \"view\": %d,\n  \"frames\": %llu,\n  \"wide_frames\": %llu,\n"
               "  \"fallback_frames\": %llu,\n  \"spawn_attempts\": %llu,\n  \"spawned\": %llu,\n"
               "  \"rendered_objects\": %llu,\n  \"background_compared\": %llu,\n"
               "  \"background_mismatch\": %llu,\n  \"signature_failures\": %llu,\n"
               "  \"background_best_mismatch\": %llu,\n  \"background_best_dx\": %d,\n"
               "  \"background_best_dy\": %d,\n  \"isolate_failures\": %llu\n}\n",
            s.view, s.frames, s.wide_frames, s.fallback_frames, s.spawn_attempts,
            s.spawned, s.rendered_objects, s.bg_compared, s.bg_mismatch,
            s.signature_failures, s.bg_best_mismatch, s.bg_best_dx, s.bg_best_dy,
            s.isolate_failures);
    fclose(f);
}

static void power_on(void *ctx)
{
    (void)ctx;
    int selected = s.view;
    char path[sizeof s.stats_path]; memcpy(path, s.stats_path, sizeof path);
    memset(&s, 0, sizeof s); s.view = selected; memcpy(s.stats_path, path, sizeof path);
    static int once;
    if (!once) { once = 1; atexit(report); }
    cyc_render_set_compositor(compose, NULL);
    cyc_render_capture_background(true);
    capture_valid = 0;
    hw_observe_line = 8;
    cyc_run_observer = capture_video_banks;
    request_view(); refresh_scene(); apply_culling();
    if (!supported_rom()) s.signature_failures++;
}

static void frame_begin(void *ctx) { (void)ctx; apply_culling(); }

static void frame_end(void *ctx)
{
    (void)ctx; s.frames++;
    refresh_scene();
    activate_wide_objects();
}

static void state_loaded(void *ctx) { (void)ctx; refresh_scene(); apply_culling(); }

static const CycHostOption OPTIONS[] = {
    { "--rose-video", true, "original | stretch | wide" },
    { "--wide-stats", true, "write Rose widescreen diagnostics JSON" },
};

static bool option(void *ctx, const char *name, const char *value)
{
    (void)ctx;
    if (!strcmp(name, "--wide-stats")) {
        if (!value || !*value) return false;
        snprintf(s.stats_path, sizeof s.stats_path, "%s", value); return true;
    }
    if (strcmp(name, "--rose-video")) return false;
    int mode = !strcmp(value, "original") ? VIEW_ORIGINAL :
               !strcmp(value, "stretch") ? VIEW_STRETCH :
               !strcmp(value, "wide") ? VIEW_WIDE : -1;
    return mode >= 0 && set_view(NULL, mode);
}

static const CycHostExtras EXTRAS = {
    .present = present,
    .view_modes = 7,
    .get_view_mode = get_view,
    .set_view_mode = set_view,
    .power_on = power_on,
    .frame_begin = frame_begin,
    .frame_end = frame_end,
    .options = OPTIONS,
    .option_count = sizeof OPTIONS / sizeof OPTIONS[0],
    .option = option,
    .state_loaded = state_loaded,
};

const CycHostExtras *cyc_host_extras(void) { return &EXTRAS; }
