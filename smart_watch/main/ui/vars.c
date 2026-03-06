#include "vars.h"

const char *get_var_var_time_str();
void set_var_var_time_str(const char *value);
const char *get_var_var_am_pm();
void set_var_var_am_pm(const char *value);
int32_t get_var_var_battery_pct();
void set_var_var_battery_pct(int32_t value);
const char *get_var_var_date_str();
void set_var_var_date_str(const char *value);
const char *get_var_var_week_day();
void set_var_var_week_day(const char *value);


static const char *var_time_str = "12:00:00";
static const char *var_am_pm = "AM";
static int32_t var_battery_pct = 99;
static const char *var_date_str = "";
static const char *var_week_day = "Saturday";

const char *get_var_var_time_str()
{
    return var_time_str ? var_time_str : "";
}

void set_var_var_time_str(const char *value)
{
    var_time_str = value ? value : "";
}

const char *get_var_var_am_pm()
{
    return var_am_pm ? var_am_pm : "";
}

void set_var_var_am_pm(const char *value)
{
    var_am_pm = value ? value : "";
}

int32_t get_var_var_battery_pct()
{
    return var_battery_pct;
}

void set_var_var_battery_pct(int32_t value)
{
    var_battery_pct = value;
}

const char *get_var_var_date_str()
{
    return var_date_str ? var_date_str : "";
}

void set_var_var_date_str(const char *value)
{
    var_date_str = value ? value : "";
}

const char *get_var_var_week_day()
{
    return var_week_day ? var_week_day : "";
}

void set_var_var_week_day(const char *value)
{
    var_week_day = value ? value : "";
}