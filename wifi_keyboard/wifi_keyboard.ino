#pragma GCC push_options
#pragma GCC optimize("O3")

#include <Arduino.h>
#include <Preferences.h>
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "pins_config.h"
#include "src/lcd/st7701_lcd.h"
#include "src/touch/gt911_touch.h"

bsp_lcd_handles_t lcd_panels;

st7701_lcd lcd = st7701_lcd(LCD_RST);
gt911_touch touch = gt911_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
static lv_color_t *buf1;

static Preferences prefs;
static constexpr const char *kNvsNamespace = "wifi";

static lv_obj_t *keyboard;
static lv_obj_t *ssid_textarea;
static lv_obj_t *password_textarea;
static lv_obj_t *status_label;

static lv_obj_t *active_textarea;
static const char *active_key;

static char ssid[33] = "";
static char password[65] = "";

static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    assert(disp_drv != NULL);
    lv_disp_flush_ready(disp_drv);

    return false;
}

// Display flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    lcd.lcd_draw_bitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, &color_p->full);
}

// Touch input callback
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    bool touched;
    uint16_t touchX, touchY;

    touched = touch.getTouch(&touchX, &touchY);

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

// Rotation callback - updates touch orientation when display is rotated
static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        touch.set_rotation(0);
        break;
    case LV_DISP_ROT_90:
        touch.set_rotation(1);
        break;
    case LV_DISP_ROT_180:
        touch.set_rotation(2);
        break;
    case LV_DISP_ROT_270:
        touch.set_rotation(3);
        break;
    }
}

static void keyboard_session_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED && target == lv_scr_act())
    {
        if (lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN))
        {
            if (active_textarea == nullptr)
            {
                active_textarea = ssid_textarea;
                active_key = "ssid";
            }
            lv_keyboard_set_textarea(keyboard, active_textarea);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED)
    {
        if (target == ssid_textarea)
        {
            active_textarea = ssid_textarea;
            active_key = "ssid";
        }
        else if (target == password_textarea)
        {
            active_textarea = password_textarea;
            active_key = "password";
        }
        else
        {
            return;
        }

        lv_keyboard_set_textarea(keyboard, active_textarea);
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (code == LV_EVENT_READY)
    {
        if (active_textarea != nullptr && active_key != nullptr)
        {
            const char *text = lv_textarea_get_text(active_textarea);

            if (strcmp(active_key, "ssid") == 0)
            {
                strncpy(ssid, text, sizeof(ssid) - 1);
                ssid[sizeof(ssid) - 1] = '\0';
            }
            else if (strcmp(active_key, "password") == 0)
            {
                strncpy(password, text, sizeof(password) - 1);
                password[sizeof(password) - 1] = '\0';
            }

            prefs.putString(active_key, text);
            lv_label_set_text_fmt(status_label, "%s gespeichert", active_key);
        }

        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(keyboard, nullptr);
        return;
    }

    if (code == LV_EVENT_CANCEL)
    {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(keyboard, nullptr);
    }
}

static void create_ui()
{
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "WLAN Zugangsdaten");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    lv_obj_t *ssid_label = lv_label_create(lv_scr_act());
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 20, 60);
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);

    ssid_textarea = lv_textarea_create(lv_scr_act());
    lv_obj_set_width(ssid_textarea, LCD_H_RES - 40);
    lv_obj_align(ssid_textarea, LV_ALIGN_TOP_LEFT, 20, 85);
    lv_textarea_set_placeholder_text(ssid_textarea, "SSID eingeben");
    lv_textarea_set_text(ssid_textarea, ssid);
    lv_obj_add_event_cb(ssid_textarea, keyboard_session_event, LV_EVENT_ALL, nullptr);

    lv_obj_t *pass_label = lv_label_create(lv_scr_act());
    lv_label_set_text(pass_label, "Passwort:");
    lv_obj_align(pass_label, LV_ALIGN_TOP_LEFT, 20, 140);
    lv_obj_set_style_text_font(pass_label, &lv_font_montserrat_14, 0);

    password_textarea = lv_textarea_create(lv_scr_act());
    lv_obj_set_width(password_textarea, LCD_H_RES - 40);
    lv_obj_align(password_textarea, LV_ALIGN_TOP_LEFT, 20, 165);
    lv_textarea_set_placeholder_text(password_textarea, "Passwort eingeben");
    lv_textarea_set_password_mode(password_textarea, true);
    lv_textarea_set_text(password_textarea, password);
    lv_obj_add_event_cb(password_textarea, keyboard_session_event, LV_EVENT_ALL, nullptr);

    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label, "Beruhre den Bildschirm für die Tastatur");
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 20, 220);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);

    keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(keyboard, LCD_H_RES, LCD_V_RES / 2);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(keyboard, keyboard_session_event, LV_EVENT_ALL, nullptr);

    lv_obj_add_event_cb(lv_scr_act(), keyboard_session_event, LV_EVENT_ALL, nullptr);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("LVGL Initialization");

    prefs.begin(kNvsNamespace, false);
    prefs.getString("ssid", ssid, sizeof(ssid));
    prefs.getString("password", password, sizeof(password));

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
    size_t buffer_size = sizeof(int16_t) * LCD_H_RES * LCD_V_RES;
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
    esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
    cbs.on_color_trans_done = lvgl_port_flush_dpi_panel_ready_callback;
    esp_lcd_dpi_panel_register_event_callbacks(lcd_panels.panel, &cbs, &disp_drv);

    Serial.println("LVGL Ready");

    create_ui();
}

void loop()
{
    lv_timer_handler();
    delay(5);
}
