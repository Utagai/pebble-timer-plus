// @file timer.c
// @brief Data and controls for timer
//
// Contains data and all functions for setting and accessing
// a timer. Also saves and loads timers between closing and reopening.
//
// @author Eric D. Phillips
// @data October 26, 2015
// @bugs No known bugs

#include "timer.h"
#include "utility.h"

#define PERSIST_VERSION 3
#define PERSIST_VERSION_KEY 4342896
#define PERSIST_TIMER_KEY 58734     // legacy single timer key
#define PERSIST_STORE_KEY 58735     // current multi-timer store key
#define VIBRATION_LENGTH_MS 20000
// legacy persistent storage
#define PERSIST_TIMER_KEY_V2 3456

// Vibration sequence
static const uint32_t vibe_sequence[] = {150, 200, 300};
static const VibePattern vibe_pattern = {
    .durations = vibe_sequence,
    .num_segments = ARRAY_LENGTH(vibe_sequence),
};

// Single timer state
typedef struct {
  int64_t length_ms; //< Length of timer in milliseconds
  int64_t start_ms;  //< The start epoch of the timer in milliseconds
  bool elapsed;      //< Used to start the vibration if first time as elapsed
  bool can_vibrate;  //< Flag used to tell when the timer has completed
} Timer;

// Collection of timers plus which one the UI is currently working with
typedef struct {
  uint8_t count;             //< Number of valid timers (always at least 1)
  uint8_t active;            //< Index of the active timer
  Timer timers[MAX_TIMERS];  //< The timers themselves
} TimerStore;
static TimerStore timer_store;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Private Functions
//

// Get the active timer
static Timer *prv_active(void) { return &timer_store.timers[timer_store.active]; }

// Get a timer by index
static Timer *prv_at(uint8_t index) { return &timer_store.timers[index]; }

// Get a timer's value in milliseconds assuming the following conditions
// 1. when the timer is running, start_ms represents the epoch when it was started
// 2. when it is paused, start_ms represents the negative of the time is has been running
static int64_t prv_value_ms(const Timer *timer) {
  int64_t value =
      timer->length_ms - epoch() + (((timer->start_ms + epoch() - 1) % epoch()) + 1);
  if (value < 0) {
    return -value;
  }
  return value;
}

// Check if a timer is in stopwatch mode
static bool prv_is_chrono(const Timer *timer) {
  // see prv_value_ms for explanation of equation
  return timer->length_ms - (int64_t)epoch() +
             ((timer->start_ms + (int64_t)epoch() - 1) % (int64_t)epoch() + 1) <=
         0;
}

// Check if a timer is paused
static bool prv_is_paused(const Timer *timer) { return timer->start_ms <= 0; }

// Check if a timer is vibrating
static bool prv_is_vibrating(const Timer *timer) {
  return prv_is_chrono(timer) && !prv_is_paused(timer) && timer->can_vibrate;
}

// Rewind a timer back to its original value
static void prv_rewind(Timer *timer) {
  timer->start_ms = 0;
  // enable vibration
  if (timer->length_ms) {
    timer->can_vibrate = true;
  }
}

// Reset a timer to zero
static void prv_reset(Timer *timer) {
  timer->length_ms = 0;
  timer->start_ms = 0;
  timer->elapsed = false;
  // disable vibration
  timer->can_vibrate = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// API Functions
//

// Get the number of timers
uint8_t timer_count(void) { return timer_store.count; }

// Get the index of the active timer
uint8_t timer_get_active(void) { return timer_store.active; }

// Set which timer is active
void timer_set_active(uint8_t index) {
  if (index < timer_store.count) {
    timer_store.active = index;
  }
}

// Add a new empty timer and make it active
uint8_t timer_add(void) {
  if (timer_store.count >= MAX_TIMERS) {
    return timer_store.active;
  }
  uint8_t index = timer_store.count++;
  prv_reset(prv_at(index));
  timer_store.active = index;
  return index;
}

// Delete a timer, always leaving at least one timer
void timer_delete(uint8_t index) {
  if (index >= timer_store.count) {
    return;
  }
  // stop a vibration that belonged to the timer being deleted
  bool was_vibrating = prv_is_vibrating(prv_at(index));
  // shift the following timers down
  for (uint8_t ii = index; ii + 1 < timer_store.count; ii++) {
    timer_store.timers[ii] = timer_store.timers[ii + 1];
  }
  timer_store.count--;
  // never leave the store empty
  if (timer_store.count == 0) {
    timer_store.count = 1;
    timer_store.active = 0;
    prv_reset(prv_at(0));
  } else if (index < timer_store.active) {
    timer_store.active--;
  } else if (timer_store.active >= timer_store.count) {
    timer_store.active = timer_store.count - 1;
  }
  // stop the vibration if no remaining timer is vibrating
  if (was_vibrating && !timer_any_vibrating()) {
    vibes_cancel();
  }
}

// Get timer value divided into time parts
void timer_get_time_parts_at(uint8_t index, uint16_t *hr, uint16_t *min, uint16_t *sec) {
  int64_t value = prv_value_ms(prv_at(index));
  (*hr) = value / MSEC_IN_HR;
  (*min) = value % MSEC_IN_HR / MSEC_IN_MIN;
  (*sec) = value % MSEC_IN_MIN / MSEC_IN_SEC;
}

// Get the active timer value divided into time parts
void timer_get_time_parts(uint16_t *hr, uint16_t *min, uint16_t *sec) {
  timer_get_time_parts_at(timer_store.active, hr, min, sec);
}

// Get the active timer time in milliseconds
int64_t timer_get_value_ms(void) { return prv_value_ms(prv_active()); }

// Get a specific timer's time in milliseconds
int64_t timer_get_value_ms_at(uint8_t index) { return prv_value_ms(prv_at(index)); }

// Get the total active timer time in milliseconds
int64_t timer_get_length_ms(void) { return prv_active()->length_ms; }

// Get a specific timer's total time in milliseconds
int64_t timer_get_length_ms_at(uint8_t index) { return prv_at(index)->length_ms; }

// Check if the active timer is vibrating
bool timer_is_vibrating(void) { return prv_is_vibrating(prv_active()); }

// Check if a specific timer is vibrating
bool timer_is_vibrating_at(uint8_t index) { return prv_is_vibrating(prv_at(index)); }

// Check if the active timer is in stopwatch mode
bool timer_is_chrono(void) { return prv_is_chrono(prv_active()); }

// Check if a specific timer is in stopwatch mode
bool timer_is_chrono_at(uint8_t index) { return prv_is_chrono(prv_at(index)); }

// Check if the active timer is paused
bool timer_is_paused(void) { return prv_is_paused(prv_active()); }

// Check if a specific timer is paused
bool timer_is_paused_at(uint8_t index) { return prv_is_paused(prv_at(index)); }

// Check if any timer is currently running
bool timer_any_running(void) {
  for (uint8_t ii = 0; ii < timer_store.count; ii++) {
    if (!prv_is_paused(prv_at(ii))) {
      return true;
    }
  }
  return false;
}

// Check if any timer is currently vibrating
bool timer_any_vibrating(void) {
  for (uint8_t ii = 0; ii < timer_store.count; ii++) {
    if (prv_is_vibrating(prv_at(ii))) {
      return true;
    }
  }
  return false;
}

// Acknowledge any vibrating timers by rewinding them and cancelling vibration
bool timer_acknowledge(void) {
  bool active_rewound = false;
  bool any = false;
  for (uint8_t ii = 0; ii < timer_store.count; ii++) {
    if (prv_is_vibrating(prv_at(ii))) {
      prv_rewind(prv_at(ii));
      any = true;
      if (ii == timer_store.active) {
        active_rewound = true;
      }
    }
  }
  if (any) {
    vibes_cancel();
  }
  return active_rewound;
}

// Check if the timer is elapsed and vibrate if this is the first call after elapsing
bool timer_check_elapsed(void) {
  bool active_rewound = false;
  bool vibrate = false;
  for (uint8_t ii = 0; ii < timer_store.count; ii++) {
    Timer *timer = prv_at(ii);
    // if a countdown has been counting overtime for the whole vibration window without being
    // acknowledged, rewind it to its original length so it does not count up indefinitely
    // (stopwatches have no length and are intentionally left running)
    if (timer->length_ms > 0 && !prv_is_paused(timer) && prv_is_chrono(timer) &&
        prv_value_ms(timer) > VIBRATION_LENGTH_MS) {
      prv_rewind(timer);
      if (ii == timer_store.active) {
        active_rewound = true;
      }
    } else if (prv_is_vibrating(timer)) {
      // vibrate once even if several timers are going off together
      vibrate = true;
    }
  }
  if (vibrate) {
    vibes_enqueue_custom_pattern(vibe_pattern);
  }
  return active_rewound;
}

// Increment timer value currently being edited
void timer_increment(int64_t increment) {
  Timer *timer = prv_active();
  // if in paused stopwatch mode, rewind to previous time
  if (prv_is_chrono(timer) && timer->start_ms) {
    prv_rewind(timer);
    return;
  }
  // identify increment class
  int64_t interval;
  if (llabs(increment) < MSEC_IN_MIN) {
    interval = MSEC_IN_MIN;
  } else if (llabs(increment) < MSEC_IN_HR) {
    interval = MSEC_IN_HR;
  } else {
    interval = MSEC_IN_HR * 100;
  }
  // calculate new time by incrementing with wrapping
  int64_t ls_bit = (timer->length_ms + timer->start_ms) % interval;
  int64_t step = (ls_bit + interval + increment) % interval - ls_bit;
  if (timer->start_ms) {
    timer->start_ms += step;
    if (timer->start_ms > 0) {
      timer->length_ms += timer->start_ms;
      timer->start_ms = 0;
    }
  } else {
    timer->length_ms += step;
  }
  // if at zero, remove any leftover milliseconds
  if (prv_value_ms(timer) < MSEC_IN_SEC) {
    prv_reset(timer);
  }
  // enable vibration
  if (timer->length_ms) {
    timer->can_vibrate = true;
  }
}

// Toggle play pause state for timer
void timer_toggle_play_pause(void) {
  Timer *timer = prv_active();
  if (timer->start_ms > 0) {
    timer->start_ms -= epoch();
  } else {
    timer->start_ms += epoch();
  }
}

//! Rewind the timer back to its original value
void timer_rewind(void) { prv_rewind(prv_active()); }

// Reset the timer to zero
void timer_reset(void) { prv_reset(prv_active()); }

// Reset a specific timer to zero
void timer_reset_at(uint8_t index) {
  if (index < timer_store.count) {
    prv_reset(prv_at(index));
  }
}

// Save the timer to persistent storage
void timer_persist_store(void) {
  // write out current persistent data version for potential future reference
  persist_write_int(PERSIST_VERSION_KEY, PERSIST_VERSION);
  persist_write_data(PERSIST_STORE_KEY, &timer_store, sizeof(timer_store));
}

// Schedule wakeups for all running countdown timers
void timer_schedule_wakeups(void) {
  wakeup_cancel_all();
  bool scheduled[MAX_TIMERS] = {false};
  time_t last = 0;
  for (uint8_t n = 0; n < timer_store.count; n++) {
    // find the earliest timer that has not been considered yet
    int best = -1;
    time_t best_when = 0;
    for (uint8_t ii = 0; ii < timer_store.count; ii++) {
      if (scheduled[ii]) {
        continue;
      }
      Timer *timer = prv_at(ii);
      if (prv_is_paused(timer) || prv_is_chrono(timer)) {
        continue;
      }
      time_t when = (time_t)((epoch() + prv_value_ms(timer)) / MSEC_IN_SEC);
      if (best < 0 || when < best_when) {
        best = ii;
        best_when = when;
      }
    }
    if (best < 0) {
      break;
    }
    scheduled[best] = true;
    // the SDK refuses wakeups within one minute of an already scheduled one
    if (last && best_when < last + 60) {
      continue;
    }
    if (wakeup_schedule(best_when, best, true)) {
      last = best_when;
    }
  }
}

// Read the timer from persistent storage
void timer_persist_read(void) {
  // read legacy version
  if (persist_exists(PERSIST_TIMER_KEY_V2)) {
    persist_delete(PERSIST_TIMER_KEY_V2);
    if (launch_reason() == APP_LAUNCH_WAKEUP) {
      timer_increment(5000);
      timer_toggle_play_pause();
    }
  }
  // read current version
  if (persist_exists(PERSIST_STORE_KEY)) {
    persist_read_data(PERSIST_STORE_KEY, &timer_store, sizeof(timer_store));
    // guard against corrupted or stale data
    if (timer_store.count == 0 || timer_store.count > MAX_TIMERS) {
      timer_store.count = 1;
    }
    if (timer_store.active >= timer_store.count) {
      timer_store.active = timer_store.count - 1;
    }
  } else if (persist_exists(PERSIST_TIMER_KEY)) {
    // migrate the legacy single timer into the store
    Timer legacy;
    persist_read_data(PERSIST_TIMER_KEY, &legacy, sizeof(legacy));
    timer_store.count = 1;
    timer_store.active = 0;
    timer_store.timers[0] = legacy;
    persist_delete(PERSIST_TIMER_KEY);
  } else {
    // fresh install, start with a single empty timer
    timer_store.count = 1;
    timer_store.active = 0;
    prv_reset(prv_at(0));
  }
}
