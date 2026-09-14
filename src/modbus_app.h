#ifndef MODBUS_APP_H
#define MODBUS_APP_H

#include "sx126x.h"
#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>

// Message representation for packets queued from LoRa receiver
struct RxMessage {
    std::vector<uint8_t> data;
};

// Queue and mutex shared with modbus_lora backend
extern std::queue<RxMessage> message_queue;
extern std::mutex queue_mutex;

// Initialize Modbus context and connect backend
bool init_modbus(sx126x_pkt_params_lora_t *pkt_params);

// Check if Modbus context is active and ready
bool is_modbus_ready();

// Enqueue an incoming packet from the DIO1 interrupt handler
void modbus_queue_rx_packet(const uint8_t *payload, uint8_t payload_len);

// Main Modbus polling execution loop
void run_modbus_loop(sx126x_mod_params_lora_t *mod_params, sx126x_pkt_params_lora_t *pkt_params);

// Clean up and close Modbus context
void cleanup_modbus();

// Calculate Modbus RTU CRC-16
uint16_t modbus_crc16(const uint8_t *buffer, uint16_t buffer_length);

// Decode and display received Modbus frame details and values
void modbus_print_packet_value(const uint8_t *payload, uint8_t payload_len);

#endif // MODBUS_APP_H

