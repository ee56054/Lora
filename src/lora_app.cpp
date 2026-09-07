#include "config.h"
#include "hal.h"
#include "modbus.h"
#include "modbus_lora.h"
#include "sx126x.h"
#include "sx126x_hal.h"
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include "web_server.h"

modbus_t *g_modbus_ctx = nullptr;

// LoRa configuration constants
#define FREQUENCY g_config.frequency
#define TX_POWER g_config.tx_power
#define SPREADING_FACTOR g_config.spreading_factor
#define BANDWIDTH g_config.bandwidth
#define CODING_RATE g_config.coding_rate
#define PREAMBLE_LENGTH g_config.preamble_length
#define RX_TIMEOUT g_config.rx_timeout

// Globals for echo server
#include <mutex>
#include <queue>
#include <vector>

struct RxMessage {
  std::vector<uint8_t> data;
};

std::queue<RxMessage> message_queue;
std::mutex queue_mutex;

// Interrupt handler for DIO1 - processes packets directly
void dio1_interrupt_handler(void) {
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
      std::cout << "  Data: ";

      // Print payload as string (if printable) or hex
      bool all_printable = true;
      for (uint8_t i = 0; i < payload_len; i++) {
        if (payload[i] < 32 || payload[i] > 126) {
          all_printable = false;
          break;
        }
      }

      if (all_printable) {
        for (uint8_t i = 0; i < payload_len; i++) {
          std::cout << (char)payload[i];
        }
      } else {
        for (uint8_t i = 0; i < payload_len; i++) {
          printf("%02X ", payload[i]);
        }
      }
      std::cout << std::endl;

      // Copy to message queue to trigger a reply
      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        RxMessage msg;
        msg.data.assign(payload, payload + payload_len);
        message_queue.push(msg);
      }
    }

    // Clear RX_DONE IRQ
    sx126x_clear_irq_status(NULL, SX126X_IRQ_RX_DONE);
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
  sx126x_status_t status;

  // Switch to TX mode
  set_rf_switch_tx();
  // Write payload to TX buffer
  status = sx126x_write_buffer(NULL, 0, payload, size);
  if (status != SX126X_STATUS_OK) {
    std::cerr << "Failed to write payload to buffer" << std::endl;
    // Re-enable RX if write failed
    set_rf_switch_rx();
    sx126x_set_rx_with_timeout_in_rtc_step(NULL, SX126X_RX_CONTINUOUS);
    return false;
  }

  // Set the hardware packet parameters to transmit EXACTLY size bytes
  pkt_params->pld_len_in_bytes = size;
  sx126x_set_lora_pkt_params(NULL, pkt_params);

  // Start transmission (interrupt will handle TX_DONE and revert to RX)
  std::cout << "tx: " << payload << std::endl;
  status = sx126x_set_tx(NULL, 0);
  std::cout << "tx done: " << payload << std::endl;
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

  if (g_config.modbus_enabled && g_modbus_ctx != nullptr) {
    std::cout << "\n-- LoRa Transceiver (Modbus Master) --\n" << std::endl;

    if (!initialize_receiver(mod_params, pkt_params)) {
      std::cerr << "Failed to initialize receiver mode" << std::endl;
      return;
    }

    std::cout << "Transceiver initialized. Polling Modbus registers..."
              << std::endl;

    while (true) {
      if (stat("config.json", &st) == 0 && st.st_mtime > last_config_time) {
        std::cout << "\nconfig.json modified! Reloading application..." << std::endl;
        return;
      }

      if (!g_config.modbus_address_devices.empty()) {
        for (int device_id : g_config.modbus_address_devices) {
          uint16_t dest[1];
          // Set the target slave/device address for this poll
          modbus_set_slave(g_modbus_ctx, device_id);

          int reg_to_read = 100; // Hardcoded test register
          std::cout << "\n--- Reading Register " << reg_to_read
                    << " from Device " << device_id << " ---" << std::endl;

          int rc = modbus_read_registers(g_modbus_ctx, reg_to_read, 1, dest);
          if (rc == -1) {
            std::cerr << "Failed to read device " << device_id << ": "
                      << modbus_strerror(errno) << std::endl;
          } else {
            std::cout << ">>> Device " << device_id << " Register "
                      << reg_to_read << " value: " << dest[0] << " <<<"
                      << std::endl;
          }

          usleep(1000000); // 1 second delay between polls
        }
      } else {
        std::cout << "No devices to poll in config. Sleeping..." << std::endl;
        usleep(5000000); // 5 seconds
      }
    }
  } else {
    std::cout << "\n-- LoRa Transceiver --\n" << std::endl;

    if (!initialize_receiver(mod_params, pkt_params)) {
      std::cerr << "Failed to initialize receiver mode" << std::endl;
      return;
    }

    std::cout
        << "Transceiver initialized. Listening for packets and transmitting every 1 sec..."
        << std::endl;

    auto last_tx_time = std::chrono::steady_clock::now();
    uint32_t tx_counter = 0;

    while (true) {
      if (stat("config.json", &st) == 0 && st.st_mtime > last_config_time) {
        std::cout << "\nconfig.json modified! Reloading application..." << std::endl;
        return;
      }

      bool has_message = false;
      RxMessage rx_msg;

      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (!message_queue.empty()) {
          rx_msg = message_queue.front();
          message_queue.pop();
          has_message = true;
        }
      }

      if (has_message && !sx126x_is_busy()) {
        std::cout << "\n--- Replying to Message ---" << std::endl;

        // Create a reply payload
        uint8_t reply_payload[256];
        const char *prefix = "Echo: ";
        uint8_t prefix_len = strlen(prefix);

        memcpy(reply_payload, prefix, prefix_len);

        uint8_t copy_len = rx_msg.data.size();
        if (prefix_len + copy_len > 255) {
          copy_len = 255 - prefix_len;
        }
        memcpy(reply_payload + prefix_len, rx_msg.data.data(), copy_len);
        uint8_t reply_len = prefix_len + copy_len;

        if (!transmit(reply_payload, reply_len, pkt_params)) {
          std::cerr << "Failed to send reply!" << std::endl;
        }
      }

      // Transmit every 1 second
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tx_time).count() >= 1000) {
        last_tx_time = now;
        if (!sx126x_is_busy()) {
          char tx_payload[64];
          int tx_len = snprintf(tx_payload, sizeof(tx_payload), "HeLoRa World! %u", tx_counter++);
          std::cout << "\n--- Periodic Transmission (every 1s) ---" << std::endl;
          if (!transmit((const uint8_t *)tx_payload, tx_len, pkt_params)) {
            std::cerr << "Periodic transmission failed!" << std::endl;
          }
        }
      }

      usleep(50000); // 50ms sleep to check queue and timer smoothly
    }
  }
}

#include "diagnostics.h"

#include "lora_app.h"

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

  if (g_config.modbus_enabled) {
    std::cout << "\nInitializing Custom Modbus LoRa Backend..." << std::endl;
    g_modbus_ctx = modbus_new_lora(&pkt_params);
    if (g_modbus_ctx == nullptr) {
      std::cerr << "Unable to create the libmodbus LoRa context\n" << std::endl;
    } else {
      modbus_set_slave(g_modbus_ctx, g_config.modbus_slave_id);
      if (modbus_connect(g_modbus_ctx) == -1) {
        std::cerr << "Modbus connection failed: " << modbus_strerror(errno)
                  << std::endl;
        modbus_free(g_modbus_ctx);
        g_modbus_ctx = nullptr;
      } else {
        std::cout << "Modbus LoRa backend connected successfully" << std::endl;
      }
    }
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

  if (g_modbus_ctx != nullptr) {
    modbus_close(g_modbus_ctx);
    modbus_free(g_modbus_ctx);
    g_modbus_ctx = nullptr;
  }
  
  std::cout << "\nRestarting application to apply new configuration...\n" << std::endl;
  usleep(1000000); // 1 second
  }

  return 0;
}