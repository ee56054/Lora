# Lora CMake Project with SX126X Driver

A CMake project for Linux that integrates the SX126X LoRa transceiver driver with SPI communication and GPIO control using wiringOP library.

## Features

- SX126X driver integration from [Lora-net/sx126x_driver](https://github.com/Lora-net/sx126x_driver)
- Linux SPI communication using spidev interface
- GPIO control using wiringOP library for reset, busy, and interrupt pins
- Hardware Abstraction Layer (HAL) implementation
- Optimized for Orange Pi Zero 2W and other wiringOP-supported boards

## Hardware Requirements

- Linux system with SPI and GPIO support
- SX126X LoRa module connected via SPI
- GPIO connections as configured in main.cpp

### GPIO Pin Configuration

- **GPIO 18**: SX126X reset pin (output, wiringOP)
- **GPIO 20**: SX126X busy pin (input, wiringOP)
- **GPIO 16**: SX126X DIO1 interrupt pin (input, wiringOP)
- **GPIO 6**: SX126X DIO4 transmit enable pin (output, wiringOP)

### SPI Connection

- SPI Device: `/dev/spidev0.0` (configurable in main.cpp)
- SPI Speed: 1 MHz (configurable in main.cpp)

## Building

1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Generate build files:
   ```bash
   cmake ..
   ```

3. Build the project:
   ```bash
   make
   ```

## Running

**Important:** This program requires root privileges to access SPI and GPIO devices.

```bash
sudo ./lora_app
```

## Configuration

### SPI Settings
Edit the following constants in `src/main.cpp`:
- `SPI_DEVICE`: SPI device path (default: "/dev/spidev0.0")
- `SPI_SPEED`: SPI clock speed in Hz (default: 1000000)
- `SPI_BITS_PER_WORD`: Bits per word (default: 8)

### GPIO Settings
- `RESET_PIN`: GPIO pin number for reset (default: 18)
- `BUSY_PIN`: GPIO pin number for busy status (default: 20)
- `DIO1_PIN`: GPIO pin number for DIO1 interrupt (default: 16)
- `DIO4_PIN`: GPIO pin number for DIO4 transmit enable (default: 6)

### LoRa Parameters
The web UI supports selecting the following LoRa parameters:
- Spreading Factor: `SF5`, `SF6`, `SF7`, `SF8`, `SF9`, `SF10`, `SF11`, `SF12`
- Bandwidth (kHz): `7.81`, `10.42`, `15.63`, `20.83`, `31.25`, `41.67`, `62.5`, `125`, `250`, `500`
- Coding Rate: `4/5`, `4/6`, `4/7`, `4/8`
- TX Power: numeric value in dBm, range `-9` to `+22`

The default configuration values are `915000000` Hz (915 MHz) frequency, `22` dBm TX power, `SF9` spreading factor, `125` kHz bandwidth, `4/6` coding rate, preamble length `8`, and RX timeout `5000` ms.

## Dependencies

- CMake 3.22 or higher
- C++11 compatible compiler
- Linux kernel with SPI and GPIO support
- spidev kernel module loaded
- wiringOP library installed in `/usr/local` (for Orange Pi and other supported boards)

## Notes

- The HAL functions use wiringOP library for GPIO control, which provides better performance than sysfs.
- This project is optimized for Orange Pi Zero 2W but should work on any Linux board with wiringOP support.
- Error handling is basic; enhance as needed for your application.
- The driver initialization includes a basic standby mode test.
   ```

4. Run the executable:
   ```
   ./lora_app
   ```

## Requirements

- CMake 3.10 or higher
- C++ compiler (g++ on Linux)