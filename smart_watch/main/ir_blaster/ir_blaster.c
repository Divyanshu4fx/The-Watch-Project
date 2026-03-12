#include "ir_blaster.h"

rmt_channel_handle_t ir_tx_channel = NULL;
rmt_encoder_handle_t nec_encoder;

void init_ir_blaster(void)
{
    ESP_LOGI("IR", "Initializing RMT TX channel");
    rmt_tx_channel_config_t tx_channel_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = IR_TX_PIN,
        .mem_block_symbols = 64, // Amount of memory for RMT
        .resolution_hz = IR_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &ir_tx_channel));

    // IR receivers require a 38kHz carrier wave. The RMT hardware applies this for us.
    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.33,    // Standard duty cycle for IR LEDs
        .frequency_hz = 38000, // 38 kHz standard carrier frequency
        .flags.polarity_active_low = false,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(ir_tx_channel, &carrier_cfg));

    ESP_ERROR_CHECK(rmt_enable(ir_tx_channel));

    // Initialize the NEC encoder to translate your 32-bit hex into RMT pulses
    ESP_LOGI("IR", "Initializing NEC encoder");
    ir_nec_encoder_config_t nec_encoder_cfg = {
        .resolution = IR_RESOLUTION_HZ, // Ensure this matches your channel resolution (e.g., 1000000)
    };
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &nec_encoder));
    // ----------------------
}

esp_err_t execute_ir_command(uint32_t command)
{
    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };

    // Assuming standard 32-bit NEC payload format (Address + Command)
    uint32_t nec_payload = (uint32_t)command;

    esp_err_t err = rmt_transmit(ir_tx_channel, nec_encoder, &nec_payload, sizeof(nec_payload), &transmit_config);

    rmt_tx_wait_all_done(ir_tx_channel, -1);

    return err;
}