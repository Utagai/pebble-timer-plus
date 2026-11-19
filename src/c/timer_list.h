//! @file timer_list.h
//! @brief Timer list window
//!
//! A simple list of all the stored timers. Used to switch between, add,
//! reset and delete timers without using any extra physical buttons.
//!
//! @author Eric D. Phillips
//! @bugs No known bugs

#pragma once
#include <pebble.h>

//! Push the timer list window onto the window stack
void timer_list_push(void);

//! Refresh the list if it is currently visible
void timer_list_refresh(void);

//! Destroy the timer list window
void timer_list_terminate(void);
