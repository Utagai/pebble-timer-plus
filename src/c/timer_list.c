// @file timer_list.c
// @brief Timer list window
//
// A simple list of all the stored timers. Used to switch between, add,
// reset and delete timers without using any extra physical buttons.
//
// @author Eric D. Phillips
// @bugs No known bugs

#include "timer_list.h"
#include "timer.h"
#include "utility.h"
#include <pebble-scalable/pebble-scalable.h>
#include <pebble.h>

// Layout (values are thousandths of the screen, see pebble-scalable)
#define LIST_ROW_HEIGHT scl_y(179) // ~30px on the original 168px tall screen
#ifdef PBL_ROUND
// Round displays need extra padding so rows stay inside the circular bezel
#define LIST_TOP_PADDING scl_y(150)
#define LIST_SIDE_PADDING scl_x(150)
#else
#define LIST_TOP_PADDING scl_y(95)  // ~16px
#define LIST_SIDE_PADDING scl_x(83) // ~12px
#endif
#define LIST_INDEX_WIDTH scl_x(139) // ~20px
#define LIST_DOT_RADIUS scl_x(35)   // ~5px
#define LIST_DOT_GAP scl_x(28)      // ~4px
#define LIST_HIGHLIGHT_RADIUS 4

// Main data
static struct {
  Window *window;   //< The list window
  Layer *layer;     //< The list layer
  uint8_t selected; //< The highlighted row
  uint8_t scroll;   //< The first visible row
} list_data;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Private Functions
//

// Format a timer's value into a display string
static void prv_format_time(uint8_t index, char *buff, size_t size) {
  uint16_t hr, min, sec;
  timer_get_time_parts_at(index, &hr, &min, &sec);
  if (hr) {
    snprintf(buff, size, "%u:%02u:%02u", hr, min, sec);
  } else {
    snprintf(buff, size, "%u:%02u", min, sec);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
//

// Draw the list
static void prv_layer_update_proc(Layer *layer, GContext *ctx) {
  // get properties
  GRect bounds = layer_get_bounds(layer);
  // draw background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  // calculate how many rows fit
  uint8_t count = timer_count();
  int16_t bottom_padding = 0;
#ifdef PBL_ROUND
  bottom_padding = LIST_TOP_PADDING;
#endif
  uint8_t rows_visible =
      (bounds.size.h - LIST_TOP_PADDING - bottom_padding) / LIST_ROW_HEIGHT;
  if (rows_visible == 0) {
    rows_visible = 1;
  }
  // keep the selection on screen
  if (list_data.selected < list_data.scroll) {
    list_data.scroll = list_data.selected;
  } else if (list_data.selected >= list_data.scroll + rows_visible) {
    list_data.scroll = list_data.selected - rows_visible + 1;
  }
  // draw each visible row
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  for (uint8_t row = 0; row < rows_visible && list_data.scroll + row < count; row++) {
    uint8_t index = list_data.scroll + row;
    GRect row_bounds =
        GRect(0, LIST_TOP_PADDING + row * LIST_ROW_HEIGHT, bounds.size.w, LIST_ROW_HEIGHT);
    bool is_selected = (index == list_data.selected);
    bool is_running = !timer_is_paused_at(index);
    // highlight the selected row
    if (is_selected) {
      GRect highlight = grect_inset(
          row_bounds, GEdgeInsets(2, LIST_SIDE_PADDING / 2, 2, LIST_SIDE_PADDING / 2));
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
      graphics_fill_rect(ctx, highlight, LIST_HIGHLIGHT_RADIUS, GCornersAll);
    }
    // pick text color
    GColor text_color = is_selected ? GColorBlack : GColorWhite;
    graphics_context_set_text_color(ctx, text_color);
    // draw the index
    char index_buff[4];
    snprintf(index_buff, sizeof(index_buff), "%u", index + 1);
    GRect index_bounds =
        GRect(LIST_SIDE_PADDING, row_bounds.origin.y, LIST_INDEX_WIDTH, row_bounds.size.h);
    graphics_draw_text(ctx, index_buff, font, index_bounds, GTextOverflowModeFill,
                       GTextAlignmentLeft, NULL);
    // draw the time
    char time_buff[24];
    prv_format_time(index, time_buff, sizeof(time_buff));
    int16_t time_left = LIST_SIDE_PADDING + LIST_INDEX_WIDTH;
    int16_t time_right = bounds.size.w - LIST_SIDE_PADDING - LIST_DOT_RADIUS * 2 - LIST_DOT_GAP;
    GRect time_bounds =
        GRect(time_left, row_bounds.origin.y, time_right - time_left, row_bounds.size.h);
    graphics_draw_text(ctx, time_buff, font, time_bounds, GTextOverflowModeFill,
                       GTextAlignmentRight, NULL);
    // draw a running / paused indicator
    GPoint dot = GPoint(bounds.size.w - LIST_SIDE_PADDING - LIST_DOT_RADIUS,
                        row_bounds.origin.y + row_bounds.size.h / 2);
    graphics_context_set_stroke_color(ctx, text_color);
    graphics_context_set_fill_color(ctx, text_color);
    if (is_running) {
      graphics_fill_circle(ctx, dot, LIST_DOT_RADIUS);
    } else {
      graphics_draw_circle(ctx, dot, LIST_DOT_RADIUS);
    }
  }
}

// Open the highlighted timer in the main window
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  timer_set_active(list_data.selected);
  window_stack_pop(true);
}

// Move the selection up
static void prv_up_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  if (list_data.selected > 0) {
    list_data.selected--;
    layer_mark_dirty(list_data.layer);
  }
}

// Move the selection down
static void prv_down_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  if (list_data.selected + 1 < timer_count()) {
    list_data.selected++;
    layer_mark_dirty(list_data.layer);
  }
}

// Add a new timer
static void prv_action_new(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  timer_add();
  list_data.selected = timer_get_active();
}

// Reset the highlighted timer
static void prv_action_reset(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  timer_reset_at(list_data.selected);
}

// Delete the highlighted timer
static void prv_action_delete(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  timer_delete(list_data.selected);
  if (list_data.selected >= timer_count()) {
    list_data.selected = timer_count() - 1;
  }
}

// Clean up the action menu
static void prv_action_did_close(ActionMenu *menu, const ActionMenuItem *performed, void *context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(menu), NULL, NULL);
  if (list_data.layer) {
    layer_mark_dirty(list_data.layer);
  }
}

// Open the context menu for the highlighted timer
static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  ActionMenuLevel *root = action_menu_level_create(3);
  action_menu_level_add_action(root, "New", prv_action_new, NULL);
  action_menu_level_add_action(root, "Reset", prv_action_reset, NULL);
  action_menu_level_add_action(root, "Delete", prv_action_delete, NULL);
  ActionMenuConfig config = {
      .root_level = root,
      .align = ActionMenuAlignCenter,
      .did_close = prv_action_did_close,
  };
  action_menu_open(&config);
}

// Click configuration provider
static void prv_click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long_click_handler, NULL);
}

// Window load handler
static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  list_data.layer = layer_create(bounds);
  ASSERT(list_data.layer);
  layer_set_update_proc(list_data.layer, prv_layer_update_proc);
  layer_add_child(root, list_data.layer);
}

// Window unload handler
static void prv_window_unload(Window *window) {
  layer_destroy(list_data.layer);
  list_data.layer = NULL;
}

// Window appear handler
static void prv_window_appear(Window *window) {
  // start on the active timer
  list_data.selected = timer_get_active();
  layer_mark_dirty(list_data.layer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// API Implementation
//

// Push the timer list window onto the window stack
void timer_list_push(void) {
  if (!list_data.window) {
    list_data.window = window_create();
    ASSERT(list_data.window);
    window_set_click_config_provider(list_data.window, prv_click_config_provider);
    window_set_window_handlers(list_data.window, (WindowHandlers){
                                                   .load = prv_window_load,
                                                   .unload = prv_window_unload,
                                                   .appear = prv_window_appear,
                                               });
  }
  window_stack_push(list_data.window, true);
}

// Refresh the list if it is currently visible
void timer_list_refresh(void) {
  if (list_data.layer) {
    layer_mark_dirty(list_data.layer);
  }
}

// Destroy the timer list window
void timer_list_terminate(void) {
  if (list_data.window) {
    window_destroy(list_data.window);
    list_data.window = NULL;
  }
}
