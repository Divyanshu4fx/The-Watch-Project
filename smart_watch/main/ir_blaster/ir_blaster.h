#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "ir_nec_encoder.h"
#include "pins.h"

#define IR_TX_PIN M5_IR_TX_PIN
#define IR_RESOLUTION_HZ 1000000 // 1MHz resolution, 1 tick = 1 microsecond

void init_ir_blaster(void);
esp_err_t execute_ir_command(uint32_t command);