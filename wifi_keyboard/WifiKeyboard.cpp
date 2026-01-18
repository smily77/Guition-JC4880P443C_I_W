#include "WifiKeyboard.h"

namespace WifiKeyboard
{
namespace
{
static Preferences *prefs_ptr = nullptr;
struct Session
{
    bool active;
    lv_obj_t *container;
    lv_obj_t *keyboard;
    lv_obj_t *ssid_dropdown;
    lv_obj_t *ssid_textarea;
    lv_obj_t *password_textarea;
    lv_obj_t *status_label;
    lv_obj_t *password_toggle;
    lv_obj_t *password_toggle_label;
    lv_obj_t *active_textarea;
    const char *active_key;
};

static Session session;

static char ssid[33] = "";
static char password[65] = "";
static bool wifi_ready = false;
static constexpr size_t kMaxSsids = 15;
static char ssid_list[kMaxSsids][33] = {};
static size_t ssid_count = 0;

static void update_active_field(Session *ctx, const char *field_label);

static void trim_trailing_whitespace(char *value)
{
    if (value == nullptr)
    {
        return;
    }

    size_t len = strlen(value);
    while (len > 0)
    {
        char last = value[len - 1];
        if (last == '\n' || last == '\r' || last == ' ' || last == '\t')
        {
            value[len - 1] = '\0';
            len--;
        }
        else
        {
            break;
        }
    }
}

static void refresh_ssid_list()
{
    if (!wifi_ready)
    {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        wifi_ready = true;
    }

    int count = WiFi.scanNetworks(false, true);
    ssid_count = 0;
    if (count <= 0)
    {
        return;
    }

    for (int i = 0; i < count && ssid_count < kMaxSsids; ++i)
    {
        String name = WiFi.SSID(i);
        if (name.length() == 0)
        {
            continue;
        }

        bool duplicate = false;
        for (size_t j = 0; j < ssid_count; ++j)
        {
            if (name.equals(ssid_list[j]))
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            continue;
        }

        name.toCharArray(ssid_list[ssid_count], sizeof(ssid_list[ssid_count]));
        ssid_count++;
    }

    WiFi.scanDelete();
}

static void build_ssid_dropdown_options(char *buffer, size_t buffer_size)
{
    if (buffer == nullptr || buffer_size == 0)
    {
        return;
    }

    buffer[0] = '\0';
    strlcpy(buffer, "SSID Auswahl", buffer_size);
    if (ssid_count == 0)
    {
        strlcat(buffer, "\nKeine SSID gefunden", buffer_size);
        return;
    }

    for (size_t i = 0; i < ssid_count; ++i)
    {
        strlcat(buffer, "\n", buffer_size);
        strlcat(buffer, ssid_list[i], buffer_size);
    }
}

static void ssid_dropdown_event(lv_event_t *e)
{
    Session *ctx = static_cast<Session *>(lv_event_get_user_data(e));
    if (ctx == nullptr || ctx->ssid_dropdown == nullptr)
    {
        return;
    }

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
    {
        return;
    }

    uint16_t selected = lv_dropdown_get_selected(ctx->ssid_dropdown);
    if (selected == 0 || selected > ssid_count)
    {
        return;
    }

    const char *selected_ssid = ssid_list[selected - 1];
    strncpy(ssid, selected_ssid, sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    lv_textarea_set_text(ctx->ssid_textarea, ssid);
    ctx->active_textarea = ctx->ssid_textarea;
    ctx->active_key = "ssid";
    update_active_field(ctx, "SSID");
    lv_keyboard_set_textarea(ctx->keyboard, ctx->active_textarea);
    lv_obj_clear_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void update_active_field(Session *ctx, const char *field_label)
{
    if (ctx == nullptr)
    {
        return;
    }

    lv_label_set_text_fmt(ctx->status_label, "Eingabe: %s", field_label);

    lv_color_t inactive_color = lv_color_white();
    lv_color_t active_color = lv_palette_main(LV_PALETTE_BLUE);

    if (ctx->active_textarea == ctx->ssid_textarea)
    {
        lv_obj_set_style_border_color(ctx->ssid_textarea, active_color, 0);
        lv_obj_set_style_border_color(ctx->password_textarea, inactive_color, 0);
    }
    else if (ctx->active_textarea == ctx->password_textarea)
    {
        lv_obj_set_style_border_color(ctx->ssid_textarea, inactive_color, 0);
        lv_obj_set_style_border_color(ctx->password_textarea, active_color, 0);
    }
}

static void password_toggle_event(lv_event_t *e)
{
    Session *ctx = static_cast<Session *>(lv_event_get_user_data(e));
    if (ctx == nullptr || ctx->password_textarea == nullptr)
    {
        return;
    }

    bool show_password = lv_obj_has_state(ctx->password_toggle, LV_STATE_CHECKED);
    lv_textarea_set_password_mode(ctx->password_textarea, !show_password);
}

static void keyboard_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    Session *ctx = static_cast<Session *>(lv_event_get_user_data(e));
    if (ctx == nullptr)
    {
        return;
    }

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED)
    {
        if (target == ctx->ssid_textarea)
        {
            ctx->active_textarea = ctx->ssid_textarea;
            ctx->active_key = "ssid";
            update_active_field(ctx, "SSID");
        }
        else if (target == ctx->password_textarea)
        {
            ctx->active_textarea = ctx->password_textarea;
            ctx->active_key = "password";
            update_active_field(ctx, "Passwort");
        }
        else
        {
            return;
        }

        lv_keyboard_set_textarea(ctx->keyboard, ctx->active_textarea);
        lv_obj_clear_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (code == LV_EVENT_READY)
    {
        if (ctx->active_textarea != nullptr && ctx->active_key != nullptr && prefs_ptr != nullptr)
        {
            const char *text = lv_textarea_get_text(ctx->active_textarea);

            if (strcmp(ctx->active_key, "ssid") == 0)
            {
                strncpy(ssid, text, sizeof(ssid) - 1);
                ssid[sizeof(ssid) - 1] = '\0';
                trim_trailing_whitespace(ssid);
            }
            else if (strcmp(ctx->active_key, "password") == 0)
            {
                strncpy(password, text, sizeof(password) - 1);
                password[sizeof(password) - 1] = '\0';
                trim_trailing_whitespace(password);
            }

            const char *value_to_store = text;
            if (strcmp(ctx->active_key, "ssid") == 0)
            {
                value_to_store = ssid;
            }
            else if (strcmp(ctx->active_key, "password") == 0)
            {
                value_to_store = password;
            }

            prefs_ptr->putString(ctx->active_key, value_to_store);
            lv_label_set_text_fmt(ctx->status_label, "%s gespeichert", ctx->active_key);
        }

        lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(ctx->keyboard, nullptr);
        lv_obj_del_async(ctx->container);
        *ctx = {};
        return;
    }

    if (code == LV_EVENT_CANCEL)
    {
        lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(ctx->keyboard, nullptr);
        lv_obj_del_async(ctx->container);
        *ctx = {};
    }
}
}

void begin(Preferences *prefs)
{
    prefs_ptr = prefs;
    if (prefs_ptr == nullptr)
    {
        return;
    }

    prefs_ptr->getString("ssid", ssid, sizeof(ssid));
    prefs_ptr->getString("password", password, sizeof(password));
}

void start()
{
    if (session.active)
    {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (indev != nullptr)
    {
        lv_indev_wait_release(indev);
    }

    session.active = true;
    session.container = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(session.container);
    lv_obj_set_size(session.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(session.container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(session.container, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(session.container);
    lv_label_set_text(title, "WLAN Zugangsdaten");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *ssid_label = lv_label_create(session.container);
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 20, 60);
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ssid_label, lv_color_white(), 0);

    session.status_label = lv_label_create(session.container);
    lv_label_set_text(session.status_label, "Eingabe: SSID");
    lv_obj_align(session.status_label, LV_ALIGN_TOP_LEFT, 20, 300);
    lv_obj_set_style_text_font(session.status_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(session.status_label, lv_color_white(), 0);

    lv_label_set_text(session.status_label, "Suche SSID...");
    lv_timer_handler();
    refresh_ssid_list();
    lv_label_set_text(session.status_label, "Eingabe: SSID");

    session.ssid_dropdown = lv_dropdown_create(session.container);
    lv_obj_set_width(session.ssid_dropdown, LV_HOR_RES - 40);
    lv_obj_align(session.ssid_dropdown, LV_ALIGN_TOP_LEFT, 20, 85);
    lv_dropdown_set_symbol(session.ssid_dropdown, LV_SYMBOL_DOWN);
    lv_dropdown_set_dir(session.ssid_dropdown, LV_DIR_BOTTOM);
    lv_obj_set_style_max_height(session.ssid_dropdown, LV_VER_RES / 2, 0);
    lv_obj_set_style_text_font(session.ssid_dropdown, &lv_font_montserrat_18, 0);
    lv_obj_set_style_border_width(session.ssid_dropdown, 2, 0);
    lv_obj_set_style_border_color(session.ssid_dropdown, lv_color_white(), 0);
    char dropdown_options[512];
    build_ssid_dropdown_options(dropdown_options, sizeof(dropdown_options));
    lv_dropdown_set_options(session.ssid_dropdown, dropdown_options);
    lv_obj_add_event_cb(session.ssid_dropdown, ssid_dropdown_event, LV_EVENT_ALL, &session);

    session.ssid_textarea = lv_textarea_create(session.container);
    lv_obj_set_width(session.ssid_textarea, LV_HOR_RES - 40);
    lv_obj_align(session.ssid_textarea, LV_ALIGN_TOP_LEFT, 20, 135);
    lv_textarea_set_placeholder_text(session.ssid_textarea, "SSID eingeben");
    lv_textarea_set_text(session.ssid_textarea, ssid);
    lv_obj_set_style_text_font(session.ssid_textarea, &lv_font_montserrat_18, 0);
    lv_obj_set_style_border_width(session.ssid_textarea, 2, 0);
    lv_obj_set_style_border_color(session.ssid_textarea, lv_color_white(), 0);
    lv_obj_add_event_cb(session.ssid_textarea, keyboard_event, LV_EVENT_ALL, &session);

    lv_obj_t *pass_label = lv_label_create(session.container);
    lv_label_set_text(pass_label, "Passwort:");
    lv_obj_align(pass_label, LV_ALIGN_TOP_LEFT, 20, 190);
    lv_obj_set_style_text_font(pass_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(pass_label, lv_color_white(), 0);

    session.password_textarea = lv_textarea_create(session.container);
    lv_obj_set_width(session.password_textarea, LV_HOR_RES - 40);
    lv_obj_align(session.password_textarea, LV_ALIGN_TOP_LEFT, 20, 215);
    lv_textarea_set_placeholder_text(session.password_textarea, "Passwort eingeben");
    lv_textarea_set_password_mode(session.password_textarea, true);
    lv_textarea_set_text(session.password_textarea, password);
    lv_obj_set_style_text_font(session.password_textarea, &lv_font_montserrat_18, 0);
    lv_obj_set_style_border_width(session.password_textarea, 2, 0);
    lv_obj_set_style_border_color(session.password_textarea, lv_color_white(), 0);
    lv_obj_add_event_cb(session.password_textarea, keyboard_event, LV_EVENT_ALL, &session);

    session.password_toggle = lv_switch_create(session.container);
    lv_obj_align(session.password_toggle, LV_ALIGN_TOP_LEFT, 20, 260);
    lv_obj_add_event_cb(session.password_toggle, password_toggle_event, LV_EVENT_VALUE_CHANGED, &session);

    session.password_toggle_label = lv_label_create(session.container);
    lv_label_set_text(session.password_toggle_label, "Passwort anzeigen");
    lv_obj_align_to(session.password_toggle_label, session.password_toggle, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_text_font(session.password_toggle_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(session.password_toggle_label, lv_color_white(), 0);

    session.keyboard = lv_keyboard_create(session.container);
    lv_obj_set_size(session.keyboard, LV_HOR_RES, LV_VER_RES / 2);
    lv_obj_align(session.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(session.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_flag(session.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(session.keyboard, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(session.keyboard, keyboard_event, LV_EVENT_ALL, &session);

    session.active_textarea = session.ssid_textarea;
    session.active_key = "ssid";
    lv_keyboard_set_textarea(session.keyboard, session.active_textarea);
    lv_obj_clear_flag(session.keyboard, LV_OBJ_FLAG_HIDDEN);
    update_active_field(&session, "SSID");
}

const char *getSsid()
{
    return ssid;
}

const char *getPassword()
{
    return password;
}
}
