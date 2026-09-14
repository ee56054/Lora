# LoRa SX126X Transceiver & Modbus RTU Manager

A high-performance C++ application for Linux (optimized for Orange Pi Zero 2W and wiringOP-supported boards) integrating the Semtech SX126X LoRa transceiver driver with SPI communication, GPIO interrupt control, **Modbus RTU over LoRa** (with standard Modbus CRC-16), and an **embedded Web Dashboard & REST API**.

---

## Features

- **SX126X LoRa Driver**: Native integration with [Lora-net/sx126x_driver](https://github.com/Lora-net/sx126x_driver).
- **Modbus RTU over LoRa**:
  - Full support for standard Modbus RTU frames with standard CRC-16 calculation and validation.
  - Seamless libmodbus backend (`modbus_lora`) bridging serial Modbus frames to LoRa wireless packets.
  - Continuous RX listening mode with non-blocking DIO1 interrupt handling.
  - On-demand single reads, bulk reads (`read_all`), single writes, and multi-writes.
- **Embedded Web Server & REST API**:
  - Built-in HTTP server running on port `8080` powered by `cpp-httplib` and `nlohmann/json`.
  - Modern dark-themed responsive Web UI.
  - **Live Telemetry Dashboard**: real-time monitoring and individual `[Read]` buttons for all 15 mapped Modbus points, with 2.5-second background UI auto-sync.
  - **Direct Controls**: quick buttons for valve switching and holding register configuration.
  - **Hardware Config Editor**: live editing of frequency, TX power, SF, bandwidth, and coding rate with hot-reload.
- **Hardware Abstraction Layer (HAL)**:
  - Linux `spidev` interface for high-speed SPI transfers.
  - GPIO control using `wiringOP` for reset, busy, DIO1 interrupt, and DIO4 transmit enable pins.

---

## Hardware Configuration & Wiring

Optimized for **Orange Pi Zero 2W** (wiringPi / wiringOP pin mapping):

### GPIO Pinout
| Signal | Orange Pi (wiringOP Pin) | SX126X Module Pin | Direction | Function |
| :--- | :---: | :---: | :---: | :--- |
| **RESET** | `GPIO 18` | `NRESET` | Output | Module hardware reset |
| **BUSY** | `GPIO 20` | `BUSY` | Input | Chip busy status monitor |
| **DIO1** | `GPIO 16` | `DIO1` | Input | Packet RX / TX interrupt |
| **DIO4 / TXEN** | `GPIO 6` | `TXEN` | Output | Transmit RF switch enable |

### SPI Interface
- **Device**: `/dev/spidev0.0`
- **Clock Speed**: 1 MHz (1,000,000 Hz)
- **Bits Per Word**: 8-bit, SPI Mode 0

---

## Modbus Address Map

**Default Slave ID:** `1` (Configurable 1–247 via Register 8)

### Coils (0x / Read-Write)
| Address | Name | Function Codes | Description |
| :---: | :--- | :---: | :--- |
| `0x0000` (0) | **Valve 1** | `01`, `05`, `15` | `0` = Closed / OFF, `1` = Open / ON |
| `0x0001` (1) | **Valve 2** | `01`, `05`, `15` | `0` = Closed / OFF, `1` = Open / ON |

### Discrete Inputs (1x / Read-Only)
| Address | Name | Function Codes | Description |
| :---: | :--- | :---: | :--- |
| `0x0000` (0) | **Valve 1 Status** | `02` | `0` = Closed, `1` = Open |
| `0x0001` (1) | **Valve 2 Status** | `02` | `0` = Closed, `1` = Open |

### Input Registers (3x / Read-Only)
| Address | Name | Function Codes | Description |
| :---: | :--- | :---: | :--- |
| `0x0000` (0) | **Sensor 1** | `04` | 16-bit sensor reading (e.g. Temperature `250` = 25.0 °C) |
| `0x0001` (1) | **Sensor 2** | `04` | 16-bit sensor reading (e.g. Pressure `1013` = 1013 hPa) |

### Holding Registers (4x / Read-Write)
| Address | Name | Function Codes | Description |
| :---: | :--- | :---: | :--- |
| `0x0000` (0) | **HW ID (High)** | `03`, `06`, `16` | Hardware ID High Word (32-bit combined with 0x0001) |
| `0x0001` (1) | **HW ID (Low)** | `03`, `06`, `16` | Hardware ID Low Word |
| `0x0002` (2) | **TX Count (High)** | `03`, `06`, `16` | TX Counter High Word (32-bit combined with 0x0003) |
| `0x0003` (3) | **TX Count (Low)** | `03`, `06`, `16` | TX Counter Low Word |
| `0x0004` (4) | **Valve 1 Mirror** | `03`, `06`, `16` | Mirror of Valve 1 state (`0` or `1`) |
| `0x0005` (5) | **Valve 2 Mirror** | `03`, `06`, `16` | Mirror of Valve 2 state (`0` or `1`) |
| `0x0006` (6) | **Sensor 1 Mirror** | `03`, `06`, `16` | Mirror of Sensor 1 reading |
| `0x0007` (7) | **Sensor 2 Mirror** | `03`, `06`, `16` | Mirror of Sensor 2 reading |
| `0x0008` (8) | **Slave ID** | `03`, `06`, `16` | Configurable Modbus Slave Address (`1` to `247`) |

---

## Configuration (`config.json`)

The application automatically reads and monitors `config.json`. When changes are saved via the Web UI or edited on disk, the application hot-reloads automatically.

```json
{
    "frequency": 915000000,
    "tx_power": 22,
    "spreading_factor": "SF9",
    "bandwidth": "125",
    "coding_rate": "4/6",
    "preamble_length": 8,
    "rx_timeout": 5000,
    "modbus_enabled": true,
    "modbus_slave_id": 1,
    "modbus_address_devices": [1]
}
```

### Supported Parameters
- **Frequency**: RF frequency in Hz (e.g. `915000000` for 915 MHz, `868000000` for 868 MHz, `433000000` for 433 MHz).
- **TX Power**: `-9` to `+22` dBm.
- **Spreading Factor**: `SF5`, `SF6`, `SF7`, `SF8`, `SF9`, `SF10`, `SF11`, `SF12`.
- **Bandwidth**: `7.81`, `10.42`, `15.63`, `20.83`, `31.25`, `41.67`, `62.5`, `125`, `250`, `500` (kHz).
- **Coding Rate**: `4/5`, `4/6`, `4/7`, `4/8`.
- **Modbus Protocol**: `modbus_enabled` (`true` / `false`), local `modbus_slave_id`, target `modbus_address_devices`.

---

## Building and Running

### Prerequisites
- Linux OS with SPI and GPIO enabled
- `spidev` kernel module loaded
- `wiringOP` library installed in `/usr/local`
- CMake 3.22+ and a C++11 compliant compiler (`g++`)

### Build
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Run
Root privileges are required for SPI and GPIO device access:
```bash
sudo ./lora_app
```

Once started:
- Transceiver enters **continuous RX listening mode**.
- The embedded Web UI is available at:
  ```
  http://<orange-pi-ip>:8080
  ```

---

## Web REST API Reference

### Configuration

#### `GET /api/config`
Retrieve the current LoRa & Modbus configuration.

#### `POST /api/config`
Save new configuration parameters to `config.json` and trigger automatic hot-reload.

---

### Modbus Operations

#### `GET /api/modbus/telemetry`
Returns the latest cached values of all telemetry parameters.
```json
{
  "slave_id": 1,
  "valve1": 1,
  "valve2": 0,
  "valve1_status": 1,
  "valve2_status": 0,
  "sensor1": 250,
  "sensor2": 1013,
  "hw_id_high": 18,
  "hw_id_low": 52,
  "tx_count_high": 0,
  "tx_count_low": 42,
  "valve1_reg": 1,
  "valve2_reg": 0,
  "sensor1_reg": 250,
  "sensor2_reg": 1013,
  "slave_id_reg": 1,
  "last_update": "2026-09-14 20:30:00"
}
```

#### `POST /api/modbus/read`
Reads a single coil, discrete input, input register, or holding register on demand.
```json
{
  "slave_id": 1,
  "type": "coil",
  "address": 0
}
```
*Valid `type` values: `"coil"`, `"discrete_input"`, `"input_register"`, `"holding_register"`*

**Response:**
```json
{
  "status": "ok",
  "value": 1
}
```

#### `POST /api/modbus/read_all`
Queries all 15 addresses for the specified target slave ID.
```json
{
  "slave_id": 1
}
```

**Response:**
```json
{
  "status": "ok",
  "telemetry": { ... }
}
```

#### `POST /api/modbus/write_valve`
Convenience endpoint to control Valve 1 or Valve 2 via Coil FC 05.
```json
{
  "slave_id": 1,
  "valve": 1,
  "open": true
}
```

#### `POST /api/modbus/write_coil`
Write any single coil (FC 05).
```json
{
  "slave_id": 1,
  "address": 0,
  "value": true
}
```

#### `POST /api/modbus/write_register`
Write a 16-bit value to any holding register (FC 06).
```json
{
  "slave_id": 1,
  "address": 8,
  "value": 2
}
```

---

## Architecture & Code Structure

```
├── CMakeLists.txt        # CMake build script (SX126X, libmodbus, httplib, json)
├── config.json           # Runtime configuration file
├── README.md             # Project documentation
└── src/
    ├── main.cpp          # App entry point, web server thread & run loop
    ├── config.h / .cpp   # JSON configuration manager & validation
    ├── hal.h / .cpp      # SX126X Hardware Abstraction Layer (SPI / wiringOP GPIO)
    ├── lora_app.h / .cpp # Transceiver orchestration and RX/TX routines
    ├── modbus_app.h / .cpp # Modbus app logic, on-demand read/write, telemetry cache
    ├── modbus_lora.h / .cpp # libmodbus custom backend for LoRa transport & CRC-16
    ├── diagnostics.h / .cpp # Radio status and packet inspection utilities
    └── web_server.h / .cpp  # Embedded HTTP server, REST endpoints & HTML UI
```

---

## License

This project incorporates the SX126X driver from Semtech and libmodbus under their respective licenses.