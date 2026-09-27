#include "config.h"
#include "hal.h"
#include "lora_app.h"
#include "modbus_app.h"
#include "lora_security.h"
#include "sx126x.h"
#include "sx126x_hal.h"
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include <mutex>
#include "web_server.h"

// LoRa configuration constants
#define FREQUENCY g_config.frequency
#define TX_POWER g_config.tx_power
#define SPREADING_FACTOR g_config.spreading_factor
#define BANDWIDTH g_config.bandwidth
#define CODING_RATE g_config.coding_rate
#define PREAMBLE_LENGTH g_config.preamble_length
#define RX_TIMEOUT g_config.rx_timeout

std::recursive_mutex g_lora_mutex;

// Forward declaration of transmit
bool transmit(const uint8_t *payload, uint8_t size,
              sx126x_pkt_params_lora_t *pkt_params);

static sx126x_pkt_params_lora_t g_default_pkt_params = {
    .preamble_len_in_symb = 8,
    .header_type = SX126X_LORA_PKT_EXPLICIT,
    .pld_len_in_bytes = 0xff,
    .crc_is_on = false,
    .invert_iq_is_on = false};
sx126x_pkt_params_lora_t *g_pkt_params = &g_default_pkt_params;

// Interrupt handler for DIO1 - processes packets directly
void dio1_interrupt_handler(void) {
  std::lock_guard<std::recursive_mutex> lock(g_lora_mutex);
  uint8_t payload[256];
  uint8_t payload_len;
  int16_t rssi = 0;
  int8_t snr = 0;
  static uint32_t packet_count = 0;
  sx126x_status_t status;

  // Get IRQ status
  sx126x_irq_mask_t irq_mask;
  status = sx126x_get_irq_status(NULL, &irq_mask);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to get IRQ status, status: " << (int)status
              << std::endl;
    return;
  }

  // Check for RX done
  if (irq_mask & SX126X_IRQ_RX_DONE) {
    // Get packet status
    sx126x_pkt_status_lora_t pkt_status;
    status = sx126x_get_lora_pkt_status(NULL, &pkt_status);
    if (status == SX126X_STATUS_OK) {
      rssi = pkt_status.rssi_pkt_in_dbm;
      snr = pkt_status.snr_pkt_in_db;
    }

    // Get RX buffer status
    sx126x_rx_buffer_status_t rx_buffer_status;
    status = sx126x_get_rx_buffer_status(NULL, &rx_buffer_status);
    uint8_t buffer_offset = rx_buffer_status.buffer_start_pointer;
    payload_len = rx_buffer_status.pld_len_in_bytes;

    // Read payload from buffer
    status = sx126x_read_buffer(NULL, buffer_offset, payload, payload_len);
    if (status == SX126X_STATUS_OK) {
      packet_count++;
      std::cout << "\n[Packet #" << packet_count << "] Received "
                << (int)payload_len << " bytes:" << std::endl;
      std::cout << "  RSSI: " << (int)rssi << " dBm, SNR: " << (int)snr << " dB"
                << std::endl;

      // Print raw packet in hex
      std::cout << "  Raw Hex: ";
      for (uint8_t i = 0; i < payload_len; i++) {
        printf("%02X ", payload[i]);
      }
      std::cout << std::endl;

      // Handle security decryption & MIC verification
      uint8_t proc_payload[256];
      uint16_t proc_len = 0;
      bool packet_valid = true;

      if (lora_security_is_encrypted(payload, payload_len)) {
        if (lora_security_decrypt(payload, payload_len, proc_payload, &proc_len)) {
          std::cout << "  [Security] Decrypted " << (int)proc_len
                    << " bytes (MIC verified, AES-128-CTR)" << std::endl;
        } else {
          std::cerr << "  [Security] Decryption FAILED! Corrupted packet or invalid key. Packet dropped."
                    << std::endl;
          packet_valid = false;
        }
      } else {
        if (g_config.security_enabled) {
          std::cout << "  [Security] Warning: Plaintext packet received while security is enabled."
                    << std::endl;
        }
        memcpy(proc_payload, payload, payload_len);
        proc_len = payload_len;
      }

      if (packet_valid && proc_len > 0) {
        if (proc_len < sizeof(proc_payload)) {
          proc_payload[proc_len] = '\0';
        }

        // Print processed payload as string (if printable) or hex
        bool all_printable = true;
        for (uint16_t i = 0; i < proc_len; i++) {
          if (proc_payload[i] < 32 || proc_payload[i] > 126) {
            all_printable = false;
            break;
          }
        }

        std::cout << "  Payload: ";
        if (all_printable) {
          for (uint16_t i = 0; i < proc_len; i++) {
            std::cout << (char)proc_payload[i];
          }
        } else {
          for (uint16_t i = 0; i < proc_len; i++) {
            printf("%02X ", proc_payload[i]);
          }
        }
        std::cout << std::endl;

        // Clear RX_DONE IRQ
        sx126x_clear_irq_status(NULL, SX126X_IRQ_RX_DONE);

        if (g_config.modbus_enabled) {
          modbus_print_packet_value(proc_payload, (uint8_t)proc_len);
          modbus_queue_rx_packet(proc_payload, (uint8_t)proc_len);
        }
      } else {
        // Clear RX_DONE IRQ even if packet was dropped
        sx126x_clear_irq_status(NULL, SX126X_IRQ_RX_DONE);
      }
    } else {
      // Clear RX_DONE IRQ if buffer read failed
      sx126x_clear_irq_status(NULL, SX126X_IRQ_RX_DONE);
    }
  } else if (irq_mask & SX126X_IRQ_TX_DONE) {
    std::cout << "\nTX_DONE IRQ detected - transmission complete!" << std::endl;
    sx126x_clear_irq_status(NULL, SX126X_IRQ_TX_DONE);

    // Return to continuous RX mode
    std::cout << "Returning to continuous RX mode..." << std::endl;
    set_rf_switch_rx();

    // Reset the payload length parameter back to maximum (0xFF) for receiving
    sx126x_pkt_params_lora_t rx_pkt_params = {
        .preamble_len_in_symb = PREAMBLE_LENGTH,
        .header_type = SX126X_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes = 0xFF,
        .crc_is_on = false,
        .invert_iq_is_on = false};
    sx126x_set_lora_pkt_params(NULL, &rx_pkt_params);

    sx126x_set_rx_with_timeout_in_rtc_step(
        NULL, SX126X_RX_CONTINUOUS); // true continuous RX
  } else if (irq_mask & SX126X_IRQ_TIMEOUT) {
    std::cout << "\nRX_TIMEOUT IRQ - timeout occurred" << std::endl;
    sx126x_clear_irq_status(NULL, SX126X_IRQ_TIMEOUT);
  } else if (irq_mask & SX126X_IRQ_CRC_ERROR) {
    std::cout << "\nCRC_ERROR IRQ - packet corrupted" << std::endl;
    sx126x_clear_irq_status(NULL, SX126X_IRQ_CRC_ERROR);
  } else {
    // Clear all IRQs to be safe
    sx126x_clear_irq_status(NULL, SX126X_IRQ_ALL);
  }

  std::cout << "interrupt handler finished: " << packet_count << " packets"
            << std::endl;
}

// Initialize receiver mode
bool initialize_receiver(sx126x_mod_params_lora_t *mod_params,
                         sx126x_pkt_params_lora_t *pkt_params) {
  sx126x_status_t status;

  std::cout << "\nEnabling receiver mode..." << std::endl;

  // Enable receiver
  set_rf_switch_rx();

  // Start continuous RX mode once during initialization
  std::cout << "Starting continuous RX mode..." << std::endl;
  status = sx126x_set_rx_with_timeout_in_rtc_step(
      NULL, SX126X_RX_CONTINUOUS); // true continuous RX
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to start continuous RX, status: " << (int)status
              << std::endl;
    return false;
  }

  // Setup DIO1 interrupt handler
  std::cout << "Setting up DIO1 interrupt..." << std::endl;
  if (!hal_attach_dio1_interrupt(&dio1_interrupt_handler)) {
    std::cerr << "Failed to setup DIO1 interrupt" << std::endl;
    return false;
  }

  std::cout << "Continuous RX mode started successfully" << std::endl;
  return true;
}

// Transmit a packet
bool transmit(const uint8_t *payload, uint8_t size,
              sx126x_pkt_params_lora_t *pkt_params) {
  std::lock_guard<std::recursive_mutex> lock(g_lora_mutex);
  sx126x_status_t status;

  const uint8_t *tx_data = payload;
  uint8_t tx_len = size;
  uint8_t sec_buf[256];
  uint16_t sec_len = 0;

  if (g_config.security_enabled) {
    if (lora_security_encrypt(payload, size, sec_buf, &sec_len)) {
      tx_data = sec_buf;
      tx_len = (uint8_t)sec_len;
      std::cout << "[Security] Encrypted TX frame (" << (int)size << " -> " << (int)tx_len << " bytes)" << std::endl;
    } else {
      std::cerr << "[Security] Encryption failed, sending plaintext!" << std::endl;
    }
  }

  // Switch to TX mode
  set_rf_switch_tx();
  // Write payload to TX buffer
  status = sx126x_write_buffer(NULL, 0, tx_data, tx_len);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to write payload to buffer" << std::endl;
    // Re-enable RX if write failed
    set_rf_switch_rx();
    sx126x_set_rx_with_timeout_in_rtc_step(NULL, SX126X_RX_CONTINUOUS);
    return false;
  }

  // Set the hardware packet parameters to transmit EXACTLY tx_len bytes
  pkt_params->pld_len_in_bytes = tx_len;
  sx126x_set_lora_pkt_params(NULL, pkt_params);

  // Start transmission (interrupt will handle TX_DONE and revert to RX)
  std::cout << "tx (" << (int)tx_len << " bytes): ";
  bool all_printable = true;
  for (size_t i = 0; i < tx_len; i++) {
    if (tx_data[i] < 32 || tx_data[i] > 126) {
      all_printable = false;
      break;
    }
  }
  for (size_t i = 0; i < tx_len; i++) {
    printf("%02X ", tx_data[i]);
  }
  if (all_printable) {
    std::cout << "(\"";
    for (size_t i = 0; i < tx_len; i++) {
      std::cout << (char)tx_data[i];
    }
    std::cout << "\")";
  }
  std::cout << std::endl;

  status = sx126x_set_tx(NULL, 0);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to start transmission" << std::endl;
    // Re-enable RX if start TX failed
    set_rf_switch_rx();
    sx126x_set_rx_with_timeout_in_rtc_step(NULL, SX126X_RX_CONTINUOUS);
    return false;
  }

  return true;
}

// Transceiver loop
void transceiver_loop(sx126x_mod_params_lora_t *mod_params,
                      sx126x_pkt_params_lora_t *pkt_params) {
  time_t last_config_time = 0;
  struct stat st;
  if (stat("config.json", &st) == 0) {
    last_config_time = st.st_mtime;
  }

  if (g_config.modbus_enabled && is_modbus_ready()) {
    run_modbus_loop(mod_params, pkt_params);
    return;
  }

  std::cout << "\n-- LoRa Transceiver --\n" << std::endl;

  if (!initialize_receiver(mod_params, pkt_params)) {
    std::cerr << "Failed to initialize receiver mode" << std::endl;
    return;
  }

  std::cout
      << "Transceiver initialized. Listening for packets and transmitting every 5 sec..."
      << std::endl;

  auto last_tx_time = std::chrono::steady_clock::now();
  uint32_t tx_counter = 0;

  while (true) {
    if (stat("config.json", &st) == 0 && st.st_mtime > last_config_time) {
      std::cout << "\nconfig.json modified! Reloading application..." << std::endl;
      return;
    }

    // Transmit every 5 seconds
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tx_time).count() >= 5000) {
      last_tx_time = now;
      std::lock_guard<std::recursive_mutex> lock(g_lora_mutex);
      if (!sx126x_is_busy()) {
        char tx_payload[64];
        int tx_len = snprintf(tx_payload, sizeof(tx_payload), "HeLoRa World! %u", tx_counter++);
        std::cout << "\n--- Periodic Transmission (every 5s) ---" << std::endl;
        if (!transmit((const uint8_t *)tx_payload, tx_len, pkt_params)) {
          std::cerr << "Periodic transmission failed!" << std::endl;
        }
      }
    }

    usleep(50000); // 50ms sleep to check timer and config smoothly
  }
}

#include "diagnostics.h"

int run_lora_app() {
  static bool web_server_started = false;
  if (!web_server_started) {
      std::thread web_thread([]() {
          start_web_server();
      });
      web_thread.detach();
      web_server_started = true;
  }

  while (true) {
    if (!g_config.load_from_file("config.json")) {
      std::cout
          << "Config file not found or invalid, saving defaults to config.json..."
          << std::endl;
      g_config.save_to_file("config.json");
    }

    // Initialize LoRa Security Engine (AES-128-CTR + CRC16 MIC)
    if (g_config.security_enabled) {
      if (!lora_security_set_key_hex(g_config.aes_key)) {
        std::cerr << "[Security] Invalid AES key in config! Falling back to factory default key." << std::endl;
        lora_security_init(nullptr);
      }
    }

    std::cout << "Hello, World from CMake project with SX126X driver!"
              << std::endl;

  // Display all configuration settings
  show_configuration();

  std::cout << "\nInitializing SX126X LoRa radio module..." << std::endl;

  // Initialize HAL setup
  if (!hal_setup()) {
    std::cerr << "Failed to initialize HAL setup" << std::endl;
    return 1;
  }

  // Initialize HAL (reset, GPIO configuration)
  sx126x_hal_status_t hal_status = sx126x_hal_reset(NULL);
  if (hal_status != SX126X_HAL_STATUS_OK) {
    std::cerr << "Failed to reset device" << std::endl;
    return 1;
  }

  // Initialize SPI
  if (!spi_init()) {
    std::cerr << "Failed to initialize SPI" << std::endl;
    return 1;
  }

  // Get device status
  sx126x_chip_status_t chip_status;
  sx126x_status_t status = sx126x_get_status(NULL, &chip_status);
  if (status == SX126X_STATUS_OK) {
    std::cout << "SX126X Status retrieved successfully!" << std::endl;
    std::cout << "  Command status: " << (int)chip_status.cmd_status
              << std::endl;
    std::cout << "  Chip mode: " << (int)chip_status.chip_mode << std::endl;
  } else {
    std::cerr << "Failed to get device status, status: " << (int)status
              << std::endl;
  }

  // Set the device to standby mode with external oscillator
  status = sx126x_set_standby(NULL, SX126X_STANDBY_CFG_XOSC);
  if (status == SX126X_STATUS_OK) {
    std::cout << "SX126X set to standby mode successfully!" << std::endl;
  } else {
    std::cerr << "Failed to set standby mode, status: " << (int)status
              << std::endl;
  }

  // ================================
  // Base Configuration (from STM32 project)
  // ================================

  // Set regulator mode to DC-DC
  std::cout << "Setting regulator mode to DC-DC..." << std::endl;
  status = sx126x_set_reg_mode(NULL, SX126X_REG_MODE_DCDC);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set regulator mode, status: " << (int)status
              << std::endl;
  }

  // Set buffer base address
  status = sx126x_set_buffer_base_address(NULL, 0x00, 0x00);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set buffer base address, status: " << (int)status
              << std::endl;
  }

  // Set packet type to LoRa
  status = sx126x_set_pkt_type(NULL, SX126X_PKT_TYPE_LORA);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set packet type, status: " << (int)status
              << std::endl;
  }

  // Set trimming capacitor values
  status = sx126x_set_trimming_capacitor_values(NULL, 0x04, 0x2f);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set trimming capacitors, status: " << (int)status
              << std::endl;
  }

  // ================================
  // Configure LoRa Parameters
  // ================================

  // Set DIO2 as RF switch pin
  std::cout << "\nConfiguring RF switch (DIO2)..." << std::endl;
  status = sx126x_set_dio2_as_rf_sw_ctrl(NULL, true);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set DIO2 as RF switch, status: " << (int)status
              << std::endl;
  }

  // Set frequency to 915 MHz
  std::cout << "Setting frequency to 915 MHz..." << std::endl;
  status = sx126x_set_rf_freq(NULL, FREQUENCY);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set frequency, status: " << (int)status
              << std::endl;
  }

  // Set TX power to +22 dBm (for SX1262)
  std::cout << "Setting TX power to +22 dBm..." << std::endl;
  sx126x_ramp_time_t ramp_time = SX126X_RAMP_3400_US; // Default ramp time
  status = sx126x_set_tx_params(NULL, TX_POWER, ramp_time);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set TX power, status: " << (int)status << std::endl;
  }

  // Configure modulation parameters
  // SF=9, BW=125kHz, CR=4/6
  std::cout << "Setting modulation parameters (SF=9, BW=125kHz, CR=4/6)..."
            << std::endl;
  sx126x_mod_params_lora_t mod_params = {
      .sf = SPREADING_FACTOR,
      .bw = BANDWIDTH,
      .cr = CODING_RATE,
      .ldro = 0 // Low DataRate Optimization disabled
  };
  status = sx126x_set_lora_mod_params(NULL, &mod_params);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set modulation parameters, status: " << (int)status
              << std::endl;
  }

  // Configure packet parameters
  // Explicit header, preamble=8, payload=255
  std::cout
      << "Setting packet parameters (Explicit header, preamble=8)..."
      << std::endl;
  sx126x_pkt_params_lora_t pkt_params = {
      .preamble_len_in_symb = PREAMBLE_LENGTH,
      .header_type = SX126X_LORA_PKT_EXPLICIT,
      .pld_len_in_bytes = 0xff,
      // .pld_len_in_bytes = 15,
      // .crc_is_on = true,
      .crc_is_on = false,
      .invert_iq_is_on = false};
  status = sx126x_set_lora_pkt_params(NULL, &pkt_params);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set packet parameters, status: " << (int)status
              << std::endl;
  };
  g_pkt_params = &pkt_params;

  if (g_config.modbus_enabled) {
    init_modbus(&pkt_params);
  }

  sx126x_pa_cfg_params_t pa_cfg = {0};

  pa_cfg.pa_duty_cycle = 0x04; // 100% duty cycle
  pa_cfg.device_sel = 0x00;    // Use PA_BOOST pin
  pa_cfg.pa_lut = 0x01;        // Use PA config from register
  pa_cfg.hp_max = 0x07;        // Max power for HP PA (0-7, where 7 is max)s

  status = sx126x_set_pa_cfg(NULL, &pa_cfg);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to set PA config, status: " << (int)status
              << std::endl;
  }

  // Configure DIO IRQ parameters - enable RX_DONE and TX_DONE interrupts
  sx126x_set_dio_irq_params(NULL, SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE,
                            SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE,
                            SX126X_IRQ_NONE, SX126X_IRQ_NONE);

  // Clear any pending IRQs
  sx126x_clear_irq_status(NULL, SX126X_IRQ_ALL);

  // Set sync word to 0x3444 (public LoRa network)
  // std::cout << "Setting sync word to 0x3444..." << std::endl;
  // status = sx126x_set_lora_sync_word(NULL, 0x44); // Low byte of 0x3444
  // if (status != SX126X_STATUS_OK) {
  //   std::cerr << "Failed to set sync word, status: " << (int)status
  //             << std::endl;
  // }

  // Read and verify chip configuration
  std::cout << "\n--- Verifying Configuration on Chip ---" << std::endl;
  read_chip_configuration();

  // Read all registers for debugging
  read_all_registers();

  // ================================
  // Transceiver Operation
  // ================================
  transceiver_loop(&mod_params, &pkt_params);

  // Cleanup (unreachable in current loop, but good practice)
  set_rf_switch_rx();
  spi_close();

  cleanup_modbus();
  
  std::cout << "\nRestarting application to apply new configuration...\n" << std::endl;
  usleep(1000000); // 1 second
  }

  return 0;
}