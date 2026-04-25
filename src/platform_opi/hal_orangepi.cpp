#include "hal.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <wiringPi.h>

// Global SPI file descriptor
static int spi_fd = -1;

void gpio_export(int pin) {
  // Not needed with wiringOP - pins are accessed directly
}

void gpio_direction(int pin, const char *direction) {
  if (strcmp(direction, "out") == 0) {
    pinMode(pin, OUTPUT);
  } else if (strcmp(direction, "in") == 0) {
    pinMode(pin, INPUT);
  }
}

int gpio_get_value(int pin) { return digitalRead(pin); }

void gpio_set_value(int pin, int value) { digitalWrite(pin, value); }

bool sx126x_is_busy() { return gpio_get_value(BUSY_PIN) == 1; }

void set_rf_switch_tx() {
  gpio_set_value(DIO4_PIN, 1);
  std::cout << "HAL: RF Switch set to TX (DIO4 set high)" << std::endl;
}

void set_rf_switch_rx() {
  gpio_set_value(DIO4_PIN, 0);
  std::cout << "HAL: RF Switch set to RX (DIO4 set low)" << std::endl;
}

int dio1_get_irq_status() { return gpio_get_value(DIO1_PIN); }

bool spi_init() {
  if (spi_fd != -1)
    return true; // Already initialized

  spi_fd = open(SPI_DEVICE, O_RDWR);
  if (spi_fd < 0) {
    std::cerr << "Failed to open SPI device: " << SPI_DEVICE << std::endl;
    return false;
  }

  // Set SPI mode
  uint8_t mode = SPI_MODE_0;
  if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
    std::cerr << "Failed to set SPI mode" << std::endl;
    close(spi_fd);
    spi_fd = -1;
    return false;
  }

  // Set bits per word
  uint8_t bits = SPI_BITS_PER_WORD;
  if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
    std::cerr << "Failed to set bits per word" << std::endl;
    close(spi_fd);
    spi_fd = -1;
    return false;
  }

  // Set max speed
  uint32_t speed = SPI_SPEED;
  if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
    std::cerr << "Failed to set SPI speed" << std::endl;
    close(spi_fd);
    spi_fd = -1;
    return false;
  }

  return true;
}

bool spi_transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t length) {
  if (spi_fd < 0)
    return false;

  // Set CS low to start SPI transaction
  gpio_set_value(CS_PIN, 0);

  struct spi_ioc_transfer tr = {
      .tx_buf = (unsigned long)tx_data,
      .rx_buf = (unsigned long)rx_data,
      .len = (uint32_t)length,
      .speed_hz = SPI_SPEED,
      .bits_per_word = SPI_BITS_PER_WORD,
  };

  bool result = true;
  if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
    std::cerr << "SPI transfer failed" << std::endl;
    result = false;
  }

  // Set CS high to end SPI transaction
  gpio_set_value(CS_PIN, 1);

  return result;
}

void spi_close() {
  if (spi_fd != -1) {
    close(spi_fd);
    spi_fd = -1;
  }
}

bool hal_setup() {
  return wiringPiSetup() != -1;
}

bool hal_attach_dio1_interrupt(void (*handler)(void)) {
  return wiringPiISR(DIO1_PIN, INT_EDGE_RISING, handler) >= 0;
}

// HAL function implementations
sx126x_hal_status_t sx126x_hal_reset(const void *context) {
  // Initialize GPIO for reset pin
  gpio_export(RESET_PIN);
  gpio_direction(RESET_PIN, "out");

  // Initialize GPIO for busy pin
  gpio_export(BUSY_PIN);
  gpio_direction(BUSY_PIN, "in");

  // Initialize GPIO for DIO1 (interrupt)
  gpio_export(DIO1_PIN);
  gpio_direction(DIO1_PIN, "in");

  // Initialize GPIO for DIO4 (transmit enable)
  gpio_export(DIO4_PIN);
  gpio_direction(DIO4_PIN, "out");
  gpio_set_value(DIO4_PIN, 0); // Start with transmit disabled

  // Initialize GPIO for CS (chip select)
  gpio_export(CS_PIN);
  gpio_direction(CS_PIN, "out");
  gpio_set_value(CS_PIN, 1); // Start with CS high (inactive)

  // Perform reset sequence according to SX126X datasheet:
  // NRESET pulse: low for min 100us, then high with 5ms stabilization time
  gpio_set_value(RESET_PIN, 0);
  usleep(100); // Min 100us low pulse
  gpio_set_value(RESET_PIN, 1);
  usleep(5000); // 5ms stabilization time after reset

  std::cout << "HAL: Reset performed (NRESET pulsed)" << std::endl;
  return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup(const void *context) {
  // For SX126X, wakeup is done by toggling NSS (chip select)
  // The SPI transfer acts as a wakeup signal
  // This is handled implicitly by SPI operations
  std::cout << "HAL: Wakeup signaled via SPI" << std::endl;
  return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_write(const void *context,
                                     const uint8_t *command,
                                     const uint16_t command_length,
                                     const uint8_t *data,
                                     const uint16_t data_length) {
  if (!spi_init()) {
    return SX126X_HAL_STATUS_ERROR;
  }

  // Wait for device to not be busy
  int timeout = 1000; // 1 second timeout
  while (sx126x_is_busy() && timeout > 0) {
    usleep(1000); // 1ms
    timeout--;
  }
  if (timeout == 0) {
    std::cerr << "HAL: Timeout waiting for device to be ready" << std::endl;
    return SX126X_HAL_STATUS_ERROR;
  }

  // Combine command and data into a single buffer
  size_t total_length = command_length + data_length;
  uint8_t *tx_buffer = new uint8_t[total_length];
  uint8_t *rx_buffer = new uint8_t[total_length];

  if (!tx_buffer || !rx_buffer) {
    return SX126X_HAL_STATUS_ERROR;
  }

  // Copy command
  memcpy(tx_buffer, command, command_length);
  // Copy data if present
  if (data && data_length > 0) {
    memcpy(tx_buffer + command_length, data, data_length);
  }

  // Perform SPI transfer
  if (!spi_transfer(tx_buffer, rx_buffer, total_length)) {
    delete[] tx_buffer;
    delete[] rx_buffer;
    return SX126X_HAL_STATUS_ERROR;
  }

  delete[] tx_buffer;
  delete[] rx_buffer;

  return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command,
                                    const uint16_t command_length,
                                    uint8_t *data, const uint16_t data_length) {
  if (!spi_init()) {
    return SX126X_HAL_STATUS_ERROR;
  }

  // Wait for device to not be busy
  int timeout = 1000; // 1 second timeout
  while (sx126x_is_busy() && timeout > 0) {
    usleep(1000); // 1ms
    timeout--;
  }
  if (timeout == 0) {
    std::cerr << "HAL: Timeout waiting for device to be ready" << std::endl;
    return SX126X_HAL_STATUS_ERROR;
  }

  // For read operations, we send the command first, then read the response
  size_t total_length = command_length + data_length;
  uint8_t *tx_buffer = new uint8_t[total_length];
  uint8_t *rx_buffer = new uint8_t[total_length];

  if (!tx_buffer || !rx_buffer) {
    return SX126X_HAL_STATUS_ERROR;
  }

  // Copy command
  memcpy(tx_buffer, command, command_length);
  // Fill data part with dummy bytes (usually 0x00)
  memset(tx_buffer + command_length, 0x00, data_length);

  // Perform SPI transfer
  if (!spi_transfer(tx_buffer, rx_buffer, total_length)) {
    delete[] tx_buffer;
    delete[] rx_buffer;
    return SX126X_HAL_STATUS_ERROR;
  }

  // Copy received data (skip command echo)
  if (data && data_length > 0) {
    memcpy(data, rx_buffer + command_length, data_length);
  }

  delete[] tx_buffer;
  delete[] rx_buffer;

  return SX126X_HAL_STATUS_OK;
}
