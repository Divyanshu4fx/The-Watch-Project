cmake_minimum_required(VERSION 3.5)

set(MAIN_SRCS
   "smart_watch.c"
   "ui/images.c"
   "ui/screens.c"
   "ui/styles.c"
   "ui/ui.c"
   "ui/ui_image_battery_icon.c"
   "ui/ui_image_bluetooth_ico.c"
   "ui/ui_font_roboto.c"
#    "ui/vars.c"
)

set(MAIN_INCLUDE_DIRS
    "."
    "ui"
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