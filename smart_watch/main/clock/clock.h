#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "pcf8563.h"
#include "pins.h"

#ifndef CLOCK_H
#define CLOCK_H


#define MAX_ALARMS 5
#define MAX_MSG_LEN 32
#define ALARM_NVS_NAMESPACE "alarms"

typedef struct {
    uint8_t index;
    uint8_t hour;
    uint8_t minute;
    uint8_t enabled;
    uint8_t message[MAX_MSG_LEN];
} myalarm_t;

typedef enum {
    ALARM_STATE_IDLE,
    ALARM_STATE_RINGING,
    ALARM_STATE_SNOOZED,
} alarm_state_t;

// extern declarations for shared variables
extern myalarm_t alarms[MAX_ALARMS];
extern uint16_t alarm_notify_handle;
extern alarm_state_t current_alarm_state;
extern int8_t ringing_alarm_index;
extern bool is_any_alarm_enabled;


void rtc_initialize(void);
void get_time_from_rtc(void);
esp_err_t set_time_on_rtc(void);
char *num_to_week(uint8_t weekday);
void buzzer_init();
void buzzer_on();
void buzzer_off();
void alarm_update();
void alarm_stop();

// NVS alarm persistence
esp_err_t save_alarm_to_nvs(uint8_t index);
esp_err_t load_alarms_from_nvs(void);
esp_err_t delete_alarm_from_nvs(uint8_t index);

// Alarm checking (call this every second from your main loop)
void check_alarms(void);
void dismiss_alarm(void);
void snooze_alarm(uint8_t snooze_minutes);


#endif // CLOCK_H