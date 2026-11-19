// The Pebble SDK, as far as the game uses it, for a phone. The watch draws
// into a 200x228 framebuffer of GColor8 bytes; so does this. The SDK's
// rect / circle / line / text calls, which the host test harness stubs
// out, are real here - the HUD, the pause menu and the map need them. The
// PCM stream the synth writes goes to an OpenSL queue.
#include "wr.h"          // SCR_W / SCR_H for this build
#include <string.h>
#include <stdlib.h>

#define W SCR_W
#define H SCR_H
#if WR_ANDROID
int g_scr_w = 600;                       // until the window says otherwise
static uint8_t FB[SCR_H * SCR_W_MAX];
#else
static uint8_t FB[SCR_H * SCR_W];        // the host harness at the watch's size
#endif
unsigned char *stub_fb(void) { return FB; }

// --- framebuffer as the SDK's GBitmap ---------------------------------------
struct GBitmap { int dummy; };
static GBitmap g_fb_obj;
GBitmap *graphics_capture_frame_buffer(GContext *c) { (void)c; return &g_fb_obj; }
void graphics_release_frame_buffer(GContext *c, GBitmap *b) { (void)c; (void)b; }
GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *b, uint16_t y) {
  (void)b; GBitmapDataRowInfo r = { FB + (size_t)y * W, 0, W - 1 }; return r;
}

// --- drawing ------------------------------------------------------------------
static uint8_t s_fill = 0xC0, s_stroke = 0xFF, s_text = 0xFF;
// The HUD, the menu and the map are laid out in the watch's 200x228
// coordinates; the framebuffer is bigger by this factor, so every SDK
// drawing call scales its coordinates on the way in.
#define S (SCR_H / 228)
static inline void px(int x, int y, uint8_t c) { if (x >= 0 && x < W && y >= 0 && y < H) FB[y * W + x] = c; }
void graphics_context_set_fill_color(GContext *c, GColor x)   { (void)c; s_fill = x.argb; }
void graphics_context_set_stroke_color(GContext *c, GColor x) { (void)c; s_stroke = x.argb; }
void graphics_context_set_text_color(GContext *c, GColor g)   { (void)c; s_text = g.argb; }

void graphics_fill_rect(GContext *c, GRect r, uint16_t rad, GCornerMask m) {
  (void)c; (void)m;
  r.origin.x *= S; r.origin.y *= S; r.size.w *= S; r.size.h *= S; rad = (uint16_t)(rad * S);
  for (int y = r.origin.y; y < r.origin.y + r.size.h; y++)
    for (int x = r.origin.x; x < r.origin.x + r.size.w; x++) {
      // rounded corners: skip the pixels outside the corner arcs
      int dx = 0, dy = 0;
      if (rad) {
        if (x < r.origin.x + rad) dx = r.origin.x + rad - x; else if (x >= r.origin.x + r.size.w - rad) dx = x - (r.origin.x + r.size.w - rad - 1);
        if (y < r.origin.y + rad) dy = r.origin.y + rad - y; else if (y >= r.origin.y + r.size.h - rad) dy = y - (r.origin.y + r.size.h - rad - 1);
        if (dx * dx + dy * dy > rad * rad) continue;
      }
      px(x, y, s_fill);
    }
}
void graphics_draw_rect(GContext *c, GRect r) {
  (void)c;
  r.origin.x *= S; r.origin.y *= S; r.size.w *= S; r.size.h *= S;
  for (int x = r.origin.x; x < r.origin.x + r.size.w; x++) { px(x, r.origin.y, s_stroke); px(x, r.origin.y + r.size.h - 1, s_stroke); }
  for (int y = r.origin.y; y < r.origin.y + r.size.h; y++) { px(r.origin.x, y, s_stroke); px(r.origin.x + r.size.w - 1, y, s_stroke); }
}
void graphics_draw_round_rect(GContext *c, GRect g, uint16_t r) { (void)r; graphics_draw_rect(c, g); }
void graphics_draw_line(GContext *c, GPoint a, GPoint b) {
  (void)c; a.x *= S; a.y *= S; b.x *= S; b.y *= S;
  int dx = abs(b.x - a.x), dy = -abs(b.y - a.y), sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1, err = dx + dy;
  int x = a.x, y = a.y;
  for (;;) { px(x, y, s_stroke); if (x == b.x && y == b.y) break; int e2 = 2 * err; if (e2 >= dy) { err += dy; x += sx; } if (e2 <= dx) { err += dx; y += sy; } }
}
void graphics_fill_circle(GContext *c, GPoint p, uint16_t r) {
  (void)c; p.x *= S; p.y *= S; r = (uint16_t)(r * S);
  for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= (int)r * r) px(p.x + x, p.y + y, s_fill);
}
void graphics_draw_circle(GContext *c, GPoint p, uint16_t r) {
  (void)c; p.x *= S; p.y *= S; r = (uint16_t)(r * S);
  for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) { int d = x * x + y * y; if (d <= (int)r * r && d > (int)(r - 1) * (r - 1)) px(p.x + x, p.y + y, s_stroke); }
}

// --- text: a 5x7 font, scaled to the requested system font's rough size ----
// Uppercase, digits and the few marks the HUD uses. Anything else is a gap.
static const uint8_t FONT[][7] = {
  {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
  {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
  {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
  {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},{0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
  {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x11,0x19,0x15,0x13,0x11,0x11},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
  {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},{0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
  {0x11,0x11,0x11,0x0A,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
  // digits 0-9
  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
  {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
  {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
  {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
  // $ : . ' - ,
  {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
  {0x0C,0x04,0x08,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x1F,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x0C,0x04,0x08},
};
static int glyph(char ch) {
  if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  if (ch >= '0' && ch <= '9') return 26 + ch - '0';
  switch (ch) { case '$': return 36; case ':': return 37; case '.': return 38; case '\'': return 39; case '-': return 40; case ',': return 41; }
  return -1;
}
// Font keys are strings; the size is in the name ("..._18", "..._14").
GFont fonts_get_system_font(const char *k) {
  int n = 0; for (const char *p = k; *p; p++) if (*p >= '0' && *p <= '9') n = n * 10 + (*p - '0');
  return (GFont)(uintptr_t)(n ? n : 14);
}
void graphics_draw_text(GContext *c, const char *t, GFont f, GRect b, GTextOverflowMode o, GTextAlignment a, void *x) {
  (void)c; (void)o; (void)x;
  b.origin.x *= S; b.origin.y *= S; b.size.w *= S; b.size.h *= S;
  int size = (int)(uintptr_t)f; int sc = (size >= 24 ? 3 : size >= 16 ? 2 : 1) * S;
  int cw = 6 * sc, ch = 8 * sc;
  int len = (int)strlen(t), tw = len * cw;
  // Left-aligned if it will not fit: this font is wider than Gothic, and a
  // centred overflow loses the start of the word.
  int x0 = b.origin.x + (tw > b.size.w ? 0 : a == GTextAlignmentCenter ? (b.size.w - tw) / 2 : a == GTextAlignmentRight ? b.size.w - tw : 0);
  int y0 = b.origin.y + (b.size.h - ch) / 2; if (y0 < b.origin.y) y0 = b.origin.y;
  for (int i = 0; i < len; i++) {
    int g = glyph(t[i]); if (g < 0) continue;
    for (int r = 0; r < 7; r++) for (int col = 0; col < 5; col++) if (FONT[g][r] & (0x10 >> col))
      for (int yy = 0; yy < sc; yy++) for (int xx = 0; xx < sc; xx++) px(x0 + i * cw + col * sc + xx, y0 + r * sc + yy, s_text);
  }
}

// --- the rest of the SDK: enough to satisfy the game's main.c-free build ----
Layer *layer_create(GRect f) { (void)f; return 0; }
void layer_destroy(Layer *l) { (void)l; }
void layer_set_update_proc(Layer *l, LayerUpdateProc p) { (void)l; (void)p; }
void layer_add_child(Layer *a, Layer *b) { (void)a; (void)b; }
void layer_mark_dirty(Layer *l) { (void)l; }
GRect layer_get_bounds(const Layer *l) { (void)l; return GRect(0, 0, W, H); }
Window *window_create(void) { return 0; }
void window_destroy(Window *w) { (void)w; }
Layer *window_get_root_layer(const Window *w) { (void)w; return 0; }
void window_set_background_color(Window *w, GColor c) { (void)w; (void)c; }
void window_set_window_handlers(Window *w, WindowHandlers h) { (void)w; (void)h; }
void window_stack_push(Window *w, bool a) { (void)w; (void)a; }
void window_stack_pop_all(bool a) { (void)a; }
void window_set_click_config_provider(Window *w, ClickConfigProvider p) { (void)w; (void)p; }
void window_raw_click_subscribe(ButtonId b, ClickHandler d, ClickHandler u, void *c) { (void)b; (void)d; (void)u; (void)c; }
void window_single_click_subscribe(ButtonId b, ClickHandler h) { (void)b; (void)h; }
void window_long_click_subscribe(ButtonId b, uint16_t d, ClickHandler x, ClickHandler y) { (void)b; (void)d; (void)x; (void)y; }
AppTimer *app_timer_register(uint32_t m, AppTimerCallback c, void *d) { (void)m; (void)c; (void)d; return 0; }
bool app_timer_cancel(AppTimer *t) { (void)t; return true; }
void app_event_loop(void) {}
void touch_service_subscribe(TouchServiceHandler h, void *c) { (void)h; (void)c; }
void touch_service_unsubscribe(void) {}
bool touch_service_is_enabled(void) { return true; }
ResHandle resource_get_handle(uint32_t id) { return (ResHandle)(uintptr_t)id; }
size_t resource_size(ResHandle h) { (void)h; return 0; }
size_t resource_load_byte_range(ResHandle h, uint32_t o, uint8_t *b, size_t n) { (void)h; (void)o; (void)b; (void)n; return 0; }

// --- audio: the synth's 8 kHz signed-8-bit stream, kept in a ring the
// OpenSL callback drains (see glue.c) ----------------------------------------
#define RING 8192
static signed char s_ring[RING]; static volatile unsigned s_rw, s_rr;
bool speaker_play_notes(const SpeakerNote *n, uint32_t c, uint8_t v) { (void)n; (void)c; (void)v; return true; }
bool speaker_play_tone(uint16_t h, uint32_t m, uint8_t v, SpeakerWaveform w) { (void)h; (void)m; (void)v; (void)w; return true; }
void speaker_stop(void) {}
bool speaker_stream_open(SpeakerPcmFormat f, uint8_t v) { (void)f; (void)v; return true; }
uint32_t speaker_stream_write(const void *d, uint32_t n) {
  const signed char *p = (const signed char *)d;
  for (uint32_t i = 0; i < n; i++) { if (s_rw - s_rr < RING) s_ring[s_rw++ % RING] = p[i]; }
  return n;
}
unsigned pebble_android_audio_pull(short *out, unsigned frames) {
  unsigned i = 0;
  for (; i < frames && s_rr < s_rw; i++) out[i] = (short)(s_ring[s_rr++ % RING] * 256);
  for (unsigned j = i; j < frames; j++) out[j] = 0;
  return i;
}
void speaker_stream_close(void) {}
void speaker_set_volume(uint8_t v) { (void)v; }
SpeakerStatus speaker_get_status(void) { return SpeakerStatusIdle; }
bool speaker_is_muted(void) { return false; }

