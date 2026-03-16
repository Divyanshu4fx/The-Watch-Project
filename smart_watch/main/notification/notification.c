#include "notification.h"

static const char *TAG = "NOTIF";

notification_t notifications[MAX_NOTIFICATIONS];
volatile uint8_t notification_count = 0;
volatile uint8_t unread_count = 0;
volatile bool notification_screen_active = false;

// ── Internal UI state ───────────────────────────────────────
bool detail_view_active = false;
int8_t selected_index = 0;      // Currently highlighted notification in list
static int8_t scroll_offset = 0;       // First visible item index in list
static int8_t detail_notif_index = -1; // Which notification is shown in detail

#define VISIBLE_ITEMS 3  // How many notifications fit on 135px height

// ── Forward declarations for internal UI helpers ────────────
static void render_notification_list(void);
static void render_notification_detail(uint8_t index);

// ── LVGL objects we create at runtime ───────────────────────
// List screen
static lv_obj_t *notif_screen = NULL;
static lv_obj_t *notif_header_bar = NULL;
static lv_obj_t *notif_header_label = NULL;
static lv_obj_t *notif_count_label = NULL;
static lv_obj_t *notif_list_container = NULL;
static lv_obj_t *notif_item_panels[VISIBLE_ITEMS];
static lv_obj_t *notif_item_app_labels[VISIBLE_ITEMS];
static lv_obj_t *notif_item_title_labels[VISIBLE_ITEMS];
static lv_obj_t *notif_item_time_labels[VISIBLE_ITEMS];
static lv_obj_t *notif_item_dot[VISIBLE_ITEMS];  // unread indicator
static lv_obj_t *notif_empty_label = NULL;

// Detail screen
static lv_obj_t *detail_container = NULL;
static lv_obj_t *detail_app_label = NULL;
static lv_obj_t *detail_title_label = NULL;
static lv_obj_t *detail_body_label = NULL;
static lv_obj_t *detail_time_label = NULL;
static lv_obj_t *detail_divider = NULL;

// Incoming notification popup (toast)
static lv_obj_t *popup_container = NULL;
static lv_obj_t *popup_app_label = NULL;
static lv_obj_t *popup_title_label = NULL;
static esp_timer_handle_t popup_timer = NULL;

/* ================================================================
 *  STYLE DEFINITIONS (matching your black/white/green watch theme)
 * ================================================================ */
static lv_style_t style_screen_bg;
static lv_style_t style_header;
static lv_style_t style_item_normal;
static lv_style_t style_item_selected;
static lv_style_t style_text_primary;
static lv_style_t style_text_secondary;
static lv_style_t style_text_app;
static lv_style_t style_unread_dot;
static lv_style_t style_popup;
static bool styles_initialized = false;

static void init_styles(void)
{
    if (styles_initialized) return;

    // Screen background — deep black
    lv_style_init(&style_screen_bg);
    lv_style_set_bg_color(&style_screen_bg, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&style_screen_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_screen_bg, 0);
    lv_style_set_pad_all(&style_screen_bg, 0);
    lv_style_set_radius(&style_screen_bg, 0);

    // Header bar — subtle dark grey
    lv_style_init(&style_header);
    lv_style_set_bg_color(&style_header, lv_color_hex(0x1A1A1A));
    lv_style_set_bg_opa(&style_header, LV_OPA_COVER);
    lv_style_set_border_width(&style_header, 0);
    lv_style_set_pad_hor(&style_header, 6);
    lv_style_set_pad_ver(&style_header, 2);
    lv_style_set_radius(&style_header, 0);

    // Notification item — normal (dark card)
    lv_style_init(&style_item_normal);
    lv_style_set_bg_color(&style_item_normal, lv_color_hex(0x111111));
    lv_style_set_bg_opa(&style_item_normal, LV_OPA_COVER);
    lv_style_set_border_color(&style_item_normal, lv_color_hex(0x222222));
    lv_style_set_border_width(&style_item_normal, 1);
    lv_style_set_border_side(&style_item_normal, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_pad_hor(&style_item_normal, 6);
    lv_style_set_pad_ver(&style_item_normal, 3);
    lv_style_set_radius(&style_item_normal, 4);

    // Notification item — selected/highlighted
    lv_style_init(&style_item_selected);
    lv_style_set_bg_color(&style_item_selected, lv_color_hex(0x1B3A1B));
    lv_style_set_bg_opa(&style_item_selected, LV_OPA_COVER);
    lv_style_set_border_color(&style_item_selected, lv_color_hex(0x55FF55));
    lv_style_set_border_width(&style_item_selected, 1);
    lv_style_set_border_side(&style_item_selected, LV_BORDER_SIDE_LEFT);
    lv_style_set_pad_hor(&style_item_selected, 6);
    lv_style_set_pad_ver(&style_item_selected, 3);
    lv_style_set_radius(&style_item_selected, 4);

    // Primary text — white
    lv_style_init(&style_text_primary);
    lv_style_set_text_color(&style_text_primary, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&style_text_primary, &lv_font_montserrat_14);

    // Secondary text — grey
    lv_style_init(&style_text_secondary);
    lv_style_set_text_color(&style_text_secondary, lv_color_hex(0x888888));
    lv_style_set_text_font(&style_text_secondary, &lv_font_montserrat_12);

    // App name text — green accent
    lv_style_init(&style_text_app);
    lv_style_set_text_color(&style_text_app, lv_color_hex(0x55FF55));
    lv_style_set_text_font(&style_text_app, &lv_font_montserrat_10);

    // Unread dot — bright green
    lv_style_init(&style_unread_dot);
    lv_style_set_bg_color(&style_unread_dot, lv_color_hex(0x55FF55));
    lv_style_set_bg_opa(&style_unread_dot, LV_OPA_COVER);
    lv_style_set_radius(&style_unread_dot, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_unread_dot, 0);

    // Popup toast
    lv_style_init(&style_popup);
    lv_style_set_bg_color(&style_popup, lv_color_hex(0x1B3A1B));
    lv_style_set_bg_opa(&style_popup, LV_OPA_90);
    lv_style_set_border_color(&style_popup, lv_color_hex(0x55FF55));
    lv_style_set_border_width(&style_popup, 1);
    lv_style_set_radius(&style_popup, 8);
    lv_style_set_pad_all(&style_popup, 6);
    lv_style_set_shadow_width(&style_popup, 10);
    lv_style_set_shadow_color(&style_popup, lv_color_hex(0x003300));
    lv_style_set_shadow_opa(&style_popup, LV_OPA_60);

    styles_initialized = true;
}

/* ================================================================
 *  DATA MANAGEMENT
 * ================================================================ */

void notifications_init(void)
{
    memset(notifications, 0, sizeof(notifications));
    notification_count = 0;
    unread_count = 0;
    // init_styles();
    ESP_LOGI(TAG, "Notification system initialized.");
}

int add_notification(const char *app_name, const char *title, const char *body, uint8_t hour, uint8_t minute)
{
    // Find an empty slot, or overwrite the oldest (index 0) after shifting
    int slot = -1;
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (!notifications[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        // Buffer full → shift everything down, discard oldest
        for (int i = 0; i < MAX_NOTIFICATIONS - 1; i++) {
            notifications[i] = notifications[i + 1];
        }
        slot = MAX_NOTIFICATIONS - 1;
        // notification_count stays the same
    } else {
        notification_count++;
    }

    memset(&notifications[slot], 0, sizeof(notification_t));
    strncpy(notifications[slot].app_name, app_name ? app_name : "App", NOTIF_APP_LEN - 1);
    strncpy(notifications[slot].title, title ? title : "", NOTIF_TITLE_LEN - 1);
    strncpy(notifications[slot].body, body ? body : "", NOTIF_BODY_LEN - 1);
    notifications[slot].hour = hour;
    notifications[slot].minute = minute;
    notifications[slot].unread = true;
    notifications[slot].active = true;

    unread_count++;

    ESP_LOGI(TAG, "Notification added [%d]: %s — %s", slot, app_name, title);
    return slot;
}

void remove_notification(uint8_t index)
{
    if (index >= MAX_NOTIFICATIONS || !notifications[index].active) return;

    if (notifications[index].unread) {
        if (unread_count > 0) unread_count--;
    }
    notifications[index].active = false;
    if (notification_count > 0) notification_count--;

    // Compact: shift items after removed one
    for (int i = index; i < MAX_NOTIFICATIONS - 1; i++) {
        notifications[i] = notifications[i + 1];
    }
    memset(&notifications[MAX_NOTIFICATIONS - 1], 0, sizeof(notification_t));
    render_notification_list();
}

void clear_all_notifications(void)
{
    memset(notifications, 0, sizeof(notifications));
    notification_count = 0;
    unread_count = 0;
}

void mark_notification_read(uint8_t index)
{
    if (index >= MAX_NOTIFICATIONS || !notifications[index].active) return;
    if (notifications[index].unread) {
        notifications[index].unread = false;
        if (unread_count > 0) unread_count--;
    }
}

/* ================================================================
 *  UI CREATION (called once, objects reused)
 * ================================================================ */

static void create_notification_list_ui(void)
{
    if (notif_screen != NULL) return; // Already created

    lv_obj_t *parent = lv_scr_act();

    // Main container (full screen overlay)
    notif_screen = lv_obj_create(parent);
    lv_obj_set_size(notif_screen, 240, 135);
    lv_obj_set_pos(notif_screen, 0, 0);
    lv_obj_add_style(notif_screen, &style_screen_bg, 0);
    lv_obj_set_scrollbar_mode(notif_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(notif_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── Header bar ──
    notif_header_bar = lv_obj_create(notif_screen);
    lv_obj_set_size(notif_header_bar, 240, 22);
    lv_obj_set_pos(notif_header_bar, 0, 0);
    lv_obj_add_style(notif_header_bar, &style_header, 0);
    lv_obj_clear_flag(notif_header_bar, LV_OBJ_FLAG_SCROLLABLE);

    notif_header_label = lv_label_create(notif_header_bar);
    lv_label_set_text(notif_header_label, LV_SYMBOL_BELL " Notifications");
    lv_obj_set_style_text_color(notif_header_label, lv_color_hex(0x55FF55), 0);
    lv_obj_set_style_text_font(notif_header_label, &lv_font_montserrat_12, 0);
    lv_obj_align(notif_header_label, LV_ALIGN_LEFT_MID, 0, 0);

    notif_count_label = lv_label_create(notif_header_bar);
    lv_label_set_text(notif_count_label, "0");
    lv_obj_set_style_text_color(notif_count_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(notif_count_label, &lv_font_montserrat_10, 0);
    lv_obj_align(notif_count_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // ── List container ──
    notif_list_container = lv_obj_create(notif_screen);
    lv_obj_set_size(notif_list_container, 240, 113);
    lv_obj_set_pos(notif_list_container, 0, 22);
    lv_obj_add_style(notif_list_container, &style_screen_bg, 0);
    lv_obj_set_scrollbar_mode(notif_list_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(notif_list_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(notif_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(notif_list_container, 2, 0);
    lv_obj_set_style_pad_all(notif_list_container, 2, 0);

    // ── Pre-create item panels ──
    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        notif_item_panels[i] = lv_obj_create(notif_list_container);
        lv_obj_set_size(notif_item_panels[i], 234, 35);
        lv_obj_add_style(notif_item_panels[i], &style_item_normal, 0);
        lv_obj_clear_flag(notif_item_panels[i], LV_OBJ_FLAG_SCROLLABLE);

        // Unread dot
        notif_item_dot[i] = lv_obj_create(notif_item_panels[i]);
        lv_obj_set_size(notif_item_dot[i], 6, 6);
        lv_obj_add_style(notif_item_dot[i], &style_unread_dot, 0);
        lv_obj_align(notif_item_dot[i], LV_ALIGN_LEFT_MID, 0, 0);

        // App name
        notif_item_app_labels[i] = lv_label_create(notif_item_panels[i]);
        lv_obj_add_style(notif_item_app_labels[i], &style_text_app, 0);
        lv_obj_align(notif_item_app_labels[i], LV_ALIGN_TOP_LEFT, 10, 0);
        lv_label_set_text(notif_item_app_labels[i], "");

        // Title
        notif_item_title_labels[i] = lv_label_create(notif_item_panels[i]);
        lv_obj_add_style(notif_item_title_labels[i], &style_text_primary, 0);
        lv_obj_set_style_text_font(notif_item_title_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_align(notif_item_title_labels[i], LV_ALIGN_BOTTOM_LEFT, 10, 0);
        lv_label_set_long_mode(notif_item_title_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(notif_item_title_labels[i], 185);
        lv_label_set_text(notif_item_title_labels[i], "");

        // Time
        notif_item_time_labels[i] = lv_label_create(notif_item_panels[i]);
        lv_obj_add_style(notif_item_time_labels[i], &style_text_secondary, 0);
        lv_obj_set_style_text_font(notif_item_time_labels[i], &lv_font_montserrat_10, 0);
        lv_obj_align(notif_item_time_labels[i], LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_label_set_text(notif_item_time_labels[i], "");

        lv_obj_add_flag(notif_item_panels[i], LV_OBJ_FLAG_HIDDEN);
    }

    // ── Empty state label ──
    notif_empty_label = lv_label_create(notif_screen);
    lv_label_set_text(notif_empty_label, "No notifications");
    lv_obj_set_style_text_color(notif_empty_label, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(notif_empty_label, &lv_font_montserrat_14, 0);
    lv_obj_align(notif_empty_label, LV_ALIGN_CENTER, 0, 10);

    // ── Detail view (overlays the list) ──
    detail_container = lv_obj_create(notif_screen);
    lv_obj_set_size(detail_container, 240, 113);
    lv_obj_set_pos(detail_container, 0, 22);
    lv_obj_add_style(detail_container, &style_screen_bg, 0);
    lv_obj_set_scrollbar_mode(detail_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(detail_container, 6, 0);
    lv_obj_set_flex_flow(detail_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(detail_container, 4, 0);

    detail_app_label = lv_label_create(detail_container);
    lv_obj_add_style(detail_app_label, &style_text_app, 0);
    lv_label_set_text(detail_app_label, "");

    detail_time_label = lv_label_create(detail_container);
    lv_obj_add_style(detail_time_label, &style_text_secondary, 0);
    lv_obj_set_style_text_font(detail_time_label, &lv_font_montserrat_10, 0);
    lv_label_set_text(detail_time_label, "");

    detail_divider = lv_obj_create(detail_container);
    lv_obj_set_size(detail_divider, 228, 1);
    lv_obj_set_style_bg_color(detail_divider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(detail_divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(detail_divider, 0, 0);
    lv_obj_set_style_pad_all(detail_divider, 0, 0);

    detail_title_label = lv_label_create(detail_container);
    lv_obj_add_style(detail_title_label, &style_text_primary, 0);
    lv_obj_set_width(detail_title_label, 226);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(detail_title_label, "");

    detail_body_label = lv_label_create(detail_container);
    lv_obj_add_style(detail_body_label, &style_text_secondary, 0);
    lv_obj_set_width(detail_body_label, 226);
    lv_label_set_long_mode(detail_body_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(detail_body_label, "");

    lv_obj_add_flag(detail_container, LV_OBJ_FLAG_HIDDEN);

    // Start hidden
    lv_obj_add_flag(notif_screen, LV_OBJ_FLAG_HIDDEN);
}

static void create_popup_ui(void)
{
    if (popup_container != NULL) return;

    lv_obj_t *parent = lv_scr_act();

    popup_container = lv_obj_create(parent);
    lv_obj_set_size(popup_container, 220, 40);
    lv_obj_align(popup_container, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_add_style(popup_container, &style_popup, 0);
    lv_obj_clear_flag(popup_container, LV_OBJ_FLAG_SCROLLABLE);

    popup_app_label = lv_label_create(popup_container);
    lv_obj_add_style(popup_app_label, &style_text_app, 0);
    lv_obj_align(popup_app_label, LV_ALIGN_TOP_LEFT, 0, -2);
    lv_label_set_text(popup_app_label, "");

    popup_title_label = lv_label_create(popup_container);
    lv_obj_set_style_text_color(popup_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(popup_title_label, &lv_font_montserrat_12, 0);
    lv_obj_align(popup_title_label, LV_ALIGN_BOTTOM_LEFT, 0, 2);
    lv_label_set_long_mode(popup_title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(popup_title_label, 200);
    lv_label_set_text(popup_title_label, "");

    lv_obj_add_flag(popup_container, LV_OBJ_FLAG_HIDDEN);
}

/* ================================================================
 *  RENDER HELPERS
 * ================================================================ */

static void render_notification_list(void)
{
    char count_buf[8];
    snprintf(count_buf, sizeof(count_buf), "%d", notification_count);
    lv_label_set_text(notif_count_label, count_buf);

    if (notification_count == 0) {
        lv_obj_clear_flag(notif_empty_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(notif_list_container, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(notif_empty_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(notif_list_container, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        int data_idx = scroll_offset + i;

        if (data_idx >= notification_count || !notifications[data_idx].active) {
            lv_obj_add_flag(notif_item_panels[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(notif_item_panels[i], LV_OBJ_FLAG_HIDDEN);

        // Highlight selected
        if (data_idx == selected_index) {
            lv_obj_remove_style(notif_item_panels[i], &style_item_normal, 0);
            lv_obj_add_style(notif_item_panels[i], &style_item_selected, 0);
        } else {
            lv_obj_remove_style(notif_item_panels[i], &style_item_selected, 0);
            lv_obj_add_style(notif_item_panels[i], &style_item_normal, 0);
        }

        // Unread dot
        if (notifications[data_idx].unread) {
            lv_obj_clear_flag(notif_item_dot[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(notif_item_dot[i], LV_OBJ_FLAG_HIDDEN);
        }

        lv_label_set_text(notif_item_app_labels[i], notifications[data_idx].app_name);
        lv_label_set_text(notif_item_title_labels[i], notifications[data_idx].title);

        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", notifications[data_idx].hour, notifications[data_idx].minute);
        lv_label_set_text(notif_item_time_labels[i], time_buf);
    }
}

static void render_notification_detail(uint8_t index)
{
    if (index >= MAX_NOTIFICATIONS || !notifications[index].active) return;

    lv_label_set_text(detail_app_label, notifications[index].app_name);
    lv_label_set_text(detail_title_label, notifications[index].title);
    lv_label_set_text(detail_body_label, notifications[index].body);

    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", notifications[index].hour, notifications[index].minute);
    lv_label_set_text(detail_time_label, time_buf);

    // Update header
    lv_label_set_text(notif_header_label, LV_SYMBOL_LEFT " Detail");
}

/* ================================================================
 *  PUBLIC UI CONTROL
 * ================================================================ */

void show_notification_screen(void)
{
    init_styles();
    create_notification_list_ui();
    create_popup_ui();

    selected_index = 0;
    scroll_offset = 0;
    detail_view_active = false;

    lv_obj_add_flag(detail_container, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(notif_header_label, LV_SYMBOL_BELL " Notifications");

    render_notification_list();

    lv_obj_clear_flag(notif_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(notif_screen);
    notification_screen_active = true;

    ESP_LOGI(TAG, "Notification screen shown.");
}

void hide_notification_screen(void)
{
    if (notif_screen == NULL) return;

    lv_obj_add_flag(notif_screen, LV_OBJ_FLAG_HIDDEN);
    notification_screen_active = false;
    detail_view_active = false;

    ESP_LOGI(TAG, "Notification screen hidden.");
}

void notification_screen_scroll_up(void)
{
    if (!notification_screen_active || detail_view_active) return;
    if (notification_count == 0) return;

    if (selected_index > 0) {
        selected_index--;
        if (selected_index < scroll_offset) {
            scroll_offset = selected_index;
        }
        render_notification_list();
    }
}

void notification_screen_scroll_down(void)
{
    if (!notification_screen_active || detail_view_active) return;
    if (notification_count == 0) return;

    if (selected_index < notification_count - 1) {
        selected_index++;
        if (selected_index >= scroll_offset + VISIBLE_ITEMS) {
            scroll_offset = selected_index - VISIBLE_ITEMS + 1;
        }
        render_notification_list();
    }
}

void notification_show_detail(uint8_t index)
{
    if (index >= notification_count) return;

    mark_notification_read(index);
    detail_notif_index = index;
    detail_view_active = true;

    render_notification_detail(index);

    lv_obj_add_flag(notif_list_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notif_empty_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(detail_container, 0, LV_ANIM_OFF);
}

void notification_back_to_list(void)
{
    if (!detail_view_active) return;

    detail_view_active = false;
    detail_notif_index = -1;

    lv_obj_add_flag(detail_container, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(notif_header_label, LV_SYMBOL_BELL " Notifications");

    render_notification_list();
}

/* ================================================================
 *  POPUP (incoming notification toast)
 * ================================================================ */

static void popup_timer_cb(void *arg)
{
    hide_notification_popup();
}

void show_notification_popup(const char *app_name, const char *title)
{
    init_styles();
    create_popup_ui();

    lv_label_set_text(popup_app_label, app_name);
    lv_label_set_text(popup_title_label, title);

    lv_obj_clear_flag(popup_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(popup_container);

    // Auto-hide after 4 seconds
    if (popup_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = popup_timer_cb,
            .name = "notif_popup",
            .dispatch_method = ESP_TIMER_TASK,
        };
        esp_timer_create(&timer_args, &popup_timer);
    } else {
        esp_timer_stop(popup_timer);
    }
    esp_timer_start_once(popup_timer, 4 * 1000 * 1000); // 4 seconds
}

void hide_notification_popup(void)
{
    if (popup_container == NULL) return;
    lv_obj_add_flag(popup_container, LV_OBJ_FLAG_HIDDEN);
}

/* ================================================================
 *  BLE CALLBACK BRIDGE
 * ================================================================ */

void on_ble_notification_received(const char *app_name, const char *title, const char *body)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    add_notification(app_name, title, body, timeinfo.tm_hour, timeinfo.tm_min);

    // Show popup toast if on watch face (not on notification screen)
    if (!notification_screen_active) {
        show_notification_popup(app_name, title);
    } else {
        // Refresh the list if notification screen is active
        render_notification_list();
    }

    // Wake screen if off
    extern volatile bool screen_on;
    extern volatile uint32_t last_activity_time;
    last_activity_time = esp_log_timestamp();
    screen_on = true;
}