#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "pcf8563.h"
#include "pins.h"

void rtc_initialize(void);
void get_time_from_rtc(void);
esp_err_t set_time_on_rtc(void);
char *num_to_week(uint8_t weekday);