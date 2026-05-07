#ifndef MODBUS_LORA_H
#define MODBUS_LORA_H

#include <modbus.h>
#include "sx126x.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize a modbus_t struct with the LoRa backend
modbus_t* modbus_new_lora(sx126x_pkt_params_lora_t *pkt_params);

#ifdef __cplusplus
}
#endif

#endif
