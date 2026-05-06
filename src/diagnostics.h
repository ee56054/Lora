#ifndef LORA_DIAGNOSTICS_H
#define LORA_DIAGNOSTICS_H

#include <cstdint>

void show_configuration();
void read_chip_configuration();
void test_spi_communication();
void read_all_registers();

#endif // LORA_DIAGNOSTICS_H
