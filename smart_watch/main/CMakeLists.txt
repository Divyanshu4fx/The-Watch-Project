cmake_minimum_required(VERSION 3.5)

set(MAIN_SRCS
   "smart_watch.c"
   "ui/images.c"
   "ui/screens.c"
   "ui/styles.c"
   "ui/ui.c"
   "ui/ui_image_battery_icon.c"
   "ui/ui_image_bluetooth_ico.c"
   "ui/ui_image_alarm_icon.c"
   "ui/ui_font_roboto.c"
   "battery/battery.c"
   "ble/ble.c"
   "clock/clock.c"
   "input_mgr/buttons.c"
   "notification/notification.c"
#    "ui/vars.c"
)

set(MAIN_INCLUDE_DIRS
    "."
    "ui"
    "ble"
    "clock"
    "battery"
    "display"
    "input_mgr"
    "notification"
)

set(MAIN_REQUIRES
    driver
    esp_lcd
    esp_adc
    bt
    nvs_flash
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
   
)