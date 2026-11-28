#include "pebble.h"
#include "wr.h"
#if WR_ANDROID
static uint8_t FB[SCR_H*SCR_W_MAX];
#else
static uint8_t FB[SCR_H*SCR_W];
#endif
unsigned char *stub_fb(void){ return FB; }
struct GBitmap { int dummy; };
static GBitmap g_fb_obj;
GBitmap *graphics_capture_frame_buffer(GContext *c){(void)c;return &g_fb_obj;}
void graphics_release_frame_buffer(GContext *c, GBitmap *b){(void)c;(void)b;}
GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *b, uint16_t y){(void)b;GBitmapDataRowInfo r={FB+(size_t)y*SCR_W,0,SCR_W-1};return r;}
void graphics_context_set_fill_color(GContext *c, GColor x){(void)c;(void)x;}
void graphics_context_set_stroke_color(GContext *c, GColor x){(void)c;(void)x;}
void graphics_fill_rect(GContext *c, GRect r, uint16_t a, GCornerMask m){(void)c;(void)r;(void)a;(void)m;}
void graphics_draw_rect(GContext *c, GRect r){(void)c;(void)r;}
void graphics_draw_circle(GContext *c, GPoint p, uint16_t r){(void)c;(void)p;(void)r;}
void graphics_draw_round_rect(GContext *c, GRect g, uint16_t r){(void)c;(void)g;(void)r;}
void graphics_draw_line(GContext *c, GPoint a, GPoint b){(void)c;(void)a;(void)b;}
GFont fonts_get_system_font(const char *k){(void)k;return 0;}
void graphics_context_set_text_color(GContext *c, GColor g){(void)c;(void)g;}
void graphics_draw_text(GContext *c,const char*t,GFont f,GRect b,GTextOverflowMode o,GTextAlignment a,void*x){(void)c;(void)t;(void)f;(void)b;(void)o;(void)a;(void)x;}
void graphics_fill_circle(GContext *c, GPoint p, uint16_t r){(void)c;(void)p;(void)r;}
void graphics_fill_radial(GContext *c, GRect r, GOvalScaleMode m, uint16_t i, int32_t a, int32_t b){(void)c;(void)r;(void)m;(void)i;(void)a;(void)b;}
Layer *layer_create(GRect f){(void)f;return 0;}
void layer_destroy(Layer *l){(void)l;}
void layer_set_update_proc(Layer *l, LayerUpdateProc p){(void)l;(void)p;}
void layer_add_child(Layer *a, Layer *b){(void)a;(void)b;}
void layer_mark_dirty(Layer *l){(void)l;}
GRect layer_get_bounds(const Layer *l){(void)l;return GRect(0,0,SCR_W,SCR_H);}
Window *window_create(void){return 0;}
void window_destroy(Window *w){(void)w;}
Layer *window_get_root_layer(const Window *w){(void)w;return 0;}
void window_set_background_color(Window *w, GColor c){(void)w;(void)c;}
void window_set_window_handlers(Window *w, WindowHandlers h){(void)w;(void)h;}
void window_stack_push(Window *w, bool a){(void)w;(void)a;}
void window_stack_pop_all(bool a){(void)a;}
void window_set_click_config_provider(Window *w, ClickConfigProvider p){(void)w;(void)p;}
void window_raw_click_subscribe(ButtonId b, ClickHandler d, ClickHandler u, void *c){(void)b;(void)d;(void)u;(void)c;}
void window_single_click_subscribe(ButtonId b, ClickHandler h){(void)b;(void)h;}
void window_long_click_subscribe(ButtonId b, uint16_t d, ClickHandler x, ClickHandler y){(void)b;(void)d;(void)x;(void)y;}
AppTimer *app_timer_register(uint32_t m, AppTimerCallback c, void *d){(void)m;(void)c;(void)d;return 0;}
bool app_timer_cancel(AppTimer *t){(void)t;return true;}
void app_event_loop(void){}
void touch_service_subscribe(TouchServiceHandler h, void *c){(void)h;(void)c;}
void touch_service_unsubscribe(void){}
bool touch_service_is_enabled(void){return true;}
bool speaker_play_notes(const SpeakerNote *n, uint32_t c, uint8_t v){(void)n;(void)c;(void)v;return true;}
bool speaker_play_tone(uint16_t h, uint32_t m, uint8_t v, SpeakerWaveform w){(void)h;(void)m;(void)v;(void)w;return true;}
void speaker_stop(void){}
bool speaker_stream_open(SpeakerPcmFormat f, uint8_t v){(void)f;(void)v;return true;}
signed char g_pcm[4096]; unsigned g_pcm_n; unsigned long g_pcm_total;
uint32_t speaker_stream_write(const void *d, uint32_t n){
  const signed char *p=(const signed char*)d;
  for(uint32_t i=0;i<n;i++){ if(g_pcm_n<sizeof g_pcm) g_pcm[g_pcm_n++]=p[i]; }
  g_pcm_total+=n; return n; }
void speaker_stream_close(void){}
void speaker_set_volume(uint8_t v){(void)v;}
SpeakerStatus speaker_get_status(void){return SpeakerStatusIdle;}
bool speaker_is_muted(void){return false;}

// Resources: the harness loads domain blobs straight off disk, so the
// monolith and overlay paths can both be exercised on the host.
ResHandle resource_get_handle(uint32_t id) { return (ResHandle)(uintptr_t)id; }
size_t resource_size(ResHandle h) { (void)h; return 0; }
size_t resource_load_byte_range(ResHandle h, uint32_t off, uint8_t *buf, size_t n) {
  (void)h; (void)off; (void)buf; (void)n; return 0;
}

// A PLACE swap while the old tenant is still executing would, on the watch,
// overwrite running code. Abort loudly here instead.
#include <stdlib.h>
#include <stdio.h>
void wr_swap_hook(uint8_t from, uint8_t to, int depth) {
  if (depth > 0 && from != 0) {
    fprintf(stderr, "FATAL: domain %d asked to be replaced by %d from inside its own call (depth %d)\n", from, to, depth);
    abort();
  }
}
