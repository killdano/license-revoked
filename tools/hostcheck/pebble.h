// Host-side stub of just the Pebble SDK surface Wrist Revoked touches.
// Used only to compile-check the game on a desktop. Not shipped.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
static inline GRect GRect_(int16_t x, int16_t y, int16_t w, int16_t h) {
  GRect r = { { x, y }, { w, h } }; return r;
}
#define GRect(x,y,w,h) GRect_((int16_t)(x),(int16_t)(y),(int16_t)(w),(int16_t)(h))

typedef union { uint8_t argb; } GColor;
typedef struct GContext GContext;
typedef struct GBitmap GBitmap;
typedef struct { uint8_t *data; int16_t min_x, max_x; } GBitmapDataRowInfo;

#define GColorBlack     ((GColor){ .argb = 0xC0 })
#define GColorWhite     ((GColor){ .argb = 0xFF })
#define GColorRed       ((GColor){ .argb = 0xF0 })
#define GColorGreen     ((GColor){ .argb = 0xCC })
#define GColorFolly     ((GColor){ .argb = 0xF1 })
#define GColorDarkGray  ((GColor){ .argb = 0xD5 })
#define GColorOrange    ((GColor){ .argb = 0xF4 })
#define GColorLightGray ((GColor){ .argb = 0xEA })
#define GColorPictonBlue ((GColor){ .argb = 0xD7 })
#define GColorMagenta ((GColor){ .argb = 0xF3 })
// Resources (the stub reads them from resources/ on disk, so the size tool
// and the harness see the same code the watch does).
// Opaque on purpose: inline stubs returning 0 let the compiler prove the
// loader always fails and delete it, which made the size tool under-report
// by hundreds of bytes.
typedef void *ResHandle;
ResHandle resource_get_handle(uint32_t id);
size_t resource_size(ResHandle h);
size_t resource_load_byte_range(ResHandle h, uint32_t off, uint8_t *buf, size_t n);
#define RESOURCE_ID_DOM_CITY  1
#define RESOURCE_ID_DOM_GHETTO 2
#define RESOURCE_ID_DOM_COAST 3
#define RESOURCE_ID_DOM_CLUB  4
#define RESOURCE_ID_DOM_STORE 5
#define RESOURCE_ID_DOM_DINER 6
#define RESOURCE_ID_DOM_AIR   7
#define RESOURCE_ID_DOM_TAXI  8
#define RESOURCE_ID_DOM_AIRPORT 9
#define RESOURCE_ID_DOM_MENU 10
#define RESOURCE_ID_DOM_AIRSIDE 18
#define RESOURCE_ID_DOM_ARREST 19
#define RESOURCE_ID_DOM_H_FLY 20
#define RESOURCE_ID_DOM_GUARD 21
#define RESOURCE_ID_DOM_H_DEAD 22
#define RESOURCE_ID_DOM_SECRET 23
#define RESOURCE_ID_DOM_HOME 24
#define RESOURCE_ID_DOM_ISLAND 25
#define RESOURCE_ID_DOM_H_MELEE 11
#define RESOURCE_ID_DOM_H_GUNS 12
#define RESOURCE_ID_DOM_H_FLAME 13
#define RESOURCE_ID_DOM_H_ROCKET 14
#define RESOURCE_ID_DOM_H_CAR 15
#define RESOURCE_ID_DOM_H_BOARD 16
#define RESOURCE_ID_DOM_H_GRENADE 17

#define GColorYellow ((GColor){ .argb = 0xFC })
#define GColorBlue      ((GColor){ .argb = 0xC3 })

typedef enum { GCornerNone = 0, GCornersAll = 0xF } GCornerMask;

GBitmap *graphics_capture_frame_buffer(GContext *ctx);
void graphics_release_frame_buffer(GContext *ctx, GBitmap *fb);
GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *bmp, uint16_t y);
void graphics_context_set_fill_color(GContext *ctx, GColor c);
void graphics_context_set_stroke_color(GContext *ctx, GColor c);
void graphics_fill_rect(GContext *ctx, GRect r, uint16_t radius, GCornerMask m);
void graphics_draw_rect(GContext *ctx, GRect r);
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius);
void graphics_draw_round_rect(GContext *ctx, GRect r, uint16_t radius);
void graphics_draw_line(GContext *ctx, GPoint a, GPoint b);
typedef struct GFontType *GFont;
typedef enum { GTextOverflowModeWordWrap, GTextOverflowModeTrailingEllipsis, GTextOverflowModeFill } GTextOverflowMode;
typedef enum { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight } GTextAlignment;
#define FONT_KEY_GOTHIC_18 "RESOURCE_ID_GOTHIC_18"
#define FONT_KEY_GOTHIC_18_BOLD "RESOURCE_ID_GOTHIC_18_BOLD"
#define FONT_KEY_GOTHIC_14 "RESOURCE_ID_GOTHIC_14"
#define FONT_KEY_GOTHIC_14_BOLD "RESOURCE_ID_GOTHIC_14_BOLD"
typedef struct { const uint32_t *durations; uint32_t num_segments; } VibePattern;
static inline void vibes_short_pulse(void) {}
static inline void vibes_enqueue_custom_pattern(VibePattern p) { (void)p; }
static inline void vibes_cancel(void) {}

// Persistent storage, backed by a small array so the harness can round-trip.
#define WR_STUB_PERSIST 16
static int32_t stub_persist_v[WR_STUB_PERSIST];
static int     stub_persist_h[WR_STUB_PERSIST];
static inline bool persist_exists(uint32_t k) {
  return k < WR_STUB_PERSIST && stub_persist_h[k];
}
static inline int32_t persist_read_int(uint32_t k) {
  return (k < WR_STUB_PERSIST) ? stub_persist_v[k] : 0;
}
static inline int persist_write_int(uint32_t k, int32_t v) {
  if (k >= WR_STUB_PERSIST) return 0;
  stub_persist_v[k] = v; stub_persist_h[k] = 1; return 4;
}
#define FONT_KEY_GOTHIC_28_BOLD "RESOURCE_ID_GOTHIC_28_BOLD"
GFont fonts_get_system_font(const char *key);
void graphics_context_set_text_color(GContext *ctx, GColor c);
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                        GTextOverflowMode o, GTextAlignment a, void *attr);
#define GColorPictonBlue ((GColor){ .argb = 0xD7 })
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius);
// The round watch's bezel arcs (WR_ROUND builds only).
typedef enum { GOvalScaleModeFitCircle, GOvalScaleModeFillCircle } GOvalScaleMode;
#define TRIG_MAX_ANGLE 0x10000
#define DEG_TO_TRIGANGLE(angle) (((angle) * TRIG_MAX_ANGLE) / 360)
void graphics_fill_radial(GContext *ctx, GRect rect, GOvalScaleMode mode, uint16_t inset,
                          int32_t angle_start, int32_t angle_end);
static inline GPoint GPoint_(int16_t x, int16_t y){ GPoint p={x,y}; return p; }
#define GPoint(x,y) GPoint_((int16_t)(x),(int16_t)(y))
#define GColorLightGray ((GColor){ .argb = 0xEA })
#define GColorBlue      ((GColor){ .argb = 0xC3 })

typedef struct Layer Layer;
typedef struct Window Window;
typedef void (*LayerUpdateProc)(Layer *, GContext *);
Layer *layer_create(GRect frame);
void layer_destroy(Layer *l);
void layer_set_update_proc(Layer *l, LayerUpdateProc p);
void layer_add_child(Layer *parent, Layer *child);
void layer_mark_dirty(Layer *l);
GRect layer_get_bounds(const Layer *l);

typedef void (*WindowHandler)(Window *);
typedef struct { WindowHandler load, appear, disappear, unload; } WindowHandlers;
Window *window_create(void);
void window_destroy(Window *w);
Layer *window_get_root_layer(const Window *w);
void window_set_background_color(Window *w, GColor c);
void window_set_window_handlers(Window *w, WindowHandlers h);
void window_stack_push(Window *w, bool animated);
void window_stack_pop_all(bool animated);

typedef struct ClickRecognizer *ClickRecognizerRef;
typedef void (*ClickHandler)(ClickRecognizerRef, void *);
typedef void (*ClickConfigProvider)(void *);
typedef enum { BUTTON_ID_BACK = 0, BUTTON_ID_UP, BUTTON_ID_SELECT, BUTTON_ID_DOWN } ButtonId;
void window_set_click_config_provider(Window *w, ClickConfigProvider p);
void window_raw_click_subscribe(ButtonId b, ClickHandler d, ClickHandler u, void *ctx);
void window_single_click_subscribe(ButtonId b, ClickHandler h);
void window_long_click_subscribe(ButtonId b, uint16_t delay, ClickHandler d, ClickHandler u);

typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *);
AppTimer *app_timer_register(uint32_t ms, AppTimerCallback cb, void *data);
bool app_timer_cancel(AppTimer *t);
void app_event_loop(void);

typedef enum { TouchEvent_Touchdown, TouchEvent_Liftoff, TouchEvent_PositionUpdate } TouchEventType;
typedef struct { TouchEventType type; int16_t x, y; } TouchEvent;
typedef void (*TouchServiceHandler)(const TouchEvent *, void *);
void touch_service_subscribe(TouchServiceHandler h, void *ctx);
void touch_service_unsubscribe(void);
bool touch_service_is_enabled(void);

typedef enum {
  SpeakerWaveformSine, SpeakerWaveformSquare,
  SpeakerWaveformTriangle, SpeakerWaveformSawtooth, SpeakerWaveformCount
} SpeakerWaveform;
typedef enum { SpeakerStatusIdle, SpeakerStatusPlaying, SpeakerStatusDraining } SpeakerStatus;
typedef enum {
  SpeakerPcmFormat_8kHz_8bit, SpeakerPcmFormat_16kHz_8bit,
  SpeakerPcmFormat_8kHz_16bit, SpeakerPcmFormat_16kHz_16bit, SpeakerPcmFormatCount
} SpeakerPcmFormat;
bool     speaker_stream_open(SpeakerPcmFormat f, uint8_t vol);
uint32_t speaker_stream_write(const void *data, uint32_t n);
void     speaker_stream_close(void);
typedef struct {
  uint8_t midi_note; uint8_t waveform; uint16_t duration_ms;
  uint8_t velocity; uint8_t reserved;
} SpeakerNote;
bool speaker_play_notes(const SpeakerNote *notes, uint32_t n, uint8_t vol);
bool speaker_play_tone(uint16_t hz, uint32_t ms, uint8_t vol, SpeakerWaveform w);
void speaker_stop(void);
void speaker_set_volume(uint8_t v);
SpeakerStatus speaker_get_status(void);
bool speaker_is_muted(void);
