#ifndef LORA_HAL_H
#define LORA_HAL_H

#include <cstdint>
#include <cstddef>
#include "sx126x_hal.h"

// SPI device configuration
#define SPI_DEVICE "/dev/spidev1.0"
#define SPI_SPEED 2000000 // 2 MHz
#define SPI_BITS_PER_WORD 8

// GPIO pins (adjust according to your hardware)
#define DIO4_PIN 20 // GPIO pin for transmit enable - DIO4 (using wiringOP)
#define RESET_PIN 6 // GPIO pin for reset (using wiringOP)
#define DIO1_PIN 24 // GPIO pin for interrupt - DIO1 (using wiringOP)
#define BUSY_PIN 26 // GPIO pin for busy status (using wiringOP)
#define CS_PIN 27   // GPIO pin for chip select (using wiringOP)

// GPIO helper functions
void gpio_export(int pin);
void gpio_direction(int pin, const char *direction);
int gpio_get_value(int pin);
void gpio_set_value(int pin, int value);

// HAL helper functions
bool sx126x_is_busy();
void set_rf_switch_tx();
void set_rf_switch_rx();
int dio1_get_irq_status();

// SPI functions
bool spi_init();
bool spi_transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t length);
void spi_close();

// Setup and Interrupts
bool hal_setup();
bool hal_attach_dio1_interrupt(void (*handler)(void));

#endif // LORA_HAL_H
