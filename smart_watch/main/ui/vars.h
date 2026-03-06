#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_VAR_TIME_STR = 0,
    FLOW_GLOBAL_VARIABLE_VAR_AM_PM = 1,
    FLOW_GLOBAL_VARIABLE_VAR_BATTERY_PCT = 2,
    FLOW_GLOBAL_VARIABLE_VAR_DATE_STR = 3,
    FLOW_GLOBAL_VARIABLE_VAR_WEEK_DAY = 4
};

// Native global variables

extern const char *get_var_var_time_str();
extern void set_var_var_time_str(const char *value);
extern const char *get_var_var_am_pm();
extern void set_var_var_am_pm(const char *value);
extern int32_t get_var_var_battery_pct();
extern void set_var_var_battery_pct(int32_t value);
extern const char *get_var_var_date_str();
extern void set_var_var_date_str(const char *value);
extern const char *get_var_var_week_day();
extern void set_var_var_week_day(const char *value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/