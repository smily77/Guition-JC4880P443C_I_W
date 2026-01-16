#pragma GCC push_options
#pragma GCC optimize("O3")

#include <Arduino.h>
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "pins_config.h"
#include "src/lcd/st7701_lcd.h"
#include "src/touch/gt911_touch.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

// WiFi Credentials
#include "D:/Credentials.h"

bsp_lcd_handles_t lcd_panels;

st7701_lcd lcd = st7701_lcd(LCD_RST);
gt911_touch touch = gt911_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
static lv_color_t *buf1;

// UI Labels
static lv_obj_t *hello_label;
static lv_obj_t *ip_label;
static lv_obj_t *status_label;

static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx) {
  lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
  assert(disp_drv != NULL);
  lv_disp_flush_ready(disp_drv);
  return false;
}

// Display flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  lcd.lcd_draw_bitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, &color_p->full);
}

// Touch input callback
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

// WiFi verbinden
bool connect_wifi() {
  Serial.println("[WiFi] Verbinde...");
  lv_label_set_text(status_label, "Connecting WiFi...");
  lv_timer_handler();

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
  } else {
    Serial.println("\n[WiFi] Fehlgeschlagen!");
    return false;
  }
}

// OTA initialisieren
void setup_ota() {
  ArduinoOTA.setHostname("HelloWorld-OTA");

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Update startet...");
    lv_label_set_text(status_label, "OTA Update...");
    lv_timer_handler();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update fertig!");
    lv_label_set_text(status_label, "OTA Done! Rebooting...");
    lv_timer_handler();
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

void setup() {
  Serial.begin(115200);
  Serial.println("Hello World OTA - Initialization");

  // Initialize I2C for touch controller
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

  // Initialize LCD and touch drivers
  lcd.begin();
  touch.begin();
  lcd.get_handle(&lcd_panels);

  // Initialize LVGL
  lv_init();

  // Allocate display buffers
  size_t buffer_size = sizeof(lv_color_t) * LCD_H_RES * LCD_V_RES;
  buf = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  assert(buf);
  assert(buf1);
  lv_disp_draw_buf_init(&draw_buf, buf, buf1, LCD_H_RES * LCD_V_RES);

  // Register display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = false;
  lv_disp_drv_register(&disp_drv);

  // Register touch input driver
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // Register display flush callback
  esp_lcd_dpi_panel_event_callbacks_t cbs = { 0 };
  cbs.on_color_trans_done = lvgl_port_flush_dpi_panel_ready_callback;
  esp_lcd_dpi_panel_register_event_callbacks(lcd_panels.panel, &cbs, &disp_drv);

  Serial.println("LVGL Ready");

  // Create UI
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000033), 0);

  // Hello World label (center)
  hello_label = lv_label_create(lv_scr_act());
  lv_label_set_text(hello_label, "Hello World!");
  lv_obj_set_style_text_color(hello_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(hello_label, &lv_font_montserrat_42, 0);
  lv_obj_align(hello_label, LV_ALIGN_CENTER, 0, -50);

  // IP label (below center)
  ip_label = lv_label_create(lv_scr_act());
  lv_label_set_text(ip_label, "IP: ---");
  lv_obj_set_style_text_color(ip_label, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_28, 0);
  lv_obj_align(ip_label, LV_ALIGN_CENTER, 0, 30);

  // Status label (bottom)
  status_label = lv_label_create(lv_scr_act());
  lv_label_set_text(status_label, "Starting...");
  lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
  lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -20);

  lv_timer_handler();

  // Connect WiFi
  if (connect_wifi()) {
    // Update IP display
    String ip_text = "IP: " + WiFi.localIP().toString();
    lv_label_set_text(ip_label, ip_text.c_str());
    lv_label_set_text(status_label, "OTA Ready");

    // Setup OTA
    setup_ota();
  } else {
    lv_label_set_text(ip_label, "IP: No WiFi");
    lv_label_set_text(status_label, "WiFi Failed");
  }

  Serial.println("Setup complete!");
}

void loop() {
  ArduinoOTA.handle();
  lv_timer_handler();
  delay(5);
}
