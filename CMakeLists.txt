cmake_minimum_required(VERSION 3.22)
project(LoraProject VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Fetch the sx126x_driver
include(FetchContent)

FetchContent_Declare(
    sx126x_driver
    GIT_REPOSITORY https://github.com/Lora-net/sx126x_driver.git
)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(sx126x_driver json)

# Platform selection
set(PLATFORM "ORANGEPI" CACHE STRING "Target platform: ORANGEPI or STM32")
set_property(CACHE PLATFORM PROPERTY STRINGS ORANGEPI STM32)

if(PLATFORM STREQUAL "ORANGEPI")
    # Add executable for Orange Pi
    add_executable(lora_app main.cpp src/platform_opi/hal_orangepi.cpp src/common/lora_app.cpp src/common/config.cpp)

    # Include directories for the driver and wiringOP
    target_include_directories(lora_app PRIVATE 
        ${sx126x_driver_SOURCE_DIR}
        /usr/local/include
        src/common
    )

    # Link the sx126x_driver and wiringOP libraries
    target_link_libraries(lora_app 
        sx126x_driver
        wiringPi
        nlohmann_json::nlohmann_json
    )
    
    target_compile_definitions(lora_app PRIVATE PLATFORM_ORANGEPI)

elseif(PLATFORM STREQUAL "STM32")
    # Note: Cross-compiling for STM32 requires an appropriate toolchain file
    # and STM32 HAL libraries. This is a basic template structure.
    
    # Add executable for STM32
    add_executable(lora_app main.cpp src/platform_stm32/hal_stm32.cpp src/common/lora_app.cpp src/common/config.cpp)

    # Include directories for the driver
    target_include_directories(lora_app PRIVATE 
        ${sx126x_driver_SOURCE_DIR}
        src/common
        # Add STM32 specific include directories here
    )

    # Link the driver and JSON library
    target_link_libraries(lora_app 
        sx126x_driver
        nlohmann_json::nlohmann_json
        # Add STM32 HAL libraries here
    )
    
    target_compile_definitions(lora_app PRIVATE PLATFORM_STM32)
endif()