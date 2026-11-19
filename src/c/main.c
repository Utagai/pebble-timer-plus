// @file main.c
// @brief Main logic for Timer++
//
// Contains the higher level logic code
//
// @author Eric D. Phillips
// @date August 27, 2015
// @bugs No known bugs

#include "main.h"
#include "drawing.h"
#include "timer.h"
#include "timer_list.h"
#include "utility.h"
#include <pebble.h>

// Main constants
#define BUTTON_HOLD_REPEAT_MS 100
#define SYSTEM_ENTRANCE_ANIMATION_MS 400

// Main data structure
static struct {
  Window *window;           //< The base window for the application
  Layer *layer;             //< The base layer on which everything will be drawn
  ControlMode control_mode; //< The current control mode of the timer
  AppTimer *app_timer;      //< The AppTimer to keep the screen refreshing
} main_data;

// Function declarations
static void prv_app_timer_callback(void *data);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Private Functions
//

// Rewind timer if button is clicked to stop vibration
static bool main_timer_rewind(void) {
  // check if any timer is vibrating
  if (!timer_any_vibrating()) {
    return false;
  }
  // rewind any vibrating timers and cancel the vibration
  bool active_rewound = timer_acknowledge();
  if (active_rewound) {
    main_data.control_mode = ControlModeEditSec;
  }
  drawing_update();
  return true;
}

// Set the control mode to match the active timer's current state
static void prv_sync_control_mode(void) {
  if (timer_is_paused()) {
    // get time parts
    uint16_t hr, min, sec;
    timer_get_time_parts(&hr, &min, &sec);
    main_data.control_mode = hr ? ControlModeEditHr : ControlModeEditMin;
  } else {
    main_data.control_mode = ControlModeCounting;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
//

// Get the current control mode of the timer
ControlMode main_get_control_mode(void) { return main_data.control_mode; }

// Background layer update procedure
static void prv_layer_update_proc_handler(Layer *layer, GContext *ctx) {
  // render the timer's visuals
  drawing_render(layer, ctx);
}

// Back click handler
static void prv_back_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // cancel vibrations
  main_timer_rewind();
  // get time parts
  uint16_t hr, min, sec;
  timer_get_time_parts(&hr, &min, &sec);
  // change control mode
  if ((hr && main_data.control_mode == ControlModeEditMin) ||
      main_data.control_mode == ControlModeEditSec) {
    main_data.control_mode--;
  } else {
    window_stack_pop(true);
  }
  // refresh
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Up click handler
static void prv_up_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // rewind timer if clicked while timer is going off
  if (main_timer_rewind() || main_data.control_mode == ControlModeCounting) {
    return;
  }
  // increment timer
  int64_t increment;
  if (main_data.control_mode == ControlModeEditHr) {
    increment = MSEC_IN_HR;
  } else if (main_data.control_mode == ControlModeEditMin) {
    increment = MSEC_IN_MIN;
  } else {
    increment = MSEC_IN_SEC;
  }
  // get starting time components
  uint16_t o_hr, o_min, o_sec;
  timer_get_time_parts(&o_hr, &o_min, &o_sec);
  // increment timer
  timer_increment(increment);
  // compare final time parts and switch into edit hr mode
  uint16_t n_hr, n_min, n_sec;
  timer_get_time_parts(&n_hr, &n_min, &n_sec);
  if (o_min > n_min && !o_hr) {
    timer_increment(MSEC_IN_HR);
    main_data.control_mode = ControlModeEditHr;
  }
  // check if switched out of ControlModeEditHr
  if (timer_get_value_ms() / MSEC_IN_HR == 0 && main_data.control_mode == ControlModeEditHr) {
    main_data.control_mode = ControlModeEditMin;
  }
  // animate and refresh
  if (!click_recognizer_is_repeating(recognizer)) {
    drawing_start_bounce_animation(true);
  }
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Select click handler
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // rewind timer if clicked while timer is going off
  if (main_timer_rewind()) {
    return;
  }
  // change timer mode
  switch (main_data.control_mode) {
  case ControlModeEditHr:
    main_data.control_mode = ControlModeEditMin;
    break;
  case ControlModeEditMin:
    main_data.control_mode = ControlModeEditSec;
    break;
  case ControlModeEditSec:
    main_data.control_mode = ControlModeCounting;
    timer_toggle_play_pause();
    if (!main_data.app_timer) {
      prv_app_timer_callback(NULL);
    }
    break;
  case ControlModeCounting:
    main_data.control_mode = ControlModeEditSec;
    timer_toggle_play_pause();
    break;
  }
  // refresh
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Select raw click handler
static void prv_select_raw_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // stop vibration
  vibes_cancel();
  // animate and refresh
  drawing_start_reset_animation();
  layer_mark_dirty(main_data.layer);
}

// Select long click handler
static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // open the timer list to manage the other timers
  timer_list_push();
}

// Select multi click handler
static void prv_select_multi_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  main_data.control_mode = ControlModeEditMin;
  timer_reset();
  // animate and refresh
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Down click handler
static void prv_down_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // rewind timer if clicked while timer is going off
  if (main_timer_rewind() || main_data.control_mode == ControlModeCounting) {
    return;
  }
  // increment timer
  int64_t increment;
  if (main_data.control_mode == ControlModeEditHr) {
    increment = -MSEC_IN_HR;
  } else if (main_data.control_mode == ControlModeEditMin) {
    increment = -MSEC_IN_MIN;
  } else {
    increment = -MSEC_IN_SEC;
  }
  timer_increment(increment);
  // check if switched out of ControlModeEditHr
  if (timer_get_value_ms() / MSEC_IN_HR == 0 && main_data.control_mode == ControlModeEditHr) {
    main_data.control_mode = ControlModeEditMin;
  }
  // animate and refresh
  if (!click_recognizer_is_repeating(recognizer)) {
    drawing_start_bounce_animation(false);
  }
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Click configuration provider
static void prv_click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_HOLD_REPEAT_MS,
                                          prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_raw_click_subscribe(BUTTON_ID_SELECT, prv_select_raw_click_handler, NULL, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, BUTTON_HOLD_RESET_MS, prv_select_long_click_handler,
                              NULL);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 2, 0, true, prv_select_multi_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_HOLD_REPEAT_MS,
                                          prv_down_click_handler);
}

// AppTimer callback
static void prv_app_timer_callback(void *data) {
  // check if timer is complete, and leave counting mode if it rewound itself
  if (timer_check_elapsed() && main_data.control_mode == ControlModeCounting) {
    main_data.control_mode = ControlModeEditSec;
  }
  // refresh
  drawing_update();
  layer_mark_dirty(main_data.layer);
  timer_list_refresh();
  // schedule next call while anything is still running
  main_data.app_timer = NULL;
  if (timer_any_running()) {
    uint32_t duration = MSEC_IN_SEC;
    if (main_data.control_mode == ControlModeCounting) {
      duration = timer_get_value_ms() % MSEC_IN_SEC;
      if (timer_is_chrono()) {
        duration = MSEC_IN_SEC - duration;
      }
    }
    main_data.app_timer = app_timer_register(duration + 5, prv_app_timer_callback, NULL);
  }
}

// TickTimerService callback
static void prv_tick_timer_service_callback(struct tm *tick_time, TimeUnits units_changed) {
  // refresh
  layer_mark_dirty(main_data.layer);
}

// Window appear handler, fires when returning from the timer list
static void prv_window_appear_handler(Window *window) {
  // sync the control mode with whichever timer is now active
  prv_sync_control_mode();
  if (timer_any_running() && !main_data.app_timer) {
    prv_app_timer_callback(NULL);
  } else {
    drawing_update();
    layer_mark_dirty(main_data.layer);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loading and Unloading
//

// Initialize the program
static void prv_initialize(void) {
  // cancel any existing wakeup events
  wakeup_cancel_all();
  // load timer
  timer_persist_read();
  // when launched by a wakeup, show the timer that fired
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    WakeupId wakeup_id;
    int32_t cookie;
    if (wakeup_get_launch_event(&wakeup_id, &cookie) && cookie >= 0 &&
        cookie < timer_count()) {
      timer_set_active((uint8_t)cookie);
    }
  }
  // set initial states
  prv_sync_control_mode();

  // initialize window
  main_data.window = window_create();
  ASSERT(main_data.window);
  window_set_click_config_provider(main_data.window, prv_click_config_provider);
  Layer *window_root = window_get_root_layer(main_data.window);
  GRect window_bounds = layer_get_bounds(window_root);
#ifdef PBL_SDK_2
  window_set_fullscreen(main_data.window, true);
  window_bounds.size.h = 168;
#endif
  window_stack_push(main_data.window, true);
  // initialize main layer
  main_data.layer = layer_create(window_bounds);
  ASSERT(main_data.layer);
  layer_set_update_proc(main_data.layer, prv_layer_update_proc_handler);
  layer_add_child(window_root, main_data.layer);

  // initialize drawing singleton
  drawing_initialize(main_data.layer);
  // handle the window appearing again (e.g. returning from the timer list)
  window_set_window_handlers(main_data.window, (WindowHandlers){
                                                 .appear = prv_window_appear_handler,
                                             });
  // subscribe to tick timer service
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_timer_service_callback);
  // start refreshing
  AppLaunchReason reason = launch_reason();
  if (reason == APP_LAUNCH_QUICK_LAUNCH || reason == APP_LAUNCH_WAKEUP) {
    // launch timer immediately for wakeups and quick launches
    prv_app_timer_callback(NULL);
  } else {
    // the system opening animation freezes the app on the first frame, wait until that is done
    // before animating elements in (ideally there would be a better way to detect/get when the
    // system animation finished)
    app_timer_register(SYSTEM_ENTRANCE_ANIMATION_MS, prv_app_timer_callback, NULL);
  }
}

// Terminate the program
static void prv_terminate(void) {
  // unsubscribe from timer service
  tick_timer_service_unsubscribe();
  // schedule wakeups for every running countdown timer
  timer_schedule_wakeups();
  // destroy
  timer_persist_store();
  timer_list_terminate();
  drawing_terminate();
  layer_destroy(main_data.layer);
  window_destroy(main_data.window);
}

// Entry point
int main(void) {
  prv_initialize();
  app_event_loop();
  prv_terminate();
}
