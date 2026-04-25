#include "hal/hal.h"
#include "main.h" // STM32 includes
#include <iostream>
#include <cstring>

extern SPI_HandleTypeDef hspi1;

// Global callback for DIO1 EXTI
static void (*dio1_isr)(void) = nullptr;

// We redefine usleep to HAL_Delay (Note: HAL_Delay is milliseconds, usleep is microseconds)
// For microsecond precision on STM32, we'd need a hardware timer. 
// For LoRa reset, 100us is needed, we can just use 1ms (HAL_Delay(1)).
static void delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

void gpio_export(int pin) {
    // Not needed for STM32
}

void gpio_direction(int pin, const char *direction) {
    // Configured via STM32CubeMX
}

int gpio_get_value(int pin) {
    switch(pin) {
        case RESET_PIN: return HAL_GPIO_ReadPin(RST_GPIO_Port, RST_Pin) == GPIO_PIN_SET ? 1 : 0;
        case CS_PIN:    return HAL_GPIO_ReadPin(NSS_GPIO_Port, NSS_Pin) == GPIO_PIN_SET ? 1 : 0;
        case DIO1_PIN:  return HAL_GPIO_ReadPin(DIO1_GPIO_Port, DIO1_Pin) == GPIO_PIN_SET ? 1 : 0;
        case BUSY_PIN:  return HAL_GPIO_ReadPin(BUSY_GPIO_Port, BUSY_Pin) == GPIO_PIN_SET ? 1 : 0;
        default: return 0;
    }
}

void gpio_set_value(int pin, int value) {
    GPIO_PinState state = value ? GPIO_PIN_SET : GPIO_PIN_RESET;
    switch(pin) {
        case RESET_PIN: HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, state); break;
        case CS_PIN:    HAL_GPIO_WritePin(NSS_GPIO_Port, NSS_Pin, state); break;
        // DIO4 is not strictly needed for basic LoRa rx/tx on all boards
    }
}

bool sx126x_is_busy() {
    return gpio_get_value(BUSY_PIN) == 1;
}

void set_rf_switch_tx() {
    // If your custom board has a TX/RX switch on a GPIO, set it here.
    // e.g., gpio_set_value(DIO4_PIN, 1);
}

void set_rf_switch_rx() {
    // If your custom board has a TX/RX switch on a GPIO, set it here.
}

int dio1_get_irq_status() {
    return gpio_get_value(DIO1_PIN);
}

bool spi_init() {
    // Handled by MX_SPI1_Init() in main.c
    return true;
}

bool spi_transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t length) {
    // Pull CS low
    gpio_set_value(CS_PIN, 0);

    HAL_StatusTypeDef status;
    if (tx_data && rx_data) {
        status = HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)tx_data, rx_data, length, HAL_MAX_DELAY);
    } else if (tx_data) {
        status = HAL_SPI_Transmit(&hspi1, (uint8_t*)tx_data, length, HAL_MAX_DELAY);
    } else if (rx_data) {
        status = HAL_SPI_Receive(&hspi1, rx_data, length, HAL_MAX_DELAY);
    } else {
        status = HAL_ERROR;
    }

    // Pull CS high
    gpio_set_value(CS_PIN, 1);

    return (status == HAL_OK);
}

void spi_close() {
    // Handled by STM32 HAL if needed
}

bool hal_setup() {
    // Setup is handled in main.c
    return true;
}

bool hal_attach_dio1_interrupt(void (*handler)(void)) {
    dio1_isr = handler;
    return true;
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == DIO1_Pin && dio1_isr) {
        dio1_isr();
    }
}

// SX126X HAL API Implementation
sx126x_hal_status_t sx126x_hal_reset(const void *context) {
    gpio_set_value(RESET_PIN, 0);
    delay_ms(1); // 1ms is > 100us required
    gpio_set_value(RESET_PIN, 1);
    delay_ms(5); // 5ms stabilization
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup(const void *context) {
    // Toggle NSS to wakeup
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_write(const void *context, const uint8_t *command, const uint16_t command_length, const uint8_t *data, const uint16_t data_length) {
    int timeout = 1000;
    while (sx126x_is_busy() && timeout > 0) {
        delay_ms(1);
        timeout--;
    }
    if (timeout == 0) return SX126X_HAL_STATUS_ERROR;

    size_t total_length = command_length + data_length;
    uint8_t *tx_buffer = new uint8_t[total_length];
    uint8_t *rx_buffer = new uint8_t[total_length];

    if (!tx_buffer || !rx_buffer) return SX126X_HAL_STATUS_ERROR;

    memcpy(tx_buffer, command, command_length);
    if (data && data_length > 0) {
        memcpy(tx_buffer + command_length, data, data_length);
    }

    if (!spi_transfer(tx_buffer, rx_buffer, total_length)) {
        delete[] tx_buffer; delete[] rx_buffer;
        return SX126X_HAL_STATUS_ERROR;
    }

    delete[] tx_buffer; delete[] rx_buffer;
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command, const uint16_t command_length, uint8_t *data, const uint16_t data_length) {
    int timeout = 1000;
    while (sx126x_is_busy() && timeout > 0) {
        delay_ms(1);
        timeout--;
    }
    if (timeout == 0) return SX126X_HAL_STATUS_ERROR;

    size_t total_length = command_length + data_length;
    uint8_t *tx_buffer = new uint8_t[total_length];
    uint8_t *rx_buffer = new uint8_t[total_length];

    if (!tx_buffer || !rx_buffer) return SX126X_HAL_STATUS_ERROR;

    memcpy(tx_buffer, command, command_length);
    memset(tx_buffer + command_length, 0x00, data_length);

    if (!spi_transfer(tx_buffer, rx_buffer, total_length)) {
        delete[] tx_buffer; delete[] rx_buffer;
        return SX126X_HAL_STATUS_ERROR;
    }

    if (data && data_length > 0) {
        memcpy(data, rx_buffer + command_length, data_length);
    }

    delete[] tx_buffer; delete[] rx_buffer;
    return SX126X_HAL_STATUS_OK;
}

