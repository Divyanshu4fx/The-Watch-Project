#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include "buttons.h"
#include <string.h>
#include <time.h>

#define MAX_NOTIFICATIONS 10
#define NOTIF_TITLE_LEN 32
#define NOTIF_BODY_LEN 128
#define NOTIF_APP_LEN 24

typedef struct {
    char app_name[NOTIF_APP_LEN];
    char title[NOTIF_TITLE_LEN];
    char body[NOTIF_BODY_LEN];
    uint8_t hour;
    uint8_t minute;
    bool unread;
    bool active;
} notification_t;

// Notification storage
extern notification_t notifications[MAX_NOTIFICATIONS];
extern volatile uint8_t notification_count;
extern volatile uint8_t unread_count;
extern volatile bool notification_screen_active;
extern bool detail_view_active;
extern int8_t selected_index;

// Core functions
void notifications_init(void);
int add_notification(const char *app_name, const char *title, const char *body, uint8_t hour, uint8_t minute);
void remove_notification(uint8_t index);
void clear_all_notifications(void);
void mark_notification_read(uint8_t index);

// UI functions
void show_notification_screen(void);
void hide_notification_screen(void);
void notification_screen_scroll_up(void);
void notification_screen_scroll_down(void);
void notification_show_detail(uint8_t index);
void notification_back_to_list(void);
void show_notification_popup(const char *app_name, const char *title);
void hide_notification_popup(void);

// Called from BLE when a new notification arrives
void on_ble_notification_received(const char *app_name, const char *title, const char *body);

#endif // NOTIFICATIONS_H