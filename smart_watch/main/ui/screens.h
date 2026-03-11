#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    _SCREEN_ID_LAST = 1
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *time_lable;
    lv_obj_t *am_pm;
    lv_obj_t *top_bar;
    lv_obj_t *bluetooth_icon;
    lv_obj_t *obj0;
    lv_obj_t *battery_percentage_label;
    lv_obj_t *date_label;
    lv_obj_t *week_label;
    lv_obj_t *alarm_icon;
    lv_obj_t *alarm_container;
    lv_obj_t *alarm_icon2;
    lv_obj_t *obj1;
    lv_obj_t *alarm_time;
    lv_obj_t *message_box;
    lv_obj_t *snooze_btn;
    lv_obj_t *obj2;
    lv_obj_t *dimiss_btn;
    lv_obj_t *obj3;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/