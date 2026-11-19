// License Revoked on a phone. The same game, rendered at twice the watch's
// resolution and filled to the screen's height. The controls are the
// phone's own: the left part of the screen is the movement stick (as on
// the watch); the rest is a trackpad that turns you, with inertia, like a
// Steam controller's; two translucent buttons on the right edge are SHOOT
// and CAR (get in / get out); Android's BACK is the pause menu.
#include <android/native_window.h>
#include <android/input.h>
#include <android/log.h>
#include <android_native_app_glue.h>
// OpenSL is deprecated from API 30 but still shipped and still works; AAudio
// is the modern path and can replace this later.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <string.h>
#include <math.h>
#include "wr.h"
#include "wr_domain.h"

#define SW SCR_W
#define SH SCR_H
#define FRAME_NS 40000000LL       // 25 fps, like the watch

extern unsigned char *stub_fb(void);
extern unsigned pebble_android_audio_pull(short *out, unsigned frames);
void render_frame(GContext *ctx, GRect bounds);

static struct android_app *s_app;
static bool s_ready;
static int s_w, s_h;                     // the display
static int s_bx;                         // right edge of the game / pad area

// --- the phone's controls -------------------------------------------------------
// ACTION, one button, the watch's SELECT: a tap attacks, a hold is the car
// door, and the press fires at once unless a vehicle is in reach (the rule
// the game already has). It is a separate pointer, so a finger held on it
// keeps firing while another finger turns you on the pad.
// Turning on foot: the pad, with inertia. In a car: the steering wheel, a
// real position that springs back to centre. Two fingers on the pad: the
// zoom, and once a pinch has started, single-finger turning is ignored
// until every finger has lifted.
static bool s_action_down;
static int32_t s_turn_vel;                 // angle units per frame, with inertia
static int s_act_pointer = -1, s_pad_pointer = -1, s_pad2_pointer = -1, s_stick_pointer = -1, s_wheel_pointer = -1;
static float s_pad_last_x, s_menu_acc; static bool s_pad_moved, s_pinched;
static float s_act_last_x; static bool s_act_off;     // the fire finger, slid off the button: it turns you
static float s_pinch_d0; static int16_t s_pinch_zoom0;
static int s_fingers;                      // pointers currently down, for the pinch latch
static float s_wheel_last_a;               // the finger's last angle about the wheel's centre
static int32_t s_wheel;                    // the wheel's position, -256..256

static int16_t s_tx, s_ty; static bool s_tpending, s_tdown;
static void touch_flush(void) { if (s_tpending) { s_tpending = false; game_touch_move(s_tx, s_ty); } }

// Layout, in display pixels. The action button is a disc at the bottom
// right; the wheel is a larger disc to its left, shown only in a car; the
// stick is the left third of the screen; the pad is everything else.
static int s_act_cx, s_act_cy, s_act_r, s_wh_cx, s_wh_cy, s_wh_r;
static bool in_disc(float fx, float fy, int cx, int cy, int r) { float dx = fx - cx, dy = fy - cy; return dx * dx + dy * dy <= (float)r * r; }
static bool in_stick(float fx) { return fx < s_bx / 3; }
// The weapon box (top left, or top right in mirror mode) is the game's to
// handle, whichever side it is on: its tap swaps the weapon.
static bool in_weapon_box(float fx, float fy) {
  int lx = (int)fx * 228 * SW / (SCR_H * s_w), ly = (int)fy * 228 / s_h;   // logical
  int wx = g_menu.cam_left ? ((SW * 228 / SCR_H) - 62) : 6;
  return lx >= wx && lx <= wx + 56 && ly >= 20 && ly <= 48;
}
static bool driving(void) { return g_p.in_car && (g_ent[g_p.in_car - 1].type == ENT_CAR || g_ent[g_p.in_car - 1].type == ENT_COPCAR || g_ent[g_p.in_car - 1].type == ENT_AMB); }

static int32_t on_input(struct android_app *app, AInputEvent *ev) {
  (void)app;
  if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
    if (AKeyEvent_getKeyCode(ev) == AKEYCODE_BACK) {
      if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) menu_toggle();
      return 1;
    }
    return 0;
  }
  if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
  int32_t action = AMotionEvent_getAction(ev);
  int masked = action & AMOTION_EVENT_ACTION_MASK;
  int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
  size_t n = AMotionEvent_getPointerCount(ev);
  bool down = masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN;
  bool up   = masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP || masked == AMOTION_EVENT_ACTION_CANCEL;
  if (down) s_fingers++;
  for (size_t i = 0; i < n; i++) {
    int pid = AMotionEvent_getPointerId(ev, i);
    float fx = AMotionEvent_getX(ev, i), fy = AMotionEvent_getY(ev, i);
    bool is_this = ((int)i == idx);
    if (down && is_this) {
      if (in_disc(fx, fy, s_act_cx, s_act_cy, s_act_r) && s_act_pointer < 0) {
        s_act_pointer = pid; s_act_last_x = fx; s_act_off = false;
        if (g_menu.open) menu_select(); else { s_action_down = true; game_input_hold(true); }
      } else if (driving() && !g_menu.open && in_disc(fx, fy, s_wh_cx, s_wh_cy, s_wh_r + s_wh_r / 4) && s_wheel_pointer < 0) {
        s_wheel_pointer = pid; s_wheel_last_a = atan2f(fy - s_wh_cy, fx - s_wh_cx);
      } else if ((in_stick(fx) || in_weapon_box(fx, fy)) && s_stick_pointer < 0 && !g_menu.open) {
        int16_t x = (int16_t)((int)fx * SW / s_w), y = (int16_t)((int)fy * SH / s_h);
        s_stick_pointer = pid; s_tdown = true; game_touch_down(x, y);
      } else if (s_pad_pointer < 0) {
        s_pad_pointer = pid; s_pad_last_x = fx; s_pad_moved = false; s_turn_vel = 0; s_menu_acc = 0;
      } else if (s_pad2_pointer < 0) {
        s_pad2_pointer = pid; s_pad_moved = true; s_pinched = true;      // a pinch: no turning until all fingers lift
        float ax = 0, ay = 0;
        for (size_t j = 0; j < n; j++) if (AMotionEvent_getPointerId(ev, j) == s_pad_pointer) { ax = AMotionEvent_getX(ev, j); ay = AMotionEvent_getY(ev, j); }
        float dx = fx - ax, dy = fy - ay; s_pinch_d0 = dx * dx + dy * dy; s_pinch_zoom0 = g_p.zoom;
      }
    } else if (masked == AMOTION_EVENT_ACTION_MOVE) {
      if (pid == s_act_pointer && s_action_down) {
        // Hold fire and slide: the button stays pressed (a flamethrower keeps
        // going), and once the finger is off it, its travel turns you - so
        // you can hose a crowd without letting go.
        if (!s_act_off && !in_disc(fx, fy, s_act_cx, s_act_cy, s_act_r)) s_act_off = true;
        if (s_act_off && !driving()) {
          float dx = fx - s_act_last_x;
          int32_t d = (int32_t)(dx * 65536.0f / (float)(s_bx - s_bx / 3));
          game_turn_by(d);
        }
        s_act_last_x = fx;
      } else if (pid == s_stick_pointer && s_tdown) {
        s_tx = (int16_t)((int)fx * SW / s_w); s_ty = (int16_t)((int)fy * SH / s_h); s_tpending = true;
      } else if (pid == s_wheel_pointer) {
        // The wheel turns with the finger's angle about its centre, up to
        // a little past a quarter turn each way.
        float a = atan2f(fy - s_wh_cy, fx - s_wh_cx), d = a - s_wheel_last_a;
        while (d > 3.14159f) d -= 6.28318f;
        while (d < -3.14159f) d += 6.28318f;
        s_wheel_last_a = a;
        s_wheel += (int32_t)(d * 256.0f / 1.9f);            // ~110 degrees = full lock
        if (s_wheel > 256) s_wheel = 256;
        if (s_wheel < -256) s_wheel = -256;
      } else if (pid == s_pad_pointer || pid == s_pad2_pointer) {
        if (s_pad2_pointer >= 0) {
          float ax = 0, ay = 0, bx = 0, by = 0;
          for (size_t j = 0; j < n; j++) { int q = AMotionEvent_getPointerId(ev, j); if (q == s_pad_pointer) { ax = AMotionEvent_getX(ev, j); ay = AMotionEvent_getY(ev, j); } if (q == s_pad2_pointer) { bx = AMotionEvent_getX(ev, j); by = AMotionEvent_getY(ev, j); } }
          float d2 = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
          if (s_pinch_d0 > 1.0f && d2 > 1.0f) {
            float ratio = d2 / s_pinch_d0;
            int z = (int)(s_pinch_zoom0 - (ratio - 1.0f) * 120.0f);
            if (z < 0) z = 0;
            if (z > ZOOM_MAX) z = ZOOM_MAX;
            g_p.zoom = (int16_t)z;
          }
        } else if (pid == s_pad_pointer && !s_pinched) {
          float dx = fx - s_pad_last_x; s_pad_last_x = fx;
          if (g_menu.open) {
            // The menu: a drag of about a finger's width per row, accumulated.
            s_menu_acc += dx;
            while (s_menu_acc > 90) { menu_nav(+1); s_menu_acc -= 90; }
            while (s_menu_acc < -90) { menu_nav(-1); s_menu_acc += 90; }
          } else if (!driving()) {
            int32_t d = (int32_t)(dx * 65536.0f / (float)(s_bx - s_bx / 3));
            game_turn_by(d); s_turn_vel = (s_turn_vel * 2 + d) / 3;
          }
          if (dx != 0) s_pad_moved = true;
        }
      }
    } else if (up && is_this) {
      if (pid == s_act_pointer) { s_act_pointer = -1; if (s_action_down) { s_action_down = false; game_input_hold(false); } }
      else if (pid == s_wheel_pointer) { s_wheel_pointer = -1; }
      else if (pid == s_stick_pointer) { s_stick_pointer = -1; if (s_tdown) { touch_flush(); game_touch_up(); s_tdown = false; } }
      else if (pid == s_pad_pointer) {
        s_pad_pointer = -1;
        if (!s_pad_moved && !s_pinched) {
          if (g_menu.open) {
            // Touch the row you want. Rows are at 34 + i*32 in the menu's
            // 228-tall logical space (see dom_menu.c); the map takes any tap.
            int ly = (int)fy * 228 / s_h;
            int row = (ly - 34) / 32;
            if (g_menu.map) menu_select();
            else if (row >= 0 && row < MENU_ROWS && ly >= 34) { g_menu.row = (uint8_t)row; menu_select(); }
          } else game_input_tap();
        }
        if (s_pad2_pointer >= 0) { s_pad_pointer = s_pad2_pointer; s_pad2_pointer = -1; s_pad_moved = true; }
      }
      else if (pid == s_pad2_pointer) { s_pad2_pointer = -1; s_pad_moved = true; }
      if (s_fingers > 0) s_fingers--;
      if (s_fingers == 0) s_pinched = false;               // everything lifted: the pad may turn again
    }
  }
  return 1;
}

// --- audio: OpenSL pulls from the synth's ring ---------------------------------
static SLObjectItf s_engine, s_mix, s_player; static SLPlayItf s_play; static SLAndroidSimpleBufferQueueItf s_bq;
static short s_abuf[2][320];
static void bq_cb(SLAndroidSimpleBufferQueueItf bq, void *ctx) {
  (void)ctx; static int k = 0; k ^= 1;
  pebble_android_audio_pull(s_abuf[k], 320);
  (*bq)->Enqueue(bq, s_abuf[k], sizeof s_abuf[k]);
}
static void audio_start(void) {
  if (slCreateEngine(&s_engine, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS) return;
  (*s_engine)->Realize(s_engine, SL_BOOLEAN_FALSE);
  SLEngineItf eng; (*s_engine)->GetInterface(s_engine, SL_IID_ENGINE, &eng);
  (*eng)->CreateOutputMix(eng, &s_mix, 0, NULL, NULL); (*s_mix)->Realize(s_mix, SL_BOOLEAN_FALSE);
  SLDataLocator_AndroidSimpleBufferQueue loc = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
  SLDataFormat_PCM fmt = { SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_8, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                           SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN };
  SLDataSource src = { &loc, &fmt };
  SLDataLocator_OutputMix oloc = { SL_DATALOCATOR_OUTPUTMIX, s_mix }; SLDataSink snk = { &oloc, NULL };
  const SLInterfaceID ids[1] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE }; const SLboolean req[1] = { SL_BOOLEAN_TRUE };
  if ((*eng)->CreateAudioPlayer(eng, &s_player, &src, &snk, 1, ids, req) != SL_RESULT_SUCCESS) return;
  (*s_player)->Realize(s_player, SL_BOOLEAN_FALSE);
  (*s_player)->GetInterface(s_player, SL_IID_PLAY, &s_play);
  (*s_player)->GetInterface(s_player, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &s_bq);
  (*s_bq)->RegisterCallback(s_bq, bq_cb, NULL);
  (*s_play)->SetPlayState(s_play, SL_PLAYSTATE_PLAYING);
  bq_cb(s_bq, NULL); bq_cb(s_bq, NULL);
}

// --- the screen ------------------------------------------------------------------
static uint32_t argb8_to_rgba(uint8_t c) {   // GColor8: 0bAARRGGBB, two bits each
  uint32_t r = ((c >> 4) & 3) * 85, g = ((c >> 2) & 3) * 85, b = (c & 3) * 85;
  return 0xFF000000u | (b << 16) | (g << 8) | r;   // RGBA_8888 in memory
}
// Blend a tint over a pixel.
static inline uint32_t tint(uint32_t c, uint32_t t, int a) {
  uint32_t r = ((c & 0xFF) * (255 - a) + (t & 0xFF) * a) / 255;
  uint32_t gg = (((c >> 8) & 0xFF) * (255 - a) + ((t >> 8) & 0xFF) * a) / 255;
  uint32_t bb = (((c >> 16) & 0xFF) * (255 - a) + ((t >> 16) & 0xFF) * a) / 255;
  return 0xFF000000u | (bb << 16) | (gg << 8) | r;
}
static void blit(void) {
  ANativeWindow *win = s_app->window; if (!win) return;
  ANativeWindow_Buffer buf; if (ANativeWindow_lock(win, &buf, NULL) < 0) return;
  uint32_t *dst = (uint32_t *)buf.bits; const unsigned char *fb = stub_fb();
  int w = buf.width, h = buf.height;
  static int xmap[4096]; if (w > 4096) w = 4096;
  for (int x = 0; x < w; x++) xmap[x] = (int)(((long long)x * SW) / w);
  bool wheel = driving() && !g_menu.open;
  float wa = (float)s_wheel * 1.9f / 256.0f, ca = cosf(wa), sa = sinf(wa);
  int R = s_wh_r, rim_in = R - R / 5;                          // rim thickness: a fifth of the radius
  int ymin = s_wh_cy - R - 4; if (ymin < 0) ymin = 0;
  int xmin = s_wh_cx - R - 4; if (xmin < 0) xmin = 0;
  for (int y = 0; y < h; y++) {
    uint32_t *row = dst + (size_t)y * buf.stride;
    const unsigned char *src = fb + (size_t)((long long)y * SH / h) * SW;
    for (int x = 0; x < w; x++) row[x] = argb8_to_rgba(src[xmap[x]]);
    if (y < ymin) continue;
    for (int x = xmin; x < w; x++) {
      int ex = x - s_wh_cx, ey = y - s_wh_cy; int e2 = ex * ex + ey * ey;
      if (e2 > (R + 3) * (R + 3)) continue;
      // the action button: the hub
      if (e2 <= s_act_r * s_act_r) {
        bool edge = e2 >= (s_act_r - 5) * (s_act_r - 5);
        row[x] = tint(row[x], edge ? 0xFFFFFFFFu : 0xFF3040E0u, edge ? 190 : s_act_pointer >= 0 ? 180 : 90);
        continue;
      }
      if (!wheel) continue;
      // the rim: thick, with a lighter outer bevel and a darker inner one
      if (e2 >= rim_in * rim_in) {
        bool outer = e2 >= (R - 3) * (R - 3), inner = e2 <= (rim_in + 3) * (rim_in + 3);
        row[x] = tint(row[x], outer ? 0xFFFFFFFFu : inner ? 0xFF202020u : 0xFF404040u, outer ? 150 : inner ? 140 : 120);
        continue;
      }
      // the spokes, rotated with the wheel: a Y - two to the upper rim, one down
      float rx = ex * ca + ey * sa, ry = -ex * sa + ey * ca;
      bool s1 = fabsf(rx) < 6 && ry > 0;                                   // down
      bool s2 = fabsf(rx * 0.5f + ry * 0.866f) < 6 && rx < 0;              // upper left
      bool s3 = fabsf(rx * 0.5f - ry * 0.866f) < 6 && rx > 0;              // upper right
      bool mark = fabsf(rx) < 4 && ry < -(rim_in - 14);                    // the top mark
      if (s1 || s2 || s3 || mark) row[x] = tint(row[x], mark ? 0xFF3040E0u : 0xFF303030u, mark ? 220 : 130);
      else row[x] = tint(row[x], 0xFF000000u, s_wheel_pointer >= 0 ? 40 : 20);   // the open wheel: faintly shaded
    }
  }
  ANativeWindow_unlockAndPost(win);
}
static void layout(void) {
  ANativeWindow *win = s_app->window; if (!win) return;
  s_w = ANativeWindow_getWidth(win); s_h = ANativeWindow_getHeight(win);
  ANativeWindow_setBuffersGeometry(win, s_w, s_h, WINDOW_FORMAT_RGBA_8888);
  int want = (int)((long long)SCR_H * s_w / s_h);
  if (want < PX(200)) want = PX(200);
  if (want > SCR_W_MAX) want = SCR_W_MAX;
  g_scr_w = want;
  s_bx = s_w;
  int shorter = s_w < s_h ? s_w : s_h;
  // The wheel sits in the bottom-right corner, whole, with a margin. In a
  // car the action button is its hub; on foot the button is the same disc,
  // alone, in the same place.
  s_wh_r = (shorter * 6) / 35;              // a fifth bigger than shorter/7
  s_wh_cx = s_w - s_wh_r - shorter / 40; s_wh_cy = s_h - s_wh_r - shorter / 40;
  s_act_r = s_wh_r / 2 + 4;
  s_act_cx = s_wh_cx; s_act_cy = s_wh_cy;
}
static void on_cmd(struct android_app *app, int32_t cmd) {
  (void)app;
  // A fold or unfold is a window resize on this device, not a new activity:
  // everything is re-laid out from the window's new size, and the game
  // state, which lives in C, is untouched.
  if (cmd == APP_CMD_INIT_WINDOW) { layout(); s_ready = true; }
  if (cmd == APP_CMD_TERM_WINDOW) s_ready = false;
  if (cmd == APP_CMD_CONFIG_CHANGED || cmd == APP_CMD_WINDOW_RESIZED || cmd == APP_CMD_CONTENT_RECT_CHANGED) layout();
}

void android_main(struct android_app *app) {
  s_app = app; app->onAppCmd = on_cmd; app->onInputEvent = on_input;
  wr_seed((uint32_t)time(NULL) ^ 0x5A17u); audio_init(); game_init(); audio_start();
  // The simulation runs at the watch's 25 Hz - every tuning constant in the
  // game is per-frame at that rate. Rendering runs as fast as the display
  // will take frames (the post blocks on vsync), so a turn on the pad, which
  // moves the camera between simulation steps, is drawn smoothly at 120 Hz.
  struct timespec last; clock_gettime(CLOCK_MONOTONIC, &last);
  long long acc = 0;
  for (;;) {
    int events; struct android_poll_source *src;
    for (;;) {
      int r = ALooper_pollOnce(s_ready ? 0 : -1, NULL, &events, (void **)&src);
      if (r == ALOOPER_POLL_TIMEOUT || r == ALOOPER_POLL_WAKE || r < 0) break;
      if (src) src->process(app, src);
      if (app->destroyRequested) return;
    }
    if (!s_ready) continue;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    acc += (now.tv_sec - last.tv_sec) * 1000000000LL + (now.tv_nsec - last.tv_nsec);
    last = now;
    if (acc > 4 * FRAME_NS) acc = 4 * FRAME_NS;          // never spiral after a stall
    if (g_menu.quit) return;
    while (acc >= FRAME_NS) {
      acc -= FRAME_NS;
      touch_flush();
      if (s_turn_vel && s_pad_pointer < 0) {              // inertia: the spin after a swipe
        game_turn_by(s_turn_vel);
        s_turn_vel = (s_turn_vel * 15) / 16;
        if (s_turn_vel > -8 && s_turn_vel < 8) s_turn_vel = 0;
      }
      if (s_wheel_pointer < 0 && s_wheel) {                // let go: the wheel returns to centre
        s_wheel = s_wheel > 0 ? (s_wheel > 24 ? s_wheel - 24 : 0) : (s_wheel < -24 ? s_wheel + 24 : 0);
      }
      g_p.wheel = (int16_t)(driving() ? s_wheel : 0);
      game_step();
    }
    render_frame((GContext *)1, GRect(0, 0, SW, SH));
    blit();                                              // blocks on vsync: the display's rate
  }
}
