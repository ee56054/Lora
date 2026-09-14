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

// --- Modbus Write Functions ---

// Write a single coil (FC 05) - state: true (1/ON) or false (0/OFF)
bool modbus_write_coil(int slave_id, int address, bool state);

// Write a single holding register (FC 06)
bool modbus_write_holding_register(int slave_id, int address, uint16_t value);

// Write multiple coils (FC 15)
bool modbus_write_multiple_coils(int slave_id, int address, int count, const uint8_t *values);

// Write multiple holding registers (FC 16)
bool modbus_write_multiple_holding_registers(int slave_id, int address, int count, const uint16_t *values);

// Convenience: control Valve 1 or Valve 2 via FC 05 (valve_index: 1 or 2, open: true/false)
bool modbus_write_valve(int slave_id, int valve_index, bool open);

// Convenience: configure remote Slave ID via Holding Register 8 (FC 06)
bool modbus_write_remote_slave_id(int current_slave_id, int new_slave_id);

// --- Modbus Read Functions & Telemetry ---

struct ModbusTelemetry {
  int slave_id = 1;
  int valve1 = -1;         // Coil 0 (-1: unknown, 0: OFF, 1: ON)
  int valve2 = -1;         // Coil 1
  int valve1_status = -1;  // Discrete Input 0
  int valve2_status = -1;  // Discrete Input 1
  int sensor1 = -1;        // Input Reg 0
  int sensor2 = -1;        // Input Reg 1
  int hw_id_high = -1;     // Holding Reg 0
  int hw_id_low = -1;      // Holding Reg 1
  int tx_count_high = -1;  // Holding Reg 2
  int tx_count_low = -1;   // Holding Reg 3
  int valve1_reg = -1;     // Holding Reg 4
  int valve2_reg = -1;     // Holding Reg 5
  int sensor1_reg = -1;    // Holding Reg 6
  int sensor2_reg = -1;    // Holding Reg 7
  int slave_id_reg = -1;   // Holding Reg 8
  std::string last_update = "Never";
};

// Read a single coil (FC 01)
bool modbus_read_coil_val(int slave_id, int address, bool &out_val);

// Read a single discrete input (FC 02)
bool modbus_read_discrete_input_val(int slave_id, int address, bool &out_val);

// Read a single input register (FC 04)
bool modbus_read_input_reg_val(int slave_id, int address, uint16_t &out_val);

// Read a single holding register (FC 03)
bool modbus_read_holding_reg_val(int slave_id, int address, uint16_t &out_val);

// Read all mapped telemetry for a slave
bool modbus_read_all(int slave_id);

// Retrieve latest telemetry cache
ModbusTelemetry get_modbus_telemetry();

#endif // MODBUS_APP_H

