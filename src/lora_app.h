#ifndef LORA_APP_H
#define LORA_APP_H

#include "sx126x.h"
#include <cstdint>

int run_lora_app();

// Core LoRa radio operations
bool transmit(const uint8_t *payload, uint8_t size, sx126x_pkt_params_lora_t *pkt_params);
bool initialize_receiver(sx126x_mod_params_lora_t *mod_params, sx126x_pkt_params_lora_t *pkt_params);

#endif // LORA_APP_H
