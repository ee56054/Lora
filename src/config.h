#ifndef CONFIG_H
#define CONFIG_H

#include "sx126x.h"
#include <cstdint>
#include <vector>
#include <nlohmann/json.hpp>
#include <string>


struct LoraConfig {
  uint32_t frequency = 915000000;
  int8_t tx_power = 22;
  sx126x_lora_sf_t spreading_factor = SX126X_LORA_SF9;
  sx126x_lora_bw_t bandwidth = SX126X_LORA_BW_125;
  sx126x_lora_cr_t coding_rate = SX126X_LORA_CR_4_6;
  uint16_t preamble_length = 8;
  uint32_t rx_timeout = 5000;

  bool modbus_enabled = true;
  int modbus_slave_id = 1;
  std::vector<int> modbus_address_devices = {1};

  // LoRa Security (AES-128-CTR + CRC16 MIC, compatible with STM32)
  bool security_enabled = true;
  std::string aes_key = "2B7E151628AED2A6ABF7158809CF4F3C";

  bool load_from_file(const std::string &filepath);
  bool save_to_file(const std::string &filepath) const;
};

// Global config instance
extern LoraConfig g_config;

#endif // CONFIG_H
