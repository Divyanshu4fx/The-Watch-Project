#include "find_phone.h"
#include "ble.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "FIND_PHONE";

volatile bool find_phone_screen_active = false;
volatile bool find_phone_ringing = false;

// LVGL objects
static lv_obj_t *fp_screen = NULL;
static lv_obj_t *fp_title_label = NULL;
static lv_obj_t *fp_icon_label = NULL;
static lv_obj_t *fp_status_label = NULL;
static lv_obj_t *fp_hint_label = NULL;

static void create_find_phone_screen(void)
{
    if (fp_screen != NULL) return;

    fp_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(fp_screen, 0, 0);
    lv_obj_set_size(fp_screen, 240, 135);
    lv_obj_clear_flag(fp_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(fp_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(fp_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fp_screen, 0, 0);
    lv_obj_set_style_pad_all(fp_screen, 0, 0);
    lv_obj_set_style_radius(fp_screen, 0, 0);

    // Title
    fp_title_label = lv_label_create(fp_screen);
    lv_obj_set_style_text_color(fp_title_label, lv_color_hex(0x55FF55), 0);
    lv_obj_set_style_text_font(fp_title_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(fp_title_label, "Find My Phone");
    lv_obj_align(fp_title_label, LV_ALIGN_TOP_MID, 0, 8);

    // Phone icon
    fp_icon_label = lv_label_create(fp_screen);
    lv_obj_set_style_text_color(fp_icon_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(fp_icon_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(fp_icon_label, LV_SYMBOL_CALL);
    lv_obj_align(fp_icon_label, LV_ALIGN_CENTER, 0, -8);

    // Status label
    fp_status_label = lv_label_create(fp_screen);
    lv_obj_set_style_text_font(fp_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(fp_status_label, "Tap to Ring");
    lv_obj_set_style_text_color(fp_status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(fp_status_label, LV_ALIGN_CENTER, 0, 18);

    // Hint
    fp_hint_label = lv_label_create(fp_screen);
    lv_obj_set_style_text_color(fp_hint_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(fp_hint_label, &lv_font_montserrat_10, 0);
    lv_label_set_text(fp_hint_label, "Long press to exit");
    lv_obj_align(fp_hint_label, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_obj_add_flag(fp_screen, LV_OBJ_FLAG_HIDDEN);
}

static void update_ui(void)
{
    if (fp_screen == NULL) return;

    if (find_phone_ringing) {
        lv_label_set_text(fp_status_label, "Ringing...");
        lv_obj_set_style_text_color(fp_status_label, lv_color_hex(0x55FF55), 0);
        lv_obj_set_style_text_color(fp_icon_label, lv_color_hex(0x55FF55), 0);
    } else {
        lv_label_set_text(fp_status_label, "Tap to Ring");
        lv_obj_set_style_text_color(fp_status_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_color(fp_icon_label, lv_color_hex(0xFFFFFF), 0);
    }
}

void find_phone_init(void)
{
    find_phone_screen_active = false;
    find_phone_ringing = false;
}

void show_find_phone_screen(void)
{
    create_find_phone_screen();
    lv_obj_clear_flag(fp_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(fp_screen);
    find_phone_screen_active = true;
    update_ui();
    ESP_LOGI(TAG, "Find Phone screen shown");
}

void hide_find_phone_screen(void)
{
    if (find_phone_ringing) {
        find_phone_ringing = false;
        ble_notify_find_phone(0x00);
    }

    if (fp_screen != NULL) {
        lv_obj_add_flag(fp_screen, LV_OBJ_FLAG_HIDDEN);
    }
    find_phone_screen_active = false;
    ESP_LOGI(TAG, "Find Phone screen hidden");
}

void toggle_find_phone_ring(void)
{
    if (!is_ble_connected()) {
        ESP_LOGW(TAG, "BLE not connected, cannot find phone");
        return;
    }

    find_phone_ringing = !find_phone_ringing;

    if (find_phone_ringing) {
        ble_notify_find_phone(0x01);
        ESP_LOGI(TAG, "Find Phone: RING START");
    } else {
        ble_notify_find_phone(0x00);
        ESP_LOGI(TAG, "Find Phone: RING STOP");
    }

    update_ui();
}