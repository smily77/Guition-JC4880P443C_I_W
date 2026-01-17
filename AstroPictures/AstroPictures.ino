#pragma GCC push_options
#pragma GCC optimize("O3")

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "lvgl.h"
#include "driver/i2c_master.h"
#include "pins_config.h"
#include "src/lcd/st7701_lcd.h"
#include "src/touch/gt911_touch.h"

// WiFi Credentials
#include <Credentials.h>

bsp_lcd_handles_t lcd_panels;

st7701_lcd lcd = st7701_lcd(LCD_RST);
gt911_touch touch = gt911_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
static lv_color_t *buf1;

static lv_obj_t *image_obj;
static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_obj_t *ip_label;
static lv_obj_t *status_label;

static lv_img_dsc_t apod_img_dsc;
static uint16_t *apod_buffer = nullptr;
static size_t apod_buffer_size = 0;

static int image_offset_x = 0;
static int image_offset_y = 0;

static String last_apod_date;
static unsigned long last_apod_check_ms = 0;
static unsigned long last_clock_update_ms = 0;
static unsigned long last_brightness_update_ms = 0;

static constexpr unsigned long kApodCheckIntervalMs = 60UL * 60UL * 1000UL;
static constexpr unsigned long kClockUpdateIntervalMs = 10UL * 1000UL;
static constexpr unsigned long kBrightnessIntervalMs = 2UL * 1000UL;

static const char *kApodEndpoint = "https://api.nasa.gov/planetary/apod";

static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io,
                                                     esp_lcd_dpi_panel_event_data_t *edata,
                                                     void *user_ctx) {
  lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
  assert(disp_drv != NULL);
  lv_disp_flush_ready(disp_drv);
  return false;
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  lcd.lcd_draw_bitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, &color_p->full);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  bool touched;
  uint16_t touchX, touchY;

  touched = touch.getTouch(&touchX, &touchY);

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

void set_status_text(const char *text) {
  if (status_label != nullptr) {
    lv_label_set_text(status_label, text);
    lv_timer_handler();
  }
}

bool connect_wifi() {
  Serial.println("[WiFi] Verbinde...");
  set_status_text("Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Verbunden!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("\n[WiFi] Fehlgeschlagen!");
  return false;
}

void setup_ota() {
  ArduinoOTA.setHostname("AstroPictures");

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Update startet...");
    set_status_text("OTA Update...");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update fertig!");
    set_status_text("OTA Done! Rebooting...");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Bereit!");
}

void init_backlight() {
#ifdef LCD_PWM
  ledcAttach(LCD_PWM, 5000, 8);
  ledcWrite(LCD_PWM, 200);
#endif
}

void update_backlight() {
#ifdef LCD_PWM
  ledcWrite(LCD_PWM, 200);
#endif
}

void init_time() {
  setenv("TZ", "Europe/Zurich", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void update_clock_labels() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  if (!localtime_r(&now, &timeinfo)) {
    return;
  }

  char time_str[6];
  snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  char date_str[11];
  snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1, timeinfo.tm_mday);

  lv_label_set_text(time_label, time_str);
  lv_label_set_text(date_label, date_str);
}

void setup_ui() {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

  status_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(status_label, lv_color_hex(0xBBBBBB), 0);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
  lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);

  apod_buffer_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
  apod_buffer = static_cast<uint16_t *>(heap_caps_malloc(apod_buffer_size, MALLOC_CAP_SPIRAM));
  if (!apod_buffer) {
    set_status_text("PSRAM alloc failed");
    return;
  }
  memset(apod_buffer, 0, apod_buffer_size);

  apod_img_dsc.header.always_zero = 0;
  apod_img_dsc.header.w = LCD_H_RES;
  apod_img_dsc.header.h = LCD_V_RES;
  apod_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  apod_img_dsc.data_size = apod_buffer_size;
  apod_img_dsc.data = reinterpret_cast<const uint8_t *>(apod_buffer);

  image_obj = lv_img_create(lv_scr_act());
  lv_img_set_src(image_obj, &apod_img_dsc);
  lv_obj_set_size(image_obj, LCD_H_RES, LCD_V_RES);
  lv_obj_align(image_obj, LV_ALIGN_CENTER, 0, 0);

  time_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);
  lv_obj_align(time_label, LV_ALIGN_BOTTOM_LEFT, 16, -16);

  date_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(date_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(date_label, &lv_font_montserrat_28, 0);
  lv_obj_align(date_label, LV_ALIGN_BOTTOM_RIGHT, -16, -16);

  ip_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(ip_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
  lv_obj_align(ip_label, LV_ALIGN_BOTTOM_MID, 0, -36);

  lv_obj_move_foreground(time_label);
  lv_obj_move_foreground(date_label);
  lv_obj_move_foreground(ip_label);
  lv_obj_move_foreground(status_label);
  lv_obj_move_to_background(image_obj);

  update_clock_labels();
  lv_label_set_text(ip_label, "IP: ---");
  set_status_text("Booting...");
}

void set_pixel(int x, int y, uint16_t color) {
  if (x < 0 || y < 0 || x >= LCD_H_RES || y >= LCD_V_RES) {
    return;
  }
  apod_buffer[y * LCD_H_RES + x] = color;
}

void draw_loading_screen(const char *message) {
  if (!apod_buffer || !image_obj) {
    set_status_text(message);
    return;
  }
  memset(apod_buffer, 0, apod_buffer_size);
  for (int i = 0; i < 200; ++i) {
    int x = random(0, LCD_H_RES);
    int y = random(0, LCD_V_RES);
    set_pixel(x, y, 0xFFFF);
  }
  apod_img_dsc.header.w = LCD_H_RES;
  apod_img_dsc.header.h = LCD_V_RES;
  lv_img_cache_invalidate_src(&apod_img_dsc);
  lv_img_set_src(image_obj, &apod_img_dsc);
  lv_obj_invalidate(image_obj);
  lv_refr_now(lv_disp_get_default());
  set_status_text(message);
}

bool tjpg_draw_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!apod_buffer) {
    return false;
  }
  int start_x = x + image_offset_x;
  int start_y = y + image_offset_y;

  for (int row = 0; row < h; ++row) {
    int dest_y = start_y + row;
    if (dest_y < 0 || dest_y >= LCD_V_RES) {
      continue;
    }
    int src_offset = 0;
    int dest_x = start_x;
    if (dest_x < 0) {
      src_offset = -dest_x;
      dest_x = 0;
    }
    int copy_width = w - src_offset;
    if (dest_x + copy_width > LCD_H_RES) {
      copy_width = LCD_H_RES - dest_x;
    }
    if (copy_width <= 0) {
      continue;
    }
    uint16_t *dest = apod_buffer + dest_y * LCD_H_RES + dest_x;
    uint16_t *src = bitmap + row * w + src_offset;
    memcpy(dest, src, copy_width * sizeof(uint16_t));
  }
  return true;
}

bool download_buffer(const String &url, uint8_t **out_buffer, size_t *out_len) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, url)) {
    return false;
  }

  int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    Serial.printf("[HTTP] GET failed: %d\n", http_code);
    http.end();
    return false;
  }

  int total_len = http.getSize();
  if (total_len <= 0 || total_len > 8 * 1024 * 1024) {
    Serial.println("[HTTP] Invalid content length");
    http.end();
    return false;
  }

  uint8_t *buffer = static_cast<uint8_t *>(heap_caps_malloc(total_len, MALLOC_CAP_SPIRAM));
  if (!buffer) {
    Serial.println("[HTTP] PSRAM allocation failed");
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t read_total = 0;
  while (http.connected() && read_total < static_cast<size_t>(total_len)) {
    size_t available = stream->available();
    if (available) {
      int read_len = stream->readBytes(buffer + read_total, available);
      if (read_len <= 0) {
        break;
      }
      read_total += read_len;
    }
    delay(1);
  }

  http.end();

  if (read_total != static_cast<size_t>(total_len)) {
    free(buffer);
    Serial.println("[HTTP] Download incomplete");
    return false;
  }

  *out_buffer = buffer;
  *out_len = read_total;
  return true;
}

bool fetch_apod_metadata(String *image_url, String *date_str, String *media_type_out) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(kApodEndpoint) + "?api_key=" + APODApiKey;

  if (!http.begin(client, url)) {
    return false;
  }

  int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    Serial.printf("[APOD] HTTP error %d\n", http_code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[APOD] JSON error: %s\n", err.c_str());
    return false;
  }

  const char *media_type = doc["media_type"] | "image";
  const char *url_image = doc["hdurl"] | doc["url"] | "";
  const char *thumb_url = doc["thumbnail_url"] | "";
  const char *date = doc["date"] | "";

  if (strcmp(media_type, "image") != 0 && strlen(thumb_url) > 0) {
    url_image = thumb_url;
  }

  if (strlen(url_image) == 0) {
    return false;
  }

  *image_url = String(url_image);
  *date_str = String(date);
  *media_type_out = String(media_type);
  return true;
}

bool decode_and_show_jpeg(uint8_t *jpg_buffer, size_t jpg_len) {
  uint16_t jpg_width = 0;
  uint16_t jpg_height = 0;

  TJpgDec.getJpgSize(&jpg_width, &jpg_height, jpg_buffer, jpg_len);

  uint8_t scale = 1;
  while ((jpg_width / scale > LCD_H_RES || jpg_height / scale > LCD_V_RES) && scale < 8) {
    scale *= 2;
  }

  image_offset_x = (LCD_H_RES - (jpg_width / scale)) / 2;
  image_offset_y = (LCD_V_RES - (jpg_height / scale)) / 2;

  memset(apod_buffer, 0, apod_buffer_size);

  TJpgDec.setJpgScale(scale);
  TJpgDec.setCallback(tjpg_draw_callback);
  if (!TJpgDec.drawJpg(0, 0, jpg_buffer, jpg_len)) {
    Serial.println("[APOD] JPEG decode failed");
    return false;
  }

  apod_img_dsc.header.w = LCD_H_RES;
  apod_img_dsc.header.h = LCD_V_RES;
  lv_img_cache_invalidate_src(&apod_img_dsc);
  lv_img_set_src(image_obj, nullptr);
  lv_img_set_src(image_obj, &apod_img_dsc);
  lv_obj_invalidate(image_obj);
  lv_refr_now(lv_disp_get_default());
  lv_timer_handler();
  return true;
}

bool update_apod_image() {
  if (!apod_buffer || !image_obj) {
    set_status_text("No image buffer");
    return false;
  }
  draw_loading_screen("Loading APOD...");

  String image_url;
  String date_str;
  String media_type;
  if (!fetch_apod_metadata(&image_url, &date_str, &media_type)) {
    set_status_text("Error: APOD metadata");
    return false;
  }

  if (media_type != "image") {
    set_status_text("APOD is a Video - No Image");
    return false;
  }

  if (date_str.length() > 0 && date_str == last_apod_date) {
    set_status_text("APOD up-to-date");
    return true;
  }

  draw_loading_screen("Downloading image...");

  uint8_t *jpg_buffer = nullptr;
  size_t jpg_len = 0;
  if (!download_buffer(image_url, &jpg_buffer, &jpg_len)) {
    set_status_text("Error: HTTP");
    return false;
  }

  bool ok = decode_and_show_jpeg(jpg_buffer, jpg_len);
  free(jpg_buffer);

  if (ok) {
    last_apod_date = date_str;
    set_status_text("APOD loaded");
  } else {
    set_status_text("DL Err: JPEG");
  }

  lv_obj_move_foreground(status_label);
  return ok;
}

void setup() {
  Serial.begin(115200);
  Serial.println("AstroPictures - Initialization");

  randomSeed(millis());

  i2c_master_bus_handle_t i2c_handle = NULL;
  i2c_master_bus_config_t i2c_bus_conf = {
    .i2c_port = I2C_NUM_1,
    .sda_io_num = (gpio_num_t)TP_I2C_SDA,
    .scl_io_num = (gpio_num_t)TP_I2C_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .trans_queue_depth = 0,
    .flags = {
      .enable_internal_pullup = 1,
    },
  };
  i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);

  lcd.begin();
  touch.begin();
  lcd.get_handle(&lcd_panels);

  lv_init();

  size_t buffer_size = sizeof(lv_color_t) * LCD_H_RES * LCD_V_RES;
  buf = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  assert(buf);
  assert(buf1);
  lv_disp_draw_buf_init(&draw_buf, buf, buf1, LCD_H_RES * LCD_V_RES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = false;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  esp_lcd_dpi_panel_event_callbacks_t cbs = { 0 };
  cbs.on_color_trans_done = lvgl_port_flush_dpi_panel_ready_callback;
  esp_lcd_dpi_panel_register_event_callbacks(lcd_panels.panel, &cbs, &disp_drv);

  init_backlight();
  setup_ui();

  if (connect_wifi()) {
    String ip_text = "IP: " + WiFi.localIP().toString();
    lv_label_set_text(ip_label, ip_text.c_str());
    setup_ota();
  } else {
    lv_label_set_text(ip_label, "IP: No WiFi");
    set_status_text("WiFi Failed");
  }

  init_time();
  update_clock_labels();
  update_apod_image();

  last_apod_check_ms = millis();
  last_clock_update_ms = millis();
  last_brightness_update_ms = millis();

  Serial.println("Setup complete!");
}

void loop() {
  ArduinoOTA.handle();
  lv_timer_handler();

  unsigned long now = millis();

  if (now - last_clock_update_ms >= kClockUpdateIntervalMs) {
    update_clock_labels();
    last_clock_update_ms = now;
  }

  if (now - last_apod_check_ms >= kApodCheckIntervalMs) {
    update_apod_image();
    last_apod_check_ms = now;
  }

  if (now - last_brightness_update_ms >= kBrightnessIntervalMs) {
    update_backlight();
    last_brightness_update_ms = now;
  }

  delay(5);
}
