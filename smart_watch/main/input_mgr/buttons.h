#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pins.h"
#include "clock.h"
#include "notification.h"
#include "find_phone.h"

#ifndef BUTTONS_H
#define BUTTONS_H


typedef enum {
    SCREEN_WATCHFACE,
    SCREEN_FIND_PHONE,
    SCREEN_NOTIFICATIONS,
    SCREEN_MAX // Helper to know the total count
} app_screen_t;

static app_screen_t current_app_screen = SCREEN_WATCHFACE;

void input_manager_init(void);

#endif