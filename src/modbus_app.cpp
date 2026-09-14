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
static std::mutex g_modbus_mutex;

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

/* Table of CRC values for high-order byte */
static const uint8_t table_crc_hi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40};

/* Table of CRC values for low-order byte */
static const uint8_t table_crc_lo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5,
    0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B,
    0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE,
    0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6,
    0xD2, 0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D,
    0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8,
    0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C,
    0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21,
    0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A,
    0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
    0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7,
    0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91, 0x51,
    0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98,
    0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D,
    0x4C, 0x8C, 0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83,
    0x41, 0x81, 0x80, 0x40};

uint16_t modbus_crc16(const uint8_t *buffer, uint16_t buffer_length) {
  uint8_t crc_hi = 0xFF;
  uint8_t crc_lo = 0xFF;
  unsigned int i;

  while (buffer_length--) {
    i = crc_lo ^ *buffer++;
    crc_lo = crc_hi ^ table_crc_hi[i];
    crc_hi = table_crc_lo[i];
  }

  return (crc_hi << 8 | crc_lo);
}

void modbus_print_packet_value(const uint8_t *payload, uint8_t payload_len) {
  if (payload == nullptr || payload_len < 3) {
    return;
  }

  uint8_t slave = payload[0];
  uint8_t fc = payload[1];

  bool crc_ok = false;
  if (payload_len >= 4) {
    uint16_t crc_calc = modbus_crc16(payload, payload_len - 2);
    uint16_t crc_recv = (payload[payload_len - 1] << 8) | payload[payload_len - 2];
    crc_ok = (crc_calc == crc_recv);
  }

  std::cout << "  [Modbus Decode] Slave ID: " << (int)slave << ", FC: 0x"
            << std::hex << (int)fc << std::dec;
  if (payload_len >= 4) {
    std::cout << (crc_ok ? " [CRC OK]" : " [CRC MISMATCH]");
  }
  std::cout << std::endl;

  // Handle Modbus Exception response
  if (fc >= 0x80) {
    uint8_t orig_fc = fc & 0x7F;
    uint8_t exc_code = (payload_len > 2) ? payload[2] : 0;
    const char *exc_str = "Unknown Exception";
    switch (exc_code) {
      case 1: exc_str = "Illegal Function"; break;
      case 2: exc_str = "Illegal Data Address"; break;
      case 3: exc_str = "Illegal Data Value"; break;
      case 4: exc_str = "Server Device Failure"; break;
      case 5: exc_str = "Acknowledge"; break;
      case 6: exc_str = "Server Device Busy"; break;
    }
    std::cout << "    >>> MODBUS EXCEPTION for FC 0x" << std::hex << (int)orig_fc
              << ": Code " << std::dec << (int)exc_code << " (" << exc_str << ") <<<" << std::endl;
    return;
  }

  switch (fc) {
    case 0x01: // Read Coils Response or Request
    case 0x02: { // Read Discrete Inputs Response or Request
      const char *name = (fc == 0x01) ? "Coil" : "Discrete Input";
      if (payload_len >= 5) {
        uint8_t byte_count = payload[2];
        std::cout << "    >>> Received " << name << " Values (Byte Count " << (int)byte_count << "): ";
        int bit_idx = 0;
        for (int i = 0; i < byte_count && (3 + i) < (payload_len - 2); i++) {
          uint8_t b = payload[3 + i];
          for (int bit = 0; bit < 8; bit++) {
            std::cout << "[" << bit_idx++ << "]=" << ((b >> bit) & 1 ? "1(ON) " : "0(OFF) ");
          }
        }
        std::cout << "<<<" << std::endl;
      }
      break;
    }

    case 0x03: // Read Holding Registers Response
    case 0x04: { // Read Input Registers Response
      const char *name = (fc == 0x03) ? "Holding Register" : "Input Register";
      if (payload_len >= 5) {
        uint8_t byte_count = payload[2];
        int num_regs = byte_count / 2;
        std::cout << "    >>> Received " << name << " Values (" << num_regs << " reg):" << std::endl;
        for (int i = 0; i < num_regs && (3 + 2 * i + 1) < (payload_len - 2); i++) {
          uint16_t val = (payload[3 + 2 * i] << 8) | payload[3 + 2 * i + 1];
          std::cout << "        [Reg " << i << "] = " << val << " (0x" << std::hex << val << std::dec << ")" << std::endl;
        }
      }
      break;
    }

    case 0x05: { // Write Single Coil
      if (payload_len >= 6) {
        uint16_t addr = (payload[2] << 8) | payload[3];
        uint16_t val = (payload[4] << 8) | payload[5];
        std::cout << "    >>> Write Single Coil Addr 0x" << std::hex << addr << std::dec
                  << " (" << addr << "): " << (val == 0xFF00 ? "1 (Open/ON)" : val == 0x0000 ? "0 (Closed/OFF)" : "Unknown")
                  << " (raw: 0x" << std::hex << val << std::dec << ") <<<" << std::endl;
      }
      break;
    }

    case 0x06: { // Write Single Register
      if (payload_len >= 6) {
        uint16_t addr = (payload[2] << 8) | payload[3];
        uint16_t val = (payload[4] << 8) | payload[5];
        std::cout << "    >>> Write Single Register Addr 0x" << std::hex << addr << std::dec
                  << " (" << addr << "): Value = " << val
                  << " (0x" << std::hex << val << std::dec << ") <<<" << std::endl;
      }
      break;
    }

    case 0x0F: { // Write Multiple Coils
      if (payload_len >= 7) {
        uint16_t addr = (payload[2] << 8) | payload[3];
        uint16_t qty = (payload[4] << 8) | payload[5];
        std::cout << "    >>> Write Multiple Coils Addr 0x" << std::hex << addr << std::dec
                  << ", Quantity: " << qty << " <<<" << std::endl;
      }
      break;
    }

    case 0x10: { // Write Multiple Registers
      if (payload_len >= 7) {
        uint16_t addr = (payload[2] << 8) | payload[3];
        uint16_t qty = (payload[4] << 8) | payload[5];
        std::cout << "    >>> Write Multiple Registers Addr 0x" << std::hex << addr << std::dec
                  << ", Quantity: " << qty;
        if (payload_len >= 8) {
          uint8_t byte_count = payload[6];
          std::cout << " (Byte Count: " << (int)byte_count << "):" << std::endl;
          for (int i = 0; i < qty && (7 + 2 * i + 1) < (payload_len - 2); i++) {
            uint16_t val = (payload[7 + 2 * i] << 8) | payload[7 + 2 * i + 1];
            std::cout << "        [Reg 0x" << std::hex << (addr + i) << std::dec << "] = "
                      << val << " (0x" << std::hex << val << std::dec << ")" << std::endl;
          }
        } else {
          std::cout << " <<<" << std::endl;
        }
      }
      break;
    }

    default: {
      std::cout << "    >>> Modbus Function Code 0x" << std::hex << (int)fc << std::dec
                << " (Data length: " << (int)payload_len << " bytes) <<<" << std::endl;
      break;
    }
  }
}

// Global telemetry state
static ModbusTelemetry g_telemetry;
static std::mutex g_telemetry_mutex;

static std::string get_current_time_str() {
  time_t now = time(nullptr);
  char buf[64];
  struct tm *tm_info = localtime(&now);
  if (tm_info) {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buf);
  }
  return "Unknown";
}

static std::string g_last_modbus_error = "";
static std::mutex g_error_mutex;

static void set_last_modbus_error(const std::string &err) {
  std::lock_guard<std::mutex> lock(g_error_mutex);
  g_last_modbus_error = err;
}

std::string get_last_modbus_error() {
  std::lock_guard<std::mutex> lock(g_error_mutex);
  return g_last_modbus_error;
}

ModbusTelemetry get_modbus_telemetry() {
  std::lock_guard<std::mutex> lock(g_telemetry_mutex);
  return g_telemetry;
}

void run_modbus_loop(sx126x_mod_params_lora_t *mod_params,
                     sx126x_pkt_params_lora_t *pkt_params) {
  if (!is_modbus_ready()) {
    std::cerr << "Cannot run Modbus loop: context is not ready" << std::endl;
    return;
  }

  std::cout << "\n-- LoRa Transceiver (Modbus Continuous Listening Mode) --\n" << std::endl;

  if (!initialize_receiver(mod_params, pkt_params)) {
    std::cerr << "Failed to initialize receiver mode" << std::endl;
    return;
  }

  std::cout << "Transceiver initialized in continuous listening mode." << std::endl;
  std::cout << "Modbus polling loop is STOPPED. Read & Write operations are ready on-demand via Web UI / API." << std::endl;

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

    usleep(100000); // 100ms idle wait, continuous RX active
  }
}

// --- Modbus Read Functions Implementation ---

bool modbus_read_coil_val(int slave_id, int address, bool &out_val) {
  if (!is_modbus_ready()) {
    std::string err = "Modbus context not initialized (check if modbus_enabled is true in config.json)";
    std::cerr << "[Modbus Read] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);
  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS READ COIL (FC 01)] Target Slave " << slave_id
            << ", Address 0x" << std::hex << address << std::dec << " (" << address << ") <<<" << std::endl;

  uint8_t dest[1] = {0};
  int rc = modbus_read_bits(g_modbus_ctx, address, 1, dest);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " for Coil " + std::to_string(address) + " (verify slave is powered, on same RF frequency/SF, and matching Slave ID)";
    } else {
      err = std::string("Failed to read coil ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Read] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  out_val = (dest[0] != 0);

  std::cout << ">>> [MODBUS READ COIL SUCCESS] Slave " << slave_id << " Coil 0x"
            << std::hex << address << std::dec << " = " << (out_val ? "1 (Open/ON)" : "0 (Closed/OFF)")
            << " <<<" << std::endl;

  {
    std::lock_guard<std::mutex> t_lock(g_telemetry_mutex);
    g_telemetry.slave_id = slave_id;
    if (address == 0) g_telemetry.valve1 = out_val ? 1 : 0;
    else if (address == 1) g_telemetry.valve2 = out_val ? 1 : 0;
    g_telemetry.last_update = get_current_time_str();
  }

  set_last_modbus_error("");
  return true;
}

bool modbus_read_discrete_input_val(int slave_id, int address, bool &out_val) {
  if (!is_modbus_ready()) {
    std::string err = "Modbus context not initialized (check if modbus_enabled is true in config.json)";
    std::cerr << "[Modbus Read] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);
  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS READ DISCRETE INPUT (FC 02)] Target Slave " << slave_id
            << ", Address 0x" << std::hex << address << std::dec << " (" << address << ") <<<" << std::endl;

  uint8_t dest[1] = {0};
  int rc = modbus_read_input_bits(g_modbus_ctx, address, 1, dest);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " for Discrete Input " + std::to_string(address);
    } else {
      err = std::string("Failed to read discrete input ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Read] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  out_val = (dest[0] != 0);

  std::cout << ">>> [MODBUS READ DISCRETE INPUT SUCCESS] Slave " << slave_id << " Input 0x"
            << std::hex << address << std::dec << " = " << (out_val ? "1 (Open)" : "0 (Closed)")
            << " <<<" << std::endl;

  {
    std::lock_guard<std::mutex> t_lock(g_telemetry_mutex);
    g_telemetry.slave_id = slave_id;
    if (address == 0) g_telemetry.valve1_status = out_val ? 1 : 0;
    else if (address == 1) g_telemetry.valve2_status = out_val ? 1 : 0;
    g_telemetry.last_update = get_current_time_str();
  }

  set_last_modbus_error("");
  return true;
}

bool modbus_read_input_reg_val(int slave_id, int address, uint16_t &out_val) {
  if (!is_modbus_ready()) {
    std::string err = "Modbus context not initialized (check if modbus_enabled is true in config.json)";
    std::cerr << "[Modbus Read] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);
  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS READ INPUT REGISTER (FC 04)] Target Slave " << slave_id
            << ", Address 0x" << std::hex << address << std::dec << " (" << address << ") <<<" << std::endl;

  uint16_t dest[1] = {0};
  int rc = modbus_read_input_registers(g_modbus_ctx, address, 1, dest);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " for Input Register " + std::to_string(address);
    } else {
      err = std::string("Failed to read input register ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Read] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  out_val = dest[0];

  std::cout << ">>> [MODBUS READ INPUT REGISTER SUCCESS] Slave " << slave_id << " Input Reg 0x"
            << std::hex << address << std::dec << " = " << out_val << " (0x"
            << std::hex << out_val << std::dec << ") <<<" << std::endl;

  {
    std::lock_guard<std::mutex> t_lock(g_telemetry_mutex);
    g_telemetry.slave_id = slave_id;
    if (address == 0) g_telemetry.sensor1 = out_val;
    else if (address == 1) g_telemetry.sensor2 = out_val;
    g_telemetry.last_update = get_current_time_str();
  }

  set_last_modbus_error("");
  return true;
}

bool modbus_read_holding_reg_val(int slave_id, int address, uint16_t &out_val) {
  if (!is_modbus_ready()) {
    std::string err = "Modbus context not initialized (check if modbus_enabled is true in config.json)";
    std::cerr << "[Modbus Read] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);
  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS READ HOLDING REGISTER (FC 03)] Target Slave " << slave_id
            << ", Address 0x" << std::hex << address << std::dec << " (" << address << ") <<<" << std::endl;

  uint16_t dest[1] = {0};
  int rc = modbus_read_registers(g_modbus_ctx, address, 1, dest);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " for Holding Register " + std::to_string(address);
    } else {
      err = std::string("Failed to read holding register ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Read] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  out_val = dest[0];

  std::cout << ">>> [MODBUS READ HOLDING REGISTER SUCCESS] Slave " << slave_id << " Holding Reg 0x"
            << std::hex << address << std::dec << " = " << out_val << " (0x"
            << std::hex << out_val << std::dec << ") <<<" << std::endl;

  {
    std::lock_guard<std::mutex> t_lock(g_telemetry_mutex);
    g_telemetry.slave_id = slave_id;
    switch (address) {
      case 0: g_telemetry.hw_id_high = out_val; break;
      case 1: g_telemetry.hw_id_low = out_val; break;
      case 2: g_telemetry.tx_count_high = out_val; break;
      case 3: g_telemetry.tx_count_low = out_val; break;
      case 4: g_telemetry.valve1_reg = out_val; break;
      case 5: g_telemetry.valve2_reg = out_val; break;
      case 6: g_telemetry.sensor1_reg = out_val; break;
      case 7: g_telemetry.sensor2_reg = out_val; break;
      case 8: g_telemetry.slave_id_reg = out_val; break;
    }
    g_telemetry.last_update = get_current_time_str();
  }

  set_last_modbus_error("");
  return true;
}

bool modbus_read_all(int slave_id) {
  std::cout << "\n========================================" << std::endl;
  std::cout << ">>> [MODBUS READ ALL] Reading full telemetry for Slave " << slave_id << " <<<" << std::endl;
  std::cout << "========================================" << std::endl;

  bool b_val = false;
  uint16_t reg_val = 0;

  // 1. Read Coils (Valves 1 & 2)
  modbus_read_coil_val(slave_id, 0, b_val);
  usleep(50000);
  modbus_read_coil_val(slave_id, 1, b_val);
  usleep(50000);

  // 2. Read Discrete Inputs (Valve Status 1 & 2)
  modbus_read_discrete_input_val(slave_id, 0, b_val);
  usleep(50000);
  modbus_read_discrete_input_val(slave_id, 1, b_val);
  usleep(50000);

  // 3. Read Input Registers (Sensors 1 & 2)
  modbus_read_input_reg_val(slave_id, 0, reg_val);
  usleep(50000);
  modbus_read_input_reg_val(slave_id, 1, reg_val);
  usleep(50000);

  // 4. Read Holding Registers 0..8
  for (int addr = 0; addr <= 8; addr++) {
    modbus_read_holding_reg_val(slave_id, addr, reg_val);
    usleep(50000);
  }

  std::cout << "\n>>> [MODBUS READ ALL COMPLETED] Slave " << slave_id << " <<<\n" << std::endl;
  return true;
}

// --- Modbus Write Functions Implementation ---

bool modbus_write_coil(int slave_id, int address, bool state) {
  if (!is_modbus_ready()) {
    std::string err = "Modbus context not initialized (check if modbus_enabled is true in config.json)";
    std::cerr << "[Modbus Write] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);

  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS WRITE COIL (FC 05)] Target Slave " << slave_id
            << ", Address 0x" << std::hex << address << std::dec << " (" << address << ") -> "
            << (state ? "1 (Open/ON)" : "0 (Closed/OFF)") << " <<<" << std::endl;

  int rc = modbus_write_bit(g_modbus_ctx, address, state ? 1 : 0);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " over LoRa within timeout (verify slave is powered, on same RF frequency/SF, and matching Slave ID)";
    } else {
      err = std::string("Failed to write Coil ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Write] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }

  std::cout << ">>> [MODBUS WRITE COIL SUCCESS] Slave " << slave_id
            << " Coil 0x" << std::hex << address << std::dec << " set to "
            << (state ? "1 (ON)" : "0 (OFF)") << " <<<" << std::endl;

  {
    std::lock_guard<std::mutex> t_lock(g_telemetry_mutex);
    g_telemetry.slave_id = slave_id;
    if (address == 0) g_telemetry.valve1 = state ? 1 : 0;
    else if (address == 1) g_telemetry.valve2 = state ? 1 : 0;
    g_telemetry.last_update = get_current_time_str();
  }

  set_last_modbus_error("");
  return true;
}

bool modbus_write_holding_register(int slave_id, int address, uint16_t value) {
  if (!is_modbus_ready()) {
    std::string err = "Modbus context not initialized (check if modbus_enabled is true in config.json)";
    std::cerr << "[Modbus Write] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);

  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS WRITE REGISTER (FC 06)] Target Slave " << slave_id
            << ", Address 0x" << std::hex << address << std::dec << " (" << address << ") -> "
            << value << " (0x" << std::hex << value << std::dec << ") <<<" << std::endl;

  int rc = modbus_write_register(g_modbus_ctx, address, value);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " over LoRa within timeout (verify slave is powered, on same RF frequency/SF, and matching Slave ID)";
    } else {
      err = std::string("Failed to write Holding Register ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Write] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }

  std::cout << ">>> [MODBUS WRITE REGISTER SUCCESS] Slave " << slave_id
            << " Register 0x" << std::hex << address << std::dec << " written with "
            << value << " (0x" << std::hex << value << std::dec << ") <<<" << std::endl;

  {
    std::lock_guard<std::mutex> t_lock(g_telemetry_mutex);
    g_telemetry.slave_id = slave_id;
    switch (address) {
      case 4: g_telemetry.valve1_reg = value; break;
      case 5: g_telemetry.valve2_reg = value; break;
      case 8: g_telemetry.slave_id_reg = value; break;
    }
    g_telemetry.last_update = get_current_time_str();
  }

  set_last_modbus_error("");
  return true;
}

bool modbus_write_multiple_coils(int slave_id, int address, int count, const uint8_t *values) {
  if (!is_modbus_ready() || values == nullptr || count <= 0) {
    std::string err = "Invalid parameters or context not initialized";
    std::cerr << "[Modbus Write] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);

  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS WRITE MULTIPLE COILS (FC 15)] Target Slave " << slave_id
            << ", Start Address 0x" << std::hex << address << std::dec << ", Count: " << count << " <<<" << std::endl;

  int rc = modbus_write_bits(g_modbus_ctx, address, count, values);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " over LoRa within timeout";
    } else {
      err = std::string("Failed to write multiple coils starting at ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Write] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }

  std::cout << ">>> [MODBUS WRITE MULTIPLE COILS SUCCESS] Slave " << slave_id
            << " wrote " << count << " coils starting at 0x" << std::hex << address << std::dec << " <<<" << std::endl;
  set_last_modbus_error("");
  return true;
}

bool modbus_write_multiple_holding_registers(int slave_id, int address, int count, const uint16_t *values) {
  if (!is_modbus_ready() || values == nullptr || count <= 0) {
    std::string err = "Invalid parameters or context not initialized";
    std::cerr << "[Modbus Write] Error: " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modbus_mutex);

  modbus_flush(g_modbus_ctx);
  modbus_set_slave(g_modbus_ctx, slave_id);

  std::cout << "\n>>> [MODBUS WRITE MULTIPLE REGISTERS (FC 16)] Target Slave " << slave_id
            << ", Start Address 0x" << std::hex << address << std::dec << ", Count: " << count << " <<<" << std::endl;

  int rc = modbus_write_registers(g_modbus_ctx, address, count, values);
  if (rc == -1) {
    std::string err;
    if (errno == ETIMEDOUT) {
      err = "Connection timed out: No response received from Slave " + std::to_string(slave_id) + " over LoRa within timeout";
    } else {
      err = std::string("Failed to write multiple registers starting at ") + std::to_string(address) + ": " + modbus_strerror(errno);
    }
    std::cerr << "[Modbus Write] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }

  std::cout << ">>> [MODBUS WRITE MULTIPLE REGISTERS SUCCESS] Slave " << slave_id
            << " wrote " << count << " registers starting at 0x" << std::hex << address << std::dec << " <<<" << std::endl;
  set_last_modbus_error("");
  return true;
}

bool modbus_write_valve(int slave_id, int valve_index, bool open) {
  if (valve_index < 1 || valve_index > 2) {
    std::string err = "Invalid valve index " + std::to_string(valve_index) + " (must be 1 or 2)";
    std::cerr << "[Modbus Write] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  int coil_addr = valve_index - 1; // Valve 1 = Coil 0, Valve 2 = Coil 1
  return modbus_write_coil(slave_id, coil_addr, open);
}

bool modbus_write_remote_slave_id(int current_slave_id, int new_slave_id) {
  if (new_slave_id < 1 || new_slave_id > 247) {
    std::string err = "Invalid new slave ID " + std::to_string(new_slave_id) + " (must be 1-247)";
    std::cerr << "[Modbus Write] " << err << std::endl;
    set_last_modbus_error(err);
    return false;
  }
  return modbus_write_holding_register(current_slave_id, 8, (uint16_t)new_slave_id);
}


