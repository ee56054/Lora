#include "modbus_app.h"
#include "lora_app.h"
#include "config.h"
#include "modbus.h"
#include "modbus_lora.h"
#include <iostream>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>

// Shared queue and mutex for received LoRa packets destined for Modbus
std::queue<RxMessage> message_queue;
std::mutex queue_mutex;

// Global Modbus context for the LoRa backend
static modbus_t *g_modbus_ctx = nullptr;

enum ModbusTableType {
  MODBUS_TABLE_COIL,            // FC 01
  MODBUS_TABLE_DISCRETE_INPUT,  // FC 02
  MODBUS_TABLE_INPUT_REG,       // FC 04
  MODBUS_TABLE_HOLDING_REG      // FC 03
};

struct ModbusPollTarget {
  ModbusTableType type;
  int address;
  const char *name;
  const char *fc_str;
};

static const ModbusPollTarget g_modbus_poll_targets[] = {
  // Coils (0x) - FC 01
  { MODBUS_TABLE_COIL, 0, "Valve 1", "FC 01" },
  { MODBUS_TABLE_COIL, 1, "Valve 2", "FC 01" },

  // Discrete Inputs (1x) - FC 02
  { MODBUS_TABLE_DISCRETE_INPUT, 0, "Valve 1 Status", "FC 02" },
  { MODBUS_TABLE_DISCRETE_INPUT, 1, "Valve 2 Status", "FC 02" },

  // Input Registers (3x) - FC 04
  { MODBUS_TABLE_INPUT_REG, 0, "Sensor 1", "FC 04" },
  { MODBUS_TABLE_INPUT_REG, 1, "Sensor 2", "FC 04" },

  // Holding Registers (4x) - FC 03
  { MODBUS_TABLE_HOLDING_REG, 0, "HW ID High", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 1, "HW ID Low", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 2, "TX Count High", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 3, "TX Count Low", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 4, "Valve 1 Register", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 5, "Valve 2 Register", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 6, "Sensor 1 Register", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 7, "Sensor 2 Register", "FC 03" },
  { MODBUS_TABLE_HOLDING_REG, 8, "Slave ID", "FC 03" }
};

bool init_modbus(sx126x_pkt_params_lora_t *pkt_params) {
  if (!g_config.modbus_enabled) {
    return false;
  }

  if (g_modbus_ctx != nullptr) {
    cleanup_modbus();
  }

  std::cout << "\nInitializing Custom Modbus LoRa Backend..." << std::endl;
  g_modbus_ctx = modbus_new_lora(pkt_params);
  if (g_modbus_ctx == nullptr) {
    std::cerr << "Unable to create the libmodbus LoRa context\n" << std::endl;
    return false;
  }

  modbus_set_slave(g_modbus_ctx, g_config.modbus_slave_id);
  modbus_set_response_timeout(g_modbus_ctx, 3, 0);

  if (modbus_connect(g_modbus_ctx) == -1) {
    std::cerr << "Modbus connection failed: " << modbus_strerror(errno) << std::endl;
    modbus_free(g_modbus_ctx);
    g_modbus_ctx = nullptr;
    return false;
  }

  std::cout << "Modbus LoRa backend connected successfully" << std::endl;
  return true;
}

bool is_modbus_ready() {
  return g_modbus_ctx != nullptr;
}

void modbus_queue_rx_packet(const uint8_t *payload, uint8_t payload_len) {
  std::lock_guard<std::mutex> lock(queue_mutex);
  RxMessage msg;
  msg.data.assign(payload, payload + payload_len);
  message_queue.push(msg);
}

void cleanup_modbus() {
  if (g_modbus_ctx != nullptr) {
    modbus_close(g_modbus_ctx);
    modbus_free(g_modbus_ctx);
    g_modbus_ctx = nullptr;
  }
}

void run_modbus_loop(sx126x_mod_params_lora_t *mod_params,
                     sx126x_pkt_params_lora_t *pkt_params) {
  if (!is_modbus_ready()) {
    std::cerr << "Cannot run Modbus loop: context is not ready" << std::endl;
    return;
  }

  std::cout << "\n-- LoRa Transceiver (Modbus Master) --\n" << std::endl;

  if (!initialize_receiver(mod_params, pkt_params)) {
    std::cerr << "Failed to initialize receiver mode" << std::endl;
    return;
  }

  std::cout << "Transceiver initialized. Polling each Modbus address..."
            << std::endl;

  time_t last_config_time = 0;
  struct stat st;
  if (stat("config.json", &st) == 0) {
    last_config_time = st.st_mtime;
  }

  while (true) {
    if (stat("config.json", &st) == 0 && st.st_mtime > last_config_time) {
      std::cout << "\nconfig.json modified! Reloading application..." << std::endl;
      return;
    }

    std::vector<int> devices = g_config.modbus_address_devices;
    if (devices.empty()) {
      devices.push_back(g_config.modbus_slave_id > 0 ? g_config.modbus_slave_id : 1);
    }

    for (int device_id : devices) {
      modbus_set_slave(g_modbus_ctx, device_id);

      for (const auto &target : g_modbus_poll_targets) {
        if (stat("config.json", &st) == 0 && st.st_mtime > last_config_time) {
          std::cout << "\nconfig.json modified! Reloading application..." << std::endl;
          return;
        }

        std::cout << "\n--- Reading Device " << device_id << " Address 0x"
                  << std::hex << target.address << " (" << std::dec << target.address << ") : "
                  << target.name << " [" << target.fc_str << "] ---" << std::endl;

        int rc = -1;
        if (target.type == MODBUS_TABLE_COIL) {
          uint8_t dest[1] = {0};
          rc = modbus_read_bits(g_modbus_ctx, target.address, 1, dest);
          if (rc == -1) {
            std::cerr << "Failed to read Coil " << target.address << " (" << target.name << "): "
                      << modbus_strerror(errno) << std::endl;
          } else {
            std::cout << ">>> Device " << device_id << " " << target.name
                      << " (Coil " << target.address << "): " << (dest[0] ? "1 (Open/ON)" : "0 (Closed/OFF)")
                      << " <<<" << std::endl;
          }
        } else if (target.type == MODBUS_TABLE_DISCRETE_INPUT) {
          uint8_t dest[1] = {0};
          rc = modbus_read_input_bits(g_modbus_ctx, target.address, 1, dest);
          if (rc == -1) {
            std::cerr << "Failed to read Discrete Input " << target.address << " (" << target.name << "): "
                      << modbus_strerror(errno) << std::endl;
          } else {
            std::cout << ">>> Device " << device_id << " " << target.name
                      << " (Input " << target.address << "): " << (dest[0] ? "1 (Open)" : "0 (Closed)")
                      << " <<<" << std::endl;
          }
        } else if (target.type == MODBUS_TABLE_INPUT_REG) {
          uint16_t dest[1] = {0};
          rc = modbus_read_input_registers(g_modbus_ctx, target.address, 1, dest);
          if (rc == -1) {
            std::cerr << "Failed to read Input Register " << target.address << " (" << target.name << "): "
                      << modbus_strerror(errno) << std::endl;
          } else {
            std::cout << ">>> Device " << device_id << " " << target.name
                      << " (Input Reg " << target.address << "): " << dest[0]
                      << " <<<" << std::endl;
          }
        } else if (target.type == MODBUS_TABLE_HOLDING_REG) {
          uint16_t dest[1] = {0};
          rc = modbus_read_registers(g_modbus_ctx, target.address, 1, dest);
          if (rc == -1) {
            std::cerr << "Failed to read Holding Register " << target.address << " (" << target.name << "): "
                      << modbus_strerror(errno) << std::endl;
          } else {
            std::cout << ">>> Device " << device_id << " " << target.name
                      << " (Holding Reg " << target.address << "): " << dest[0]
                      << " <<<" << std::endl;
          }
        }

        usleep(5000000); // 5 second delay between requests
      }
    }
  }
}

