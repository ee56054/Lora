import os

with open('src/lora_app.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# find the start and end of diagnostics functions
start_idx = -1
end_idx = -1
for i, line in enumerate(lines):
    if line.startswith('// Function to display all configuration settings'):
        start_idx = i
    if line.startswith('#include "lora_app.h"'):
        end_idx = i

if start_idx != -1 and end_idx != -1:
    diagnostics_lines = lines[start_idx:end_idx]
    lora_app_lines = lines[:start_idx] + ['#include "diagnostics.h"\n\n'] + lines[end_idx:]

    with open('src/diagnostics.cpp', 'w', encoding='utf-8') as f:
        f.write('#include "diagnostics.h"\n')
        f.write('#include "hal.h"\n')
        f.write('#include "sx126x.h"\n')
        f.write('#include "sx126x_hal.h"\n')
        f.write('#include "config.h"\n')
        f.write('#include <iostream>\n')
        f.write('#include <string>\n')
        f.write('#include <unistd.h>\n')
        f.write('\n')
        f.write('// LoRa configuration constants\n')
        f.write('#define FREQUENCY g_config.frequency\n')
        f.write('#define TX_POWER g_config.tx_power\n')
        f.write('#define SPREADING_FACTOR g_config.spreading_factor\n')
        f.write('#define BANDWIDTH g_config.bandwidth\n')
        f.write('#define CODING_RATE g_config.coding_rate\n')
        f.write('#define PREAMBLE_LENGTH g_config.preamble_length\n')
        f.write('#define RX_TIMEOUT g_config.rx_timeout\n')
        f.write('\n')
        f.writelines(diagnostics_lines)

    with open('src/lora_app.cpp', 'w', encoding='utf-8') as f:
        f.writelines(lora_app_lines)

    with open('src/diagnostics.h', 'w', encoding='utf-8') as f:
        f.write('#ifndef LORA_DIAGNOSTICS_H\n')
        f.write('#define LORA_DIAGNOSTICS_H\n\n')
        f.write('#include <cstdint>\n\n')
        f.write('void show_configuration();\n')
        f.write('void read_chip_configuration();\n')
        f.write('void test_spi_communication();\n')
        f.write('void read_all_registers();\n\n')
        f.write('#endif // LORA_DIAGNOSTICS_H\n')
    
    print("Refactoring completed.")
else:
    print(f"Could not find indices: start_idx={start_idx}, end_idx={end_idx}")

