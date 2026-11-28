#include "wr.h"
#define KEY_RESIDENTS 7   // persistent storage: the secret island's population

// ---------------------------------------------------------------------------
// Wrist Revoked
//
// Controls:
//   Touch          virtual analogue stick - forward, reverse, strafe.
//                  It floats: wherever your thumb lands becomes centre.
//   UP      held   turn left
//   DOWN    held   turn right
//   SELECT  click  attack, steal / exit a car, enter or leave the gun shop
//   BACK    click  pause menu (settings, camera, quit)
//
// BACK is deliberately unbound. A raw click handler on it does not reliably
// stop the window stack popping the app, so it is left to do its normal job.
// ---------------------------------------------------------------------------

static Window    *s_window;
static Layer     *s_canvas;
static AppTimer  *s_timer;

static bool s_hold_left;
static bool s_hold_right;

static void apply_turn(void) {
  int8_t dir = 0;
  if (s_hold_left && !s_hold_right)      dir = -1;
  else if (s_hold_right && !s_hold_left) dir = +1;
  game_input_turn(dir);
}

// --- buttons ---------------------------------------------------------------

// While the menu is open the same buttons drive it instead of the car.
static void left_down(ClickRecognizerRef r, void *c) {
  (void)r; (void)c;
  if (g_menu.open) { menu_nav(-1); return; }
  s_hold_left = true;  apply_turn();
}
static void left_up(ClickRecognizerRef r, void *c) {
  (void)r; (void)c; s_hold_left = false; apply_turn();
}
static void right_down(ClickRecognizerRef r, void *c) {
  (void)r; (void)c;
  if (g_menu.open) { menu_nav(+1); return; }
  s_hold_right = true; apply_turn();
}
static void right_up(ClickRecognizerRef r, void *c) {
  (void)r; (void)c; s_hold_right = false; apply_turn();
}

// SELECT is a raw subscription rather than a single click, so the game can
// tell press from release. A single-click handler only fires on release and
// carries no notion of holding - which is why the flamethrower could only
// ever be tapped.
#ifdef WR_KEYS
// Emulator build: no touchscreen, so SELECT does double duty. Held, it is
// the throttle (the stick pushed straight forward); tapped, it is the
// action. The decision is made on release: under six frames is a tap.
// Held-fire weapons therefore fire once per tap in this build. UP/DOWN
// still turn, BACK is still the menu. The QEMU window maps the arrow
// keys to the buttons: up/down = UP/DOWN, right = SELECT, left = BACK.
static uint8_t s_sel_frames;
static void action_down(ClickRecognizerRef r, void *c) {
  (void)r; (void)c;
  if (g_menu.open) { menu_select(); return; }
  s_sel_frames = 1;
}
static void action_up(ClickRecognizerRef r, void *c) {
  (void)r; (void)c;
  if (s_sel_frames && s_sel_frames < 6) game_input_action();
  s_sel_frames = 0;
  game_touch_up();
}
static void keys_tick(void) {
  if (!s_sel_frames) return;
  if (s_sel_frames < 250) s_sel_frames++;
  if (s_sel_frames == 6) game_touch_down(STICK_HOME_X, STICK_HOME_Y);
  if (s_sel_frames >= 6) game_touch_move(STICK_HOME_X, (int16_t)(STICK_HOME_Y - STICK_RANGE));
}
#else
#if WR_ROUND
// A round watch that delivers no touch (or has it switched off) plays the
// emulator's way: SELECT held is the throttle, tapped is the action. Decided
// when the window loads.
static bool s_keys;
static uint8_t s_sel_frames;
static void keys_tick(void) {
  if (!s_sel_frames) return;
  if (s_sel_frames < 250) s_sel_frames++;
  if (s_sel_frames == 6) game_touch_down(STICK_HOME_X, STICK_HOME_Y);
  if (s_sel_frames >= 6) game_touch_move(STICK_HOME_X, (int16_t)(STICK_HOME_Y - STICK_RANGE));
}
#endif
// Press and release only: the game decides on release whether it was a
// tap (the action) or a hold (the car door, or the trigger).
static void action_down(ClickRecognizerRef r, void *c) {
  (void)r; (void)c;
  if (g_menu.open) { menu_select(); return; }
#if WR_ROUND
  if (s_keys) { s_sel_frames = 1; return; }
#endif
  game_input_hold(true);
}

static void action_up(ClickRecognizerRef r, void *c) {
  (void)r; (void)c;
#if WR_ROUND
  if (s_keys) {
    if (s_sel_frames && s_sel_frames < 6) game_input_action();
    s_sel_frames = 0;
    game_touch_up();
    return;
  }
#endif
  game_input_hold(false);
}
#endif

static void back_click(ClickRecognizerRef r, void *c) {
  (void)r; (void)c;
  // Inside a slider, BACK backs out of editing before closing the menu.
  if (g_menu.open && g_menu.editing) { g_menu.editing = 0; return; }
  menu_toggle();
}

static void click_config(void *context) {
  (void)context;
  window_raw_click_subscribe(BUTTON_ID_UP,   left_down,  left_up,  NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, right_down, right_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, action_down, action_up, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

// --- touch -----------------------------------------------------------------

// Touch under load. A drag is dozens of position events a second; if the
// handler does more than record them the kernel's event queue backs up,
// touch goes dead for a second, and PebbleOS resets the watch rather than
// wait. So the handler only records the newest position; the frame tick
// applies it once. Touchdown and liftoff are one event each and carry
// meaning (a dropped liftoff leaves the stick stuck), so they go straight
// through - after flushing any held position, so liftoff sees the true
// final one.
static int16_t s_tx, s_ty; static bool s_tpending;
static void touch_flush(void) {
  if (!s_tpending) return;
  s_tpending = false;
  game_touch_move(s_tx, s_ty);
}
static void touch_handler(const TouchEvent *event, void *context) {
  (void)context;
  if (g_menu.open) return;         // the menu is buttons only
  switch (event->type) {
    case TouchEvent_Touchdown:      game_touch_down(event->x, event->y); break;
    case TouchEvent_PositionUpdate: s_tx = event->x; s_ty = event->y; s_tpending = true; break;
    case TouchEvent_Liftoff:        touch_flush(); game_touch_up();      break;
    default: break;
  }
}

// --- frame -----------------------------------------------------------------

static void canvas_update(Layer *layer, GContext *ctx) {
  render_frame(ctx, layer_get_bounds(layer));
}

static void tick(void *data) {
  (void)data;
  if (g_menu.quit) { window_stack_pop_all(true); return; }
#ifdef WR_KEYS
  keys_tick();
#elif WR_ROUND
  if (s_keys) keys_tick();
#endif
  touch_flush();                       // the newest finger position, once per frame
  game_step();
  layer_mark_dirty(s_canvas);
  s_timer = app_timer_register(FRAME_MS, tick, NULL);
}

// --- lifecycle -------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_canvas = layer_create(b);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  if (touch_service_is_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
#if WR_ROUND && !defined(WR_KEYS)
  else s_keys = true;
#endif
}

static void window_unload(Window *window) {
  (void)window;
  touch_service_unsubscribe();
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  wr_seed((uint32_t)time(NULL) ^ 0x5A17u);
  audio_init();
  g_menu.volume = 80;
  g_menu.cam_left = 1;              // over the left shoulder by default; Mirror flips it
  g_menu.sfx_on = 1;
  game_init();
  g_p.residents = (uint8_t)persist_read_int(KEY_RESIDENTS);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, false);

  s_timer = app_timer_register(FRAME_MS, tick, NULL);
}

static void deinit(void) {
  persist_write_int(KEY_RESIDENTS, g_p.residents);    // the island's population outlives the app
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  audio_shutdown();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
