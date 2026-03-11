#include "buttons.h"

static const char *TAG = "BUTTONS";

#define BUTTON_A_PIN M5_BUTTON_A_PIN // Big front button
#define BUTTON_B_PIN M5_BUTTON_B_PIN // Right side button
#define BUTTON_C_PIN M5_BUTTON_C_PIN // Left side

#define DEBOUNCE_MS 20
#define LONG_PRESS_MS 1000
#define BTN_MAX 3

extern volatile bool screen_on;
extern volatile uint32_t last_activity_time;
extern bool alarm_popup_enabled;

typedef enum
{
    BTN_EVENT_DOWN,
    BTN_EVENT_UP,
    BTN_EVENT_SHORT_CLICK,
    BTN_EVENT_LONG_CLICK
} button_event_t;

typedef enum
{
    BTN_ID_A = 0,
    BTN_ID_B,
    BTN_ID_C
} button_id_t;

typedef struct
{
    gpio_num_t pin;
    uint8_t active_level;
    bool current_state;
    bool previous_state;
    int64_t state_change_time;
    bool long_press_handled;
} button_t;

static button_t buttons[BTN_MAX] = {
    {BUTTON_A_PIN, 0, false, false, 0, false},
    {BUTTON_B_PIN, 0, false, false, 0, false},
    {BUTTON_C_PIN, 0, false, false, 0, false},
};

void input_event_callback(button_id_t btn_id, button_event_t);

static void input_manager_timer_cb(void *arg)
{
    int64_t now = esp_timer_get_time() / 1000;

    for (int i = 0; i < BTN_MAX; i++)
    {
        // Read physical pin state
        bool physical_pressed = (gpio_get_level(buttons[i].pin) == buttons[i].active_level);

        // Apply debounce filtering
        if (physical_pressed != buttons[i].current_state)
        {
            if (now - buttons[i].state_change_time >= DEBOUNCE_MS)
            {
                buttons[i].current_state = physical_pressed;
                buttons[i].state_change_time = now;

                if (buttons[i].current_state)
                {
                    buttons[i].long_press_handled = false;
                    input_event_callback(i, BTN_EVENT_DOWN);
                }
                else
                {
                    input_event_callback(i, BTN_EVENT_UP);

                    // Trigger short click if released before long press threshold
                    if (!buttons[i].long_press_handled)
                    {
                        input_event_callback(i, BTN_EVENT_SHORT_CLICK);
                    }
                }
            }
        }
        else if (buttons[i].current_state)
        {
            // Check for continuous hold triggering a long press
            if (!buttons[i].long_press_handled && (now - buttons[i].state_change_time >= LONG_PRESS_MS))
            {
                buttons[i].long_press_handled = true;
                input_event_callback(i, BTN_EVENT_LONG_CLICK);
            }
        }
    }
}

void input_manager_init(void)
{
    for (int i = 0; i < BTN_MAX; i++)
    {
        gpio_config_t btn_conf = {
            .intr_type = GPIO_INTR_DISABLE, // Polling used instead of interrupts
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << buttons[i].pin),
            .pull_down_en = 0,
            .pull_up_en = 1 // Enable internal pull-up
        };
        gpio_config(&btn_conf);
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &input_manager_timer_cb,
        .name = "input_manager_timer"};

    esp_timer_handle_t input_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &input_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(input_timer, 10 * 1000)); // 10ms polling
}

void input_event_callback(button_id_t btn_id, button_event_t event)
{
    // Global wake-up on any button down
    if (event == BTN_EVENT_DOWN)
    {
        last_activity_time = esp_log_timestamp();
        screen_on = true;
    }

    switch (btn_id)
    {
    case BTN_ID_A: // Middle/Main Button
        if (event == BTN_EVENT_SHORT_CLICK)
        {
            ESP_LOGI(TAG, "Button A Short Pressed");
            buzzer_off();
            if (find_phone_screen_active)
            {
                toggle_find_phone_ring();
            }
            else if (notification_screen_active)
            {
                if (detail_view_active)
                {
                    notification_back_to_list();
                }
                else
                {
                    // Open selected notification detail
                    notification_show_detail(selected_index);
                }
            }
        }
        else if (event == BTN_EVENT_LONG_CLICK)
        {
            ESP_LOGI(TAG, "Button A Long Pressed");
            dismiss_alarm();
            // Insert explicit device sleep or sync logic here
            if (!(alarm_popup_enabled))
            {
                if (find_phone_screen_active)
                {
                    hide_find_phone_screen();
                }
                else if (notification_screen_active)
                {
                    hide_notification_screen();
                }
                else
                {
                    show_find_phone_screen();
                    // show_notification_screen();
                }
            }
        }
        break;

    case BTN_ID_B: // Right Side Button
        if (event == BTN_EVENT_SHORT_CLICK)
        {
            ESP_LOGI(TAG, "Button B Short Clicked");
            snooze_alarm(5); // snooze for 5 minutes
                             // Navigate UI forward
            if (!(alarm_popup_enabled))
            {
                if (notification_screen_active)
                {
                    if (detail_view_active)
                    {
                        notification_back_to_list();
                    }
                    else
                    {
                        notification_screen_scroll_down();
                    }
                }
            }
        }
        break;

    case BTN_ID_C: // Left Side / Power Button
        if (event == BTN_EVENT_SHORT_CLICK)
        {
            ESP_LOGI(TAG, "Button C Short Clicked");
            // Navigate UI backward
            if (!(alarm_popup_enabled))
            {
                if (notification_screen_active)
                {
                    if (detail_view_active)
                    {
                        notification_back_to_list();
                    }
                    else
                    {
                        notification_screen_scroll_up();
                    }
                }
            }
        }
        break;

    default:
        break;
    }
}