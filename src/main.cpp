#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>
#include "sx126x.h"
#include "sx126x_hal.h"

// SPI device configuration
#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_SPEED 1000000  // 1 MHz
#define SPI_BITS_PER_WORD 8

// GPIO pins (adjust according to your hardware)
#define RESET_PIN 18  // GPIO pin for reset (using sysfs)
#define BUSY_PIN 20   // GPIO pin for busy status (using sysfs)
#define DIO1_PIN 16   // GPIO pin for interrupt - DIO1 (using sysfs)
#define DIO4_PIN 6    // GPIO pin for transmit enable - DIO4 (using sysfs)

// Global SPI file descriptor
static int spi_fd = -1;

// Helper function to export GPIO pin
void gpio_export(int pin) {
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d", pin);
    if (access(path, F_OK) == -1) {
        int fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd != -1) {
            char buffer[16];
            sprintf(buffer, "%d", pin);
            write(fd, buffer, strlen(buffer));
            close(fd);
        }
    }
}

// Helper function to set GPIO direction
void gpio_direction(int pin, const char* direction) {
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd != -1) {
        write(fd, direction, strlen(direction));
        close(fd);
    }
}

// Helper function to set GPIO value
void gpio_set_value(int pin, int value) {
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd != -1) {
        const char* val = value ? "1" : "0";
        write(fd, val, 1);
        close(fd);
    }
}

// Helper function to get GPIO value
int gpio_get_value(int pin) {
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_RDONLY);
    if (fd != -1) {
        char buffer[2];
        read(fd, buffer, 1);
        close(fd);
        return buffer[0] - '0';
    }
    return -1; // Error
}

// Helper function to check if SX126X is busy
bool sx126x_is_busy() {
    return gpio_get_value(BUSY_PIN) == 1;
}

// Helper function to enable transmit (set DIO4 high)
void transmit_enable() {
    gpio_set_value(DIO4_PIN, 1);
    std::cout << "HAL: Transmit enabled (DIO4 set high)" << std::endl;
}

// Helper function to disable transmit (set DIO4 low)
void transmit_disable() {
    gpio_set_value(DIO4_PIN, 0);
    std::cout << "HAL: Transmit disabled (DIO4 set low)" << std::endl;
}

// Helper function to check DIO1 interrupt status
int dio1_get_irq_status() {
    return gpio_get_value(DIO1_PIN);
}

// Initialize SPI device
bool spi_init() {
    if (spi_fd != -1) return true; // Already initialized
    
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

// SPI transfer function
bool spi_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
    if (spi_fd < 0) return false;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = (uint32_t)length,
        .speed_hz = SPI_SPEED,
        .bits_per_word = SPI_BITS_PER_WORD,
    };
    
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        std::cerr << "SPI transfer failed" << std::endl;
        return false;
    }
    
    return true;
}

// HAL function implementations
sx126x_hal_status_t sx126x_hal_reset(const void* context) {
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
    
    // Perform reset sequence according to SX126X datasheet:
    // NRESET pulse: low for min 100us, then high with 5ms stabilization time
    gpio_set_value(RESET_PIN, 0);
    usleep(100);  // Min 100us low pulse
    gpio_set_value(RESET_PIN, 1);
    usleep(5000); // 5ms stabilization time after reset
    
    std::cout << "HAL: Reset performed (NRESET pulsed)" << std::endl;
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup(const void* context) {
    // For SX126X, wakeup is done by toggling NSS (chip select)
    // The SPI transfer acts as a wakeup signal
    // This is handled implicitly by SPI operations
    std::cout << "HAL: Wakeup signaled via SPI" << std::endl;
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_write(const void* context, const uint8_t* command, const uint16_t command_length, const uint8_t* data, const uint16_t data_length) {
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
    uint8_t* tx_buffer = new uint8_t[total_length];
    uint8_t* rx_buffer = new uint8_t[total_length];
    
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

sx126x_hal_status_t sx126x_hal_read(const void* context, const uint8_t* command, const uint16_t command_length, uint8_t* data, const uint16_t data_length) {
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
    uint8_t* tx_buffer = new uint8_t[total_length];
    uint8_t* rx_buffer = new uint8_t[total_length];
    
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

int main() {
    std::cout << "Hello, World from CMake project with SX126X driver!" << std::endl;
    std::cout << "Initializing SX126X LoRa radio module..." << std::endl;
    
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
        std::cout << "  Command status: " << (int)chip_status.cmd_status << std::endl;
        std::cout << "  Chip mode: " << (int)chip_status.chip_mode << std::endl;
    } else {
        std::cerr << "Failed to get device status, status: " << (int)status << std::endl;
    }
    
    // Set the device to standby mode with external oscillator
    status = sx126x_set_standby(NULL, SX126X_STANDBY_CFG_XOSC);
    if (status == SX126X_STATUS_OK) {
        std::cout << "SX126X set to standby mode successfully!" << std::endl;
    } else {
        std::cerr << "Failed to set standby mode, status: " << (int)status << std::endl;
    }
    
    // ================================
    // Configure LoRa Parameters (from transmitter.py)
    // ================================
    
    // Set DIO2 as RF switch pin
    std::cout << "\nConfiguring RF switch (DIO2)..." << std::endl;
    status = sx126x_set_dio2_as_rf_sw_ctrl(NULL, true);
    if (status != SX126X_STATUS_OK) {
        std::cerr << "Failed to set DIO2 as RF switch, status: " << (int)status << std::endl;
    }
    
    // Set frequency to 868 MHz
    std::cout << "Setting frequency to 868 MHz..." << std::endl;
    uint32_t frequency = 868000000; // 868 MHz in Hz
    status = sx126x_set_rf_freq(NULL, frequency);
    if (status != SX126X_STATUS_OK) {
        std::cerr << "Failed to set frequency, status: " << (int)status << std::endl;
    }
    
    // Set TX power to +22 dBm (for SX1262)
    std::cout << "Setting TX power to +22 dBm..." << std::endl;
    int8_t tx_power = 22;
    sx126x_ramp_time_t ramp_time = SX126X_RAMP_200_US;  // Default ramp time
    status = sx126x_set_tx_params(NULL, tx_power, ramp_time);
    if (status != SX126X_STATUS_OK) {
        std::cerr << "Failed to set TX power, status: " << (int)status << std::endl;
    }
    
    // Configure modulation parameters
    // SF=7, BW=125kHz, CR=4/5
    std::cout << "Setting modulation parameters (SF=7, BW=125kHz, CR=4/5)..." << std::endl;
    sx126x_mod_params_lora_t mod_params = {
        .sf = SX126X_LORA_SF7,
        .bw = SX126X_LORA_BW_125,
        .cr = SX126X_LORA_CR_4_5,
        .ldro = 0  // Low DataRate Optimization disabled
    };
    status = sx126x_set_lora_mod_params(NULL, &mod_params);
    if (status != SX126X_STATUS_OK) {
        std::cerr << "Failed to set modulation parameters, status: " << (int)status << std::endl;
    }
    
    // Configure packet parameters
    // Explicit header, preamble=12, payload=15, CRC enabled
    std::cout << "Setting packet parameters (Explicit header, preamble=12, CRC=on)..." << std::endl;
    sx126x_pkt_params_lora_t pkt_params = {
        .preamble_len_in_symb = 12,
        .header_type = SX126X_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes = 15,
        .crc_is_on = true,
        .invert_iq_is_on = false
    };
    status = sx126x_set_lora_pkt_params(NULL, &pkt_params);
    if (status != SX126X_STATUS_OK) {
        std::cerr << "Failed to set packet parameters, status: " << (int)status << std::endl;
    }
    
    // Set sync word to 0x3444 (public LoRa network)
    std::cout << "Setting sync word to 0x3444..." << std::endl;
    status = sx126x_set_lora_sync_word(NULL, 0x44);  // Low byte of 0x3444
    if (status != SX126X_STATUS_OK) {
        std::cerr << "Failed to set sync word, status: " << (int)status << std::endl;
    }
    
    // Enable transmitter (set DIO4 high for RF module)
    transmit_enable();
    
    // ================================
    // Transmit Loop
    // ================================
    std::cout << "\n-- LoRa Transmitter --\n" << std::endl;
    
    const char* message = "HeLoRa World!";
    uint8_t counter = 0;
    
    while (true) {
        // Prepare payload: message + counter byte
        uint8_t payload[16];
        uint8_t msg_len = strlen(message);
        
        // Copy message to payload
        for (uint8_t i = 0; i < msg_len; i++) {
            payload[i] = (uint8_t)message[i];
        }
        // Add counter
        payload[msg_len] = counter;
        
        // Send packet using proper SX126X API
        // First write payload to buffer, then start transmission
        std::cout << "Transmitting: " << message << " " << (int)counter << std::endl;
        
        // Write payload to TX buffer
        status = sx126x_write_buffer(NULL, 0, payload, msg_len + 1);
        if (status != SX126X_STATUS_OK) {
            std::cerr << "Failed to write payload to buffer, status: " << (int)status << std::endl;
            continue;
        }
        
        // Start transmission with timeout (0 = no timeout, continuous until done)
        status = sx126x_set_tx(NULL, 0);
        if (status != SX126X_STATUS_OK) {
            std::cerr << "Failed to start transmission, status: " << (int)status << std::endl;
            continue;
        }
        
        // Wait for transmission to complete (check TX_DONE interrupt or just delay)
        usleep(100000); // 100ms delay to allow transmission
        
        // Print transmission info
        std::cout << "Transmission complete" << std::endl;
        
        // Increment counter
        counter = (counter + 1) % 256;
        
        // Don't overload the RF module - 5 second delay between transmissions
        std::cout << "Waiting 5 seconds before next transmission..." << std::endl;
        sleep(5);
    }
    
    // Cleanup (unreachable in current loop, but good practice)
    transmit_disable();
    if (spi_fd != -1) {
        close(spi_fd);
        spi_fd = -1;
    }
    
    return 0;
}