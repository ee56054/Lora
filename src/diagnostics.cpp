#include "diagnostics.h"
#include "hal.h"
#include "sx126x.h"
#include "sx126x_hal.h"
#include "config.h"
#include <iostream>
#include <string>
#include <unistd.h>

// LoRa configuration constants
#define FREQUENCY g_config.frequency
#define TX_POWER g_config.tx_power
#define SPREADING_FACTOR g_config.spreading_factor
#define BANDWIDTH g_config.bandwidth
#define CODING_RATE g_config.coding_rate
#define PREAMBLE_LENGTH g_config.preamble_length
#define RX_TIMEOUT g_config.rx_timeout

// Function to display all configuration settings
void show_configuration() {
  std::cout << "\n==========================================" << std::endl;
  std::cout << "           LoRa Configuration" << std::endl;
  std::cout << "==========================================" << std::endl;

  // LoRa Radio Parameters
  std::cout << "\n--- LoRa Radio Parameters ---" << std::endl;
  std::cout << "Frequency: " << (FREQUENCY / 1000000.0) << " MHz (" << FREQUENCY
            << " Hz)" << std::endl;
  std::cout << "TX Power: +" << (int)TX_POWER << " dBm" << std::endl;

  // Modulation Parameters
  std::cout << "\n--- Modulation Parameters ---" << std::endl;
  std::cout << "Spreading Factor: ";
  switch (SPREADING_FACTOR) {
  case SX126X_LORA_SF5:
    std::cout << "SF5";
    break;
  case SX126X_LORA_SF6:
    std::cout << "SF6";
    break;
  case SX126X_LORA_SF7:
    std::cout << "SF7";
    break;
  case SX126X_LORA_SF8:
    std::cout << "SF8";
    break;
  case SX126X_LORA_SF9:
    std::cout << "SF9";
    break;
  case SX126X_LORA_SF10:
    std::cout << "SF10";
    break;
  case SX126X_LORA_SF11:
    std::cout << "SF11";
    break;
  case SX126X_LORA_SF12:
    std::cout << "SF12";
    break;
  default:
    std::cout << "Unknown";
    break;
  }
  std::cout << std::endl;

  std::cout << "Bandwidth: ";
  switch (BANDWIDTH) {
  case SX126X_LORA_BW_007:
    std::cout << "7.81 kHz";
    break;
  case SX126X_LORA_BW_010:
    std::cout << "10.42 kHz";
    break;
  case SX126X_LORA_BW_015:
    std::cout << "15.63 kHz";
    break;
  case SX126X_LORA_BW_020:
    std::cout << "20.83 kHz";
    break;
  case SX126X_LORA_BW_031:
    std::cout << "31.25 kHz";
    break;
  case SX126X_LORA_BW_041:
    std::cout << "41.67 kHz";
    break;
  case SX126X_LORA_BW_062:
    std::cout << "62.5 kHz";
    break;
  case SX126X_LORA_BW_125:
    std::cout << "125 kHz";
    break;
  case SX126X_LORA_BW_250:
    std::cout << "250 kHz";
    break;
  case SX126X_LORA_BW_500:
    std::cout << "500 kHz";
    break;
  default:
    std::cout << "Unknown";
    break;
  }
  std::cout << std::endl;

  std::cout << "Coding Rate: ";
  switch (CODING_RATE) {
  case SX126X_LORA_CR_4_5:
    std::cout << "4/5";
    break;
  case SX126X_LORA_CR_4_6:
    std::cout << "4/6";
    break;
  case SX126X_LORA_CR_4_7:
    std::cout << "4/7";
    break;
  case SX126X_LORA_CR_4_8:
    std::cout << "4/8";
    break;
  default:
    std::cout << "Unknown";
    break;
  }
  std::cout << std::endl;

  // Packet Parameters
  std::cout << "\n--- Packet Parameters ---" << std::endl;
  std::cout << "Preamble Length: " << PREAMBLE_LENGTH << " symbols"
            << std::endl;
  std::cout << "Header Type: Explicit" << std::endl;
  std::cout << "Payload Length: 15 bytes" << std::endl;
  std::cout << "CRC: Enabled" << std::endl;
  std::cout << "IQ Inversion: Disabled" << std::endl;
  std::cout << "Sync Word: 0x3444 (Public LoRa Network)" << std::endl;

  // SPI Configuration
  std::cout << "\n--- SPI Configuration ---" << std::endl;
  std::cout << "Device: " << SPI_DEVICE << std::endl;
  std::cout << "Speed: " << (SPI_SPEED / 1000.0) << " kHz" << std::endl;
  std::cout << "Bits per Word: " << SPI_BITS_PER_WORD << std::endl;
  std::cout << "Mode: SPI_MODE_0" << std::endl;

  // GPIO Pin Configuration
  std::cout << "\n--- GPIO Pin Configuration ---" << std::endl;
  std::cout << "RESET_PIN: GPIO " << RESET_PIN << std::endl;
  std::cout << "DIO4_PIN: GPIO " << DIO4_PIN << " (TX Enable)" << std::endl;
  std::cout << "DIO1_PIN: GPIO " << DIO1_PIN << " (Interrupt)" << std::endl;
  std::cout << "BUSY_PIN: GPIO " << BUSY_PIN << " (Busy Status)" << std::endl;
  std::cout << "CS_PIN: GPIO " << CS_PIN << " (Chip Select)" << std::endl;

  // Timing Configuration
  std::cout << "\n--- Timing Configuration ---" << std::endl;
  std::cout << "RX Timeout: " << RX_TIMEOUT << " ms" << std::endl;
  std::cout << "TX Delay: 5 seconds between transmissions" << std::endl;

  std::cout << "==========================================\n" << std::endl;
}

// Function to read and display actual configuration from the SX126X chip
void read_chip_configuration() {
  std::cout << "\n==========================================" << std::endl;
  std::cout << "    SX126X Chip Configuration (Live Read)" << std::endl;
  std::cout << "==========================================\n" << std::endl;

  // 1. Chip Status
  sx126x_chip_status_t chip_status;
  if (sx126x_get_status(NULL, &chip_status) == SX126X_STATUS_OK) {
    std::cout << "--- Chip Status ---" << std::endl;
    std::cout << "Command Status: " << (int)chip_status.cmd_status << std::endl;
    std::cout << "Chip Mode: " << (int)chip_status.chip_mode << std::endl;
  }

  // 2. Device Errors
  sx126x_errors_mask_t errors;
  if (sx126x_get_device_errors(NULL, &errors) == SX126X_STATUS_OK) {
    std::cout << "\n--- Device Errors ---" << std::endl;
    std::cout << "Error Mask: 0x" << std::hex << (int)errors << std::dec
              << std::endl;
    if (errors == 0)
      std::cout << "No Errors" << std::endl;
  }

  // 3. IRQ Status
  sx126x_irq_mask_t irq_mask;
  if (sx126x_get_irq_status(NULL, &irq_mask) == SX126X_STATUS_OK) {
    std::cout << "\n--- Interrupt Status ---" << std::endl;
    std::cout << "IRQ Mask: 0x" << std::hex << (int)irq_mask << std::dec
              << std::endl;
  }

  // 4. Instantaneous RSSI
  int16_t rssi_inst;
  if (sx126x_get_rssi_inst(NULL, &rssi_inst) == SX126X_STATUS_OK) {
    std::cout << "\n--- Radio State ---" << std::endl;
    std::cout << "Instantaneous RSSI: " << rssi_inst << " dBm" << std::endl;
  }

  // 5. RX Buffer Status
  sx126x_rx_buffer_status_t rx_buffer;
  if (sx126x_get_rx_buffer_status(NULL, &rx_buffer) == SX126X_STATUS_OK) {
    std::cout << "\n--- RX Buffer Status ---" << std::endl;
    std::cout << "RX Payload Length: " << (int)rx_buffer.pld_len_in_bytes
              << " bytes" << std::endl;
    std::cout << "RX Buffer Pointer: 0x" << std::hex
              << (int)rx_buffer.buffer_start_pointer << std::dec << std::endl;
  }

  std::cout << "\n--- Internal Memory Registers ---" << std::endl;

  // 6. RF Frequency (0x088B)
  uint8_t freq_bytes[4] = {0};
  if (sx126x_read_register(NULL, 0x088B, freq_bytes, 4) == SX126X_STATUS_OK) {
    uint32_t freq_reg =
        ((uint32_t)freq_bytes[0] << 24) | ((uint32_t)freq_bytes[1] << 16) |
        ((uint32_t)freq_bytes[2] << 8) | (uint32_t)freq_bytes[3];
    double freq_mhz = (freq_reg * 32000000.0) / (1UL << 25) / 1000000.0;
    std::cout << "RF Frequency: " << freq_mhz << " MHz" << std::endl;
  }

  // 7. LoRa Sync Word (0x0740)
  uint8_t sync_word[2] = {0};
  if (sx126x_read_register(NULL, 0x0740, sync_word, 2) == SX126X_STATUS_OK) {
    std::cout << "LoRa Sync Word: 0x" << std::hex << (int)sync_word[0]
              << (int)sync_word[1] << std::dec;
    if (sync_word[0] == 0x34 && sync_word[1] == 0x44)
      std::cout << " (Public LoRaWAN)";
    else if (sync_word[0] == 0x14 && sync_word[1] == 0x24)
      std::cout << " (Private Network)";
    std::cout << std::endl;
  }

  // 8. OCP (0x08E7)
  uint8_t ocp = 0;
  if (sx126x_read_register(NULL, 0x08E7, &ocp, 1) == SX126X_STATUS_OK) {
    std::cout << "OCP Config: 0x" << std::hex << (int)ocp << std::dec;
    if (ocp == 0x18)
      std::cout << " (60mA - Default)";
    else if (ocp == 0x38)
      std::cout << " (140mA)";
    std::cout << std::endl;
  }

  // 9. IQ Polarity Setup (0x0736)
  uint8_t iq_setup = 0;
  if (sx126x_read_register(NULL, 0x0736, &iq_setup, 1) == SX126X_STATUS_OK) {
    std::cout << "IQ Polarity: 0x" << std::hex << (int)iq_setup << std::dec;
    if (iq_setup == 0x0D)
      std::cout << " (Standard)";
    else if (iq_setup == 0x09)
      std::cout << " (Inverted)";
    std::cout << std::endl;
  }

  std::cout << "\n==========================================\n" << std::endl;
}

// Function to test SPI communication
void test_spi_communication() {
  std::cout << "\n==========================================" << std::endl;
  std::cout << "    SPI Communication Test" << std::endl;
  std::cout << "==========================================\n" << std::endl;

  // Test 1: Check if device file is accessible
  std::cout << "Test 1: SPI Device Accessibility" << std::endl;
  if (access(SPI_DEVICE, F_OK) == 0) {
    std::cout << "  ✓ SPI device " << SPI_DEVICE << " exists" << std::endl;
  } else {
    std::cout << "  ✗ SPI device " << SPI_DEVICE << " NOT FOUND" << std::endl;
  }

  // Test 2: Check GPIO pins
  std::cout << "\nTest 2: GPIO Pin Status" << std::endl;
  std::cout << "  RESET_PIN (GPIO " << RESET_PIN
            << "):  " << (gpio_get_value(RESET_PIN) ? "HIGH" : "LOW")
            << std::endl;
  std::cout << "  BUSY_PIN (GPIO " << BUSY_PIN
            << "):   " << (gpio_get_value(BUSY_PIN) ? "HIGH" : "LOW")
            << std::endl;
  std::cout << "  DIO1_PIN (GPIO " << DIO1_PIN
            << "):   " << (gpio_get_value(DIO1_PIN) ? "HIGH" : "LOW")
            << std::endl;
  std::cout << "  CS_PIN (GPIO " << CS_PIN
            << "):     " << (gpio_get_value(CS_PIN) ? "HIGH" : "LOW")
            << std::endl;

  // Test 3: Simple NOOP command to check SPI
  std::cout << "\nTest 3: NOOP SPI Command (command 0x00)" << std::endl;
  uint8_t noop_cmd = 0x00; // NOOP command for SX126X
  uint8_t response = 0;

  // Set CS low before transfer
  gpio_set_value(CS_PIN, 0);
  usleep(10);

  bool spi_ok = spi_transfer(&noop_cmd, &response, 1);

  // CS already managed by spi_transfer
  if (spi_ok) {
    std::cout << "  ✓ SPI transfer OK" << std::endl;
    std::cout << "  Response byte: 0x" << std::hex << (int)response << std::dec
              << std::endl;
    if (response == 0xFF || response == 0x00) {
      std::cout << "  WARNING: Response is all 0s or 1s - possible floating bus"
                << std::endl;
    }
  } else {
    std::cout << "  ✗ SPI transfer FAILED" << std::endl;
  }

  // Test 4: Check chip status via direct register read
  std::cout << "\nTest 4: Chip Status via HAL (GetStatus command)" << std::endl;
  sx126x_chip_status_t chip_status;
  sx126x_status_t status = sx126x_get_status(NULL, &chip_status);
  if (status == SX126X_STATUS_OK) {
    std::cout << "  ✓ Get status OK" << std::endl;
    std::cout << "  Command status: " << (int)chip_status.cmd_status
              << std::endl;
    std::cout << "  Chip mode: " << (int)chip_status.chip_mode << std::endl;
    if (chip_status.chip_mode == 0xFF) {
      std::cout << "  ERROR: Chip mode is 0xFF - Chip not responding!"
                << std::endl;
    }
  } else {
    std::cout << "  ✗ Get status FAILED (status: " << (int)status << ")"
              << std::endl;
  }

  // Test 5: Verify CS control
  std::cout << "\nTest 5: Chip Select (CS) Control Verification" << std::endl;
  std::cout << "  Setting CS LOW..." << std::endl;
  gpio_set_value(CS_PIN, 0);
  usleep(10);
  int cs_state_low = gpio_get_value(CS_PIN);
  std::cout << "  CS state: " << (cs_state_low ? "HIGH" : "LOW") << std::endl;
  if (cs_state_low != 0) {
    std::cout << "  ✗ ERROR: CS not going LOW!" << std::endl;
  }

  std::cout << "  Setting CS HIGH..." << std::endl;
  gpio_set_value(CS_PIN, 1);
  usleep(10);
  int cs_state_high = gpio_get_value(CS_PIN);
  std::cout << "  CS state: " << (cs_state_high ? "HIGH" : "LOW") << std::endl;
  if (cs_state_high != 1) {
    std::cout << "  ✗ ERROR: CS not going HIGH!" << std::endl;
  }

  std::cout << "\n==========================================\n" << std::endl;
}

// Function to read and display all SX126X registers
// Register value formatting functions - convert raw values to human-readable
// format
std::string format_operating_mode(uint8_t value) {
  std::string modes[] = {"Sleep",
                         "Standby RC",
                         "Standby XOSC",
                         "FS",
                         "RX",
                         "TX",
                         "Channel Activity Detection"};
  int mode = value & 0x07;
  if (mode < 7)
    return modes[mode];
  return "Unknown";
}

std::string format_tx_config(uint8_t value) {
  std::string result = "TX Config: ";
  if (value & 0x01)
    result += "RampTime[3:0]=" + std::to_string((value >> 1) & 0x0F);
  return result;
}

std::string format_modulation_sf_bw(uint8_t value) {
  std::string result = "SF/BW: SF=";
  int sf = (value >> 4) & 0x0F;
  int bw = value & 0x0F;
  result += std::to_string(5 + sf);
  result += " BW=";
  const char *bw_names[] = {"7.81k", "10.4k", "15.6k", "20.8k", "31.2k",
                            "41.7k", "62.5k", "125k",  "250k",  "500k"};
  if (bw < 10)
    result += bw_names[bw];
  else
    result += "Unknown";
  return result;
}

std::string format_modulation_cr_ldro(uint8_t value) {
  std::string result = "CR/LDRO: CR=4/";
  int cr = (value >> 1) & 0x07;
  int ldro = value & 0x01;
  result += std::to_string(5 + cr);
  result += " LDRO=" + std::string(ldro ? "ON" : "OFF");
  return result;
}

std::string format_packet_params_3(uint8_t value) {
  std::string result = "Header=";
  result += (value & 0x01) ? "Explicit" : "Implicit";
  result += " CRC=" + std::string((value & 0x04) ? "ON" : "OFF");
  result += " IQ_Invert=" + std::string((value & 0x08) ? "ON" : "OFF");
  return result;
}

std::string format_irq_status(uint16_t value) {
  std::string result = "";
  if (value & 0x0001)
    result += "TX_DONE ";
  if (value & 0x0002)
    result += "RX_DONE ";
  if (value & 0x0004)
    result += "PREAMBLE_DETECTED ";
  if (value & 0x0008)
    result += "SYNCWORD_VALID ";
  if (value & 0x0010)
    result += "HEADER_VALID ";
  if (value & 0x0020)
    result += "HEADER_ERROR ";
  if (value & 0x0040)
    result += "CRC_ERROR ";
  if (value & 0x0080)
    result += "CAD_DONE ";
  if (value & 0x0100)
    result += "CAD_DETECTED ";
  if (value & 0x0200)
    result += "TIMEOUT ";
  if (result.empty())
    result = "None";
  return result;
}

// Convert RF frequency register bytes to MHz
// SX126X stores frequency as: Freq(Hz) = Register_Value * (Fxosc / 2^25)
// Where Fxosc = 32 MHz, so: Freq(MHz) = (Register_Value * 32) / 2^25
double convert_rf_frequency_to_mhz(uint8_t msb, uint8_t byte2, uint8_t byte3,
                                   uint8_t lsb) {
  uint32_t freq_reg = ((uint32_t)msb << 24) | ((uint32_t)byte2 << 16) |
                      ((uint32_t)byte3 << 8) | (uint32_t)lsb;
  // SX126X Frequency formula:
  // Frequency (Hz) = (freq_reg * 32000000) / 2^25
  // Frequency (MHz) = (freq_reg * 32000000) / 2^25 / 1000000
  // = (freq_reg * 32) / 33554432 / 1000
  double freq_hz = (freq_reg * 32000000.0) / (1UL << 25);
  return freq_hz / 1000000.0; // Return in MHz
}

void read_all_registers() {
  sx126x_status_t status;
  uint8_t reg_value;

  std::cout << "\n==========================================" << std::endl;
  std::cout << "    SX126X Register Dump (Human Readable)" << std::endl;
  std::cout << "==========================================\n" << std::endl;

  // First run SPI diagnostics
  test_spi_communication();

  // Important SX126X register addresses
  const struct {
    uint16_t addr;
    const char *name;
  } registers[] = {
      // SX1261/2 Datasheet - Official Register Map (Section 13)

      // GFSK Related Registers
      {0x06B8, "GFSK Node Address"},
      {0x06B9, "GFSK Broadcast Address"},
      {0x06C0, "GFSK Sync Word 0"},
      {0x06C1, "GFSK Sync Word 1"},
      {0x06C2, "GFSK Sync Word 2"},
      {0x06C3, "GFSK Sync Word 3"},
      {0x06C4, "GFSK Sync Word 4"},
      {0x06C5, "GFSK Sync Word 5"},
      {0x06C6, "GFSK Sync Word 6"},
      {0x06C7, "GFSK Sync Word 7"},

      // LoRa Related Registers
      {0x0736, "IQ Polarity Setup"},
      {0x0740, "LoRa Sync Word (MSB)"},
      {0x0741, "LoRa Sync Word (LSB)"},

      // Frequency and Hardware Registers
      {0x088B, "RF Frequency (MSB)"},
      {0x088C, "RF Frequency (Mid-High)"},
      {0x088D, "RF Frequency (Mid-Low)"},
      {0x088E, "RF Frequency (LSB)"},
      {0x08E7, "Over Current Protection (OCP)"},
      {0x08E9, "XTA Trim (Crystal oscillator)"},
      {0x08F0, "TX Modulation Setup (Errata)"},
  };

  std::cout << "--- Register Values ---\n" << std::endl;
  std::cout << "Note: All 0xFF values suggest SPI communication issue!\n"
            << std::endl;

  int num_registers = sizeof(registers) / sizeof(registers[0]);
  int read_failures = 0, all_ff_count = 0;

  // Read frequency registers separately for special handling
  uint8_t freq_bytes[4] = {0};
  bool freq_valid = true;
  for (int i = 0; i < 4; i++) {
    status = sx126x_read_register(NULL, 0x088B + i, &freq_bytes[i], 1);
    if (status != SX126X_STATUS_OK)
      freq_valid = false;
  }

  for (int i = 0; i < num_registers; i++) {
    // Skip frequency registers if we already read them
    if (registers[i].addr >= 0x088B && registers[i].addr <= 0x088E) {
      // Display frequency as combined MHz value
      // printf("0x%04X %-40s : %04X ", registers[i].addr, "RF Frequency
      // (32-bit)  \n",
      //         freq_bytes );
      if (registers[i].addr == 0x088B) {
        if (freq_valid && freq_bytes[0] != 0xFF) {
          double freq_mhz = convert_rf_frequency_to_mhz(
              freq_bytes[0], freq_bytes[1], freq_bytes[2], freq_bytes[3]);
          printf("0x088B-0x088E %-34s : 0x%02X%02X%02X%02X (%u) | ",
                 "RF Frequency (32-bit)", freq_bytes[0], freq_bytes[1],
                 freq_bytes[2], freq_bytes[3],
                 ((uint32_t)freq_bytes[0] << 24) |
                     ((uint32_t)freq_bytes[1] << 16) |
                     ((uint32_t)freq_bytes[2] << 8) | (uint32_t)freq_bytes[3]);
          printf("%.2f MHz\n", freq_mhz);
        } else if (freq_bytes[0] == 0xFF) {
          printf("0x088B-0x088E %-34s : 0xFF FF FF FF | [WARNING: 0xFF - "
                 "possible SPI issue]\n",
                 "RF Frequency (32-bit)");
        }
      }
      continue; // Skip individual frequency register entries
    }

    // Try to read the register
    uint8_t buffer = 0;
    status = sx126x_read_register(NULL, registers[i].addr, &buffer, 1);

    if (status == SX126X_STATUS_OK) {
      printf("0x%04X %-40s : 0x%02X (%3d) | ", registers[i].addr,
             registers[i].name, buffer, buffer);

      // Decode register value
      switch (registers[i].addr) {
      case 0x0740:
      case 0x0741:
        if (buffer == 0x34 || buffer == 0x44) {
          std::cout << "(Public LoRaWAN Sync Word)";
        } else if (buffer == 0x14 || buffer == 0x24) {
          std::cout << "(Private LoRa Network Sync Word)";
        } else {
          std::cout << "(Custom Sync Word)";
        }
        break;
      case 0x08E7:
        if (buffer == 0x18)
          std::cout << "(OCP: 60mA - Default)";
        else if (buffer == 0x38)
          std::cout << "(OCP: 140mA)";
        break;
      case 0x0736:
        if (buffer == 0x0D)
          std::cout << "(IQ: Standard/Default)";
        else if (buffer == 0x09)
          std::cout << "(IQ: Inverted)";
        break;
      default:
        if (buffer == 0xFF) {
          std::cout << "[WARNING: 0xFF - possible SPI issue]";
          all_ff_count++;
        }
      }
      std::cout << std::endl;
    } else {
      printf("0x%04X %-40s : READ FAILED (status: %d)\n", registers[i].addr,
             registers[i].name, (int)status);
      read_failures++;
    }
  }

  std::cout << "\n--- Summary ---" << std::endl;
  std::cout << "Total registers read: " << num_registers << std::endl;
  std::cout << "Read failures: " << read_failures << std::endl;
  std::cout << "Registers returning 0xFF: " << all_ff_count << std::endl;

  if (all_ff_count == num_registers) {
    std::cerr << "\n*** CRITICAL: ALL registers are 0xFF! ***" << std::endl;
    std::cerr << "Possible causes:" << std::endl;
    std::cerr << "1. SPI bus not connected or not working" << std::endl;
    std::cerr << "2. CS pin not toggling correctly" << std::endl;
    std::cerr << "3. SX126X chip not powered or not responding" << std::endl;
    std::cerr << "4. Device clock issue" << std::endl;
  }

  std::cout << "\n--- Extended Register Bank (0x01xx) ---\n" << std::endl;

  // Read some extended registers
  uint16_t ext_regs[] = {0x0120, 0x0121, 0x0131, 0x01A4, 0x01A5};
  int ext_all_ff = 0;
  for (uint16_t addr : ext_regs) {
    uint8_t buffer = 0;
    status = sx126x_read_register(NULL, addr, &buffer, 1);

    if (status == SX126X_STATUS_OK) {
      printf("0x%04X : 0x%02X (%3d)", addr, buffer, buffer);
      if (buffer == 0xFF) {
        std::cout << "  [0xFF]";
        ext_all_ff++;
      }
      std::cout << std::endl;
    } else {
      printf("0x%04X : READ FAILED (status: %d)\n", addr, (int)status);
    }
  }

  if (ext_all_ff == 5) {
    std::cerr << "\n*** Extended registers also all 0xFF - Likely chip "
                 "communication issue ***"
              << std::endl;
  }

  std::cout << "\n==========================================\n" << std::endl;
}

