// V60 pour-over timer.
//
// Fixed recipe (300 g total), with a short warning buzz 5 s before each pour:
//   0:00  pour to  60 g   (bloom)
//   0:45  pour to  90 g   (warn 0:40)
//   1:30  pour to 180 g   (warn 1:25)
//   2:15  pour to 240 g   (warn 2:10)
//   3:00  pour to 300 g   (warn 2:55)
//
// SELECT starts. Long-press SELECT resets. BACK exits.
// Each pour cue is a long double buzz; the warning is a short buzz.

#include <pebble.h>

typedef enum { STEP_POUR, STEP_WARN } StepKind;

typedef struct {
  int at_s;        // seconds after start
  StepKind kind;
  int target_g;    // cumulative grams on the scale after this pour
} Step;

static const Step STEPS[] = {
  {   0, STEP_POUR,  60 },
  {  40, STEP_WARN,  90 },
  {  45, STEP_POUR,  90 },
  {  85, STEP_WARN, 180 },
  {  90, STEP_POUR, 180 },
  { 130, STEP_WARN, 240 },
  { 135, STEP_POUR, 240 },
  { 175, STEP_WARN, 300 },
  { 180, STEP_POUR, 300 },
};
#define NUM_STEPS ((int)(sizeof(STEPS) / sizeof(STEPS[0])))

typedef enum { STATE_IDLE, STATE_RUNNING } State;

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_now_layer;
static TextLayer *s_next_layer;
static TextLayer *s_hint_layer;

static State s_state = STATE_IDLE;
static uint64_t s_start_ms;
static int s_elapsed_s;
static int s_next_step;       // index of the next step that hasn't fired yet
static AppTimer *s_timer;

static char s_time_buf[16];
static char s_now_buf[32];
static char s_next_buf[32];

static uint64_t prv_now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint64_t)s * 1000 + ms;
}

static void prv_fmt_mmss(char *buf, size_t len, int secs) {
  snprintf(buf, len, "%d:%02d", secs / 60, secs % 60);
}

static void prv_vibe_pour(void) {
  static const uint32_t segs[] = { 500, 200, 500 };
  vibes_enqueue_custom_pattern((VibePattern){ .durations = segs, .num_segments = 3 });
}

static void prv_vibe_final_pour(void) {
  static const uint32_t segs[] = { 500, 200, 500, 200, 500 };
  vibes_enqueue_custom_pattern((VibePattern){ .durations = segs, .num_segments = 5 });
}

static void prv_vibe_warn(void) {
  vibes_short_pulse();
}

static void prv_render(void) {
  if (s_state == STATE_IDLE) {
    text_layer_set_text(s_time_layer, "0:00");
    text_layer_set_text(s_now_layer, "V60  300g");
    text_layer_set_text(s_next_layer, "60/90/180/240/300");
    text_layer_set_text(s_hint_layer, "SELECT to start");
    return;
  }

  prv_fmt_mmss(s_time_buf, sizeof(s_time_buf), s_elapsed_s);
  text_layer_set_text(s_time_layer, s_time_buf);

  // "Now" = the most recent pour step that has fired.
  int last_pour = -1;
  for (int i = 0; i < s_next_step; i++) {
    if (STEPS[i].kind == STEP_POUR) last_pour = i;
  }
  if (last_pour >= 0) {
    snprintf(s_now_buf, sizeof(s_now_buf), "Pour to %dg", STEPS[last_pour].target_g);
  } else {
    snprintf(s_now_buf, sizeof(s_now_buf), "Ready");
  }
  text_layer_set_text(s_now_layer, s_now_buf);

  // "Next" = the next pour step, with a countdown.
  int next_pour = -1;
  for (int i = s_next_step; i < NUM_STEPS; i++) {
    if (STEPS[i].kind == STEP_POUR) { next_pour = i; break; }
  }
  if (next_pour >= 0) {
    char cd[16];
    prv_fmt_mmss(cd, sizeof(cd), STEPS[next_pour].at_s - s_elapsed_s);
    snprintf(s_next_buf, sizeof(s_next_buf), "%dg in %s", STEPS[next_pour].target_g, cd);
    text_layer_set_text(s_next_layer, s_next_buf);
    text_layer_set_text(s_hint_layer, "");
  } else {
    text_layer_set_text(s_next_layer, "Drawdown");
    text_layer_set_text(s_hint_layer, "hold SELECT to reset");
  }
}

static void prv_fire_due_steps(void) {
  while (s_next_step < NUM_STEPS && STEPS[s_next_step].at_s <= s_elapsed_s) {
    const Step *st = &STEPS[s_next_step];
    if (st->kind == STEP_WARN) {
      prv_vibe_warn();
    } else if (s_next_step == NUM_STEPS - 1) {
      prv_vibe_final_pour();
    } else {
      prv_vibe_pour();
    }
    light_enable_interaction();
    APP_LOG(APP_LOG_LEVEL_INFO, "t=%d step %d (%s %dg)", s_elapsed_s, s_next_step,
            st->kind == STEP_WARN ? "warn" : "pour", st->target_g);
    s_next_step++;
  }
}

static void prv_tick(void *ctx);

// Schedule the next tick on the next whole-second boundary since start, so
// cues don't drift even though app timers are only approximately 1000 ms.
static void prv_schedule_tick(void) {
  uint64_t elapsed_ms = prv_now_ms() - s_start_ms;
  uint32_t delay = 1000 - (uint32_t)(elapsed_ms % 1000);
  s_timer = app_timer_register(delay, prv_tick, NULL);
}

static void prv_tick(void *ctx) {
  s_timer = NULL;
  if (s_state != STATE_RUNNING) return;
  // Round to nearest second so a tick that lands at 44.998 s counts as 45.
  s_elapsed_s = (int)((prv_now_ms() - s_start_ms + 500) / 1000);
  prv_fire_due_steps();
  prv_render();
  prv_schedule_tick();
}

static void prv_start(void) {
  s_state = STATE_RUNNING;
  s_start_ms = prv_now_ms();
  s_elapsed_s = 0;
  s_next_step = 0;
  prv_fire_due_steps();
  prv_render();
  prv_schedule_tick();
}

static void prv_reset(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  s_state = STATE_IDLE;
  prv_render();
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_state == STATE_IDLE) prv_start();
}

static void prv_select_long(ClickRecognizerRef recognizer, void *context) {
  prv_reset();
  vibes_short_pulse();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, prv_select_long, NULL);
}

static TextLayer *prv_make_layer(Layer *parent, GRect frame, const char *font_key) {
  TextLayer *tl = text_layer_create(frame);
  text_layer_set_font(tl, fonts_get_system_font(font_key));
  text_layer_set_text_alignment(tl, GTextAlignmentCenter);
  text_layer_set_background_color(tl, GColorClear);
  layer_add_child(parent, text_layer_get_layer(tl));
  return tl;
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  // Lay out relative to screen height so it works on 168, 180 (round) and 228 px screens.
  int time_h = 50;
  int now_h = 30;
  int next_h = 24;
  int hint_h = 20;
  int total = time_h + now_h + next_h + hint_h;
  int y = (b.size.h - total) / 2 - 4;
  if (y < 0) y = 0;

  s_time_layer = prv_make_layer(root, GRect(0, y, b.size.w, time_h), FONT_KEY_LECO_42_NUMBERS);
  y += time_h + 4;
  s_now_layer = prv_make_layer(root, GRect(0, y, b.size.w, now_h), FONT_KEY_GOTHIC_28_BOLD);
  y += now_h;
  s_next_layer = prv_make_layer(root, GRect(0, y, b.size.w, next_h), FONT_KEY_GOTHIC_18);
  y += next_h;
  s_hint_layer = prv_make_layer(root, GRect(0, y, b.size.w, hint_h), FONT_KEY_GOTHIC_14);

  prv_render();
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_now_layer);
  text_layer_destroy(s_next_layer);
  text_layer_destroy(s_hint_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  if (s_timer) app_timer_cancel(s_timer);
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
