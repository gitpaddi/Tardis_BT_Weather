#include <pebble.h>

static BitmapLayer *s_background_layer, *s_bt_icon_layer;
static GBitmap *s_background_bitmap, *s_bt_icon_bitmap;

static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;
TextLayer *time_layer;
TextLayer *date_layer;

static Window *s_main_window;
static TextLayer *s_temperature_layer;
static TextLayer *s_city_layer;
static BitmapLayer *s_icon_layer;
static GBitmap *s_icon_bitmap = NULL;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
  WEATHER_CITY_KEY = 0x2,         // TUPLE_CSTRING
};

static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_IMAGE_SUN, // 0
  RESOURCE_ID_IMAGE_CLOUD, // 1
  RESOURCE_ID_IMAGE_RAIN, // 2
  RESOURCE_ID_IMAGE_SNOW // 3
};

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case WEATHER_ICON_KEY:
      if (s_icon_bitmap) {
        gbitmap_destroy(s_icon_bitmap);
      }
      s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[new_tuple->value->uint8]);
      bitmap_layer_set_compositing_mode(s_icon_layer, GCompOpSet);
      bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
      break;

    case WEATHER_TEMPERATURE_KEY:
      // App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
      text_layer_set_text(s_temperature_layer, new_tuple->value->cstring);
      break;

    case WEATHER_CITY_KEY:
      text_layer_set_text(s_city_layer, new_tuple->value->cstring);
      break;
  }
}

static void request_weather(void) {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    if (!iter) {
        // Error creating outbound message
        return;
    }    
    int value = 1;
    dict_write_int(iter, 1, &value, sizeof(int), true);
    dict_write_end(iter);    
    app_message_outbox_send();
}

static void bluetooth_callback(bool connected) {
layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), connected);
    if(!connected) {
        vibes_double_pulse();
    }
}

void handle_timechanges(struct tm *tick_time, TimeUnits units_changed){
    static char timebuffer[10];
    static char datebuffer[10];
    strftime(timebuffer, sizeof(timebuffer), "%H\n%M", tick_time);
    text_layer_set_text(time_layer, timebuffer);
    strftime(datebuffer, sizeof(datebuffer), "%b\n%e", tick_time);
    text_layer_set_text(date_layer, datebuffer);
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TARDIS_BG);
    s_background_layer = bitmap_layer_create(bounds);
    bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
    
    time_layer = text_layer_create(GRect(25, 29, 40, 48));
    text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
    text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
    layer_add_child(window_get_root_layer(s_main_window), text_layer_get_layer(time_layer));
    
    date_layer = text_layer_create(GRect(83, 29, 40, 48));
    text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
    text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
    layer_add_child(window_get_root_layer(s_main_window), text_layer_get_layer(date_layer));
    
    // Create the Bluetooth icon GBitmap
    s_bt_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_ICON);
    s_bt_icon_layer = bitmap_layer_create(GRect(30, 92, 30, 30));
    bitmap_layer_set_bitmap(s_bt_icon_layer, s_bt_icon_bitmap);
    layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_bt_icon_layer));
    bluetooth_callback(connection_service_peek_pebble_app_connection());

    s_icon_layer = bitmap_layer_create(GRect(87, 92, 30, 30));
    layer_add_child(window_layer, bitmap_layer_get_layer(s_icon_layer));
    
    s_temperature_layer = text_layer_create(GRect(86, 128, 35, 20));
    text_layer_set_text_color(s_temperature_layer, GColorWhite);
    text_layer_set_background_color(s_temperature_layer, GColorBlue);
    text_layer_set_font(s_temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(s_temperature_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_temperature_layer));
    
    s_city_layer = text_layer_create(GRect(82, 148, 40, 20));
    text_layer_set_text_color(s_city_layer, GColorWhite);
    text_layer_set_background_color(s_city_layer, GColorClear);
    text_layer_set_font(s_city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_alignment(s_city_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_city_layer));

    Tuplet initial_values[] = {
        TupletInteger(WEATHER_ICON_KEY, (uint8_t) 2),
        TupletCString(WEATHER_TEMPERATURE_KEY, "load\u00B0C"),
        TupletCString(WEATHER_CITY_KEY, "Absam"),
    };

    app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
        initial_values, ARRAY_LENGTH(initial_values),
        sync_tuple_changed_callback, sync_error_callback, NULL);
    request_weather();
    
    time_t now = time(NULL);
    handle_timechanges(localtime(&now), MINUTE_UNIT);
    tick_timer_service_subscribe(MINUTE_UNIT, handle_timechanges);
}

static void main_window_unload(Window *window) {
    if (s_icon_bitmap) {
        gbitmap_destroy(s_icon_bitmap);
    }
    text_layer_destroy(s_city_layer);
    text_layer_destroy(s_temperature_layer);
    bitmap_layer_destroy(s_icon_layer);
    
    gbitmap_destroy(s_background_bitmap);
    bitmap_layer_destroy(s_background_layer);
    gbitmap_destroy(s_bt_icon_bitmap);
    bitmap_layer_destroy(s_bt_icon_layer);
}

static void init() {
    connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = bluetooth_callback
    });
    s_main_window = window_create();
    window_set_background_color(s_main_window, PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack));    
    window_set_window_handlers(s_main_window, (WindowHandlers) {
      .load = main_window_load,
      .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);
    app_message_open(64, 64);
}

static void deinit() {
    window_destroy(s_main_window);
    text_layer_destroy(time_layer);
    text_layer_destroy(date_layer);
    app_sync_deinit(&s_sync);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}

