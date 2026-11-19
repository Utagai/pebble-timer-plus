//! @file timer.h
//! @brief Data and controls for timer
//!
//! Contains data and all functions for setting and accessing
//! a timer. Also saves and loads timers between closing and reopening.
//!
//! @author Eric D. Phillips
//! @data October 26, 2015
//! @bugs No known bugs

#pragma once
#include <pebble.h>

//! Maximum number of simultaneous timers
#define MAX_TIMERS 8

//! Get the number of timers currently stored (always at least one)
//! @return The number of timers
uint8_t timer_count(void);

//! Get the index of the currently active timer
//! @return The index of the active timer
uint8_t timer_get_active(void);

//! Set which timer is active
//! @param index The index of the timer to make active
void timer_set_active(uint8_t index);

//! Add a new empty timer and make it active
//! @return The index of the newly added timer
uint8_t timer_add(void);

//! Delete a timer, always leaving at least one timer
//! @param index The index of the timer to delete
void timer_delete(uint8_t index);

//! Get the active timer's value divided into time parts
//! @param hr A pointer to where to store the hour value of the timer
//! @param min A pointer to where to store the minute value of the timer
//! @param sec A pointer to where to store the second value of the timer
void timer_get_time_parts(uint16_t *hr, uint16_t *min, uint16_t *sec);

//! Get a specific timer's value divided into time parts
//! @param index The index of the timer to read
//! @param hr A pointer to where to store the hour value of the timer
//! @param min A pointer to where to store the minute value of the timer
//! @param sec A pointer to where to store the second value of the timer
void timer_get_time_parts_at(uint8_t index, uint16_t *hr, uint16_t *min, uint16_t *sec);

//! Get the active timer time in milliseconds
//! @return The current value of the active timer in milliseconds
int64_t timer_get_value_ms(void);

//! Get a specific timer's time in milliseconds
//! @param index The index of the timer to read
//! @return The current value of the timer in milliseconds
int64_t timer_get_value_ms_at(uint8_t index);

//! Get the total active timer time in milliseconds
//! @return The total value of the active timer in milliseconds
int64_t timer_get_length_ms(void);

//! Get a specific timer's total time in milliseconds
//! @param index The index of the timer to read
//! @return The total value of the timer in milliseconds
int64_t timer_get_length_ms_at(uint8_t index);

//! Check if the active timer is vibrating
//! @return True if the active timer is currently vibrating
bool timer_is_vibrating(void);

//! Check if a specific timer is vibrating
//! @param index The index of the timer to read
//! @return True if the timer is currently vibrating
bool timer_is_vibrating_at(uint8_t index);

//! Check if the active timer is in stopwatch mode
//! @return True if the active timer is counting up as a stopwatch
bool timer_is_chrono(void);

//! Check if a specific timer is in stopwatch mode
//! @param index The index of the timer to read
//! @return True if the timer is counting up as a stopwatch
bool timer_is_chrono_at(uint8_t index);

//! Check if the active timer is paused
//! @return True if the active timer is paused
bool timer_is_paused(void);

//! Check if a specific timer is paused
//! @param index The index of the timer to read
//! @return True if the timer is paused
bool timer_is_paused_at(uint8_t index);

//! Check if any timer is currently running (not paused)
//! @return True if at least one timer is running
bool timer_any_running(void);

//! Check if any timer is currently vibrating
//! @return True if at least one timer is vibrating
bool timer_any_vibrating(void);

//! Acknowledge any vibrating timers by rewinding them and cancelling vibration
//! @return True if the active timer was among those rewound
bool timer_acknowledge(void);

//! Check if any timer is elapsed and vibrate if this is the first call after elapsing
//! @return True if the active timer was an unacknowledged countdown rewound to its original length
bool timer_check_elapsed(void);

//! Increment the active timer value currently being edited
//! @param increment The amount to increment by
void timer_increment(int64_t increment);

//! Toggle play pause state for the active timer
void timer_toggle_play_pause(void);

//! Rewind the active timer back to its original value
void timer_rewind(void);

//! Reset the active timer to zero
void timer_reset(void);

//! Reset a specific timer to zero
//! @param index The index of the timer to reset
void timer_reset_at(uint8_t index);

//! Save the timers to persistent storage
void timer_persist_store(void);

//! Read the timers from persistent storage
void timer_persist_read(void);

//! Schedule wakeups for all running countdown timers, respecting the SDK's one minute spacing rule
void timer_schedule_wakeups(void);
