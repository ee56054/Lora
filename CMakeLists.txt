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

# Platform selection
set(PLATFORM "ORANGEPI" CACHE STRING "Target platform: ORANGEPI or STM32")
set_property(CACHE PLATFORM PROPERTY STRINGS ORANGEPI STM32)

if(PLATFORM STREQUAL "ORANGEPI")
    FetchContent_Declare(json
      URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(sx126x_driver json)
elseif(PLATFORM STREQUAL "STM32")
    FetchContent_Declare(arduinojson
      GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
      GIT_TAG v6.21.3
    )
    FetchContent_MakeAvailable(sx126x_driver arduinojson)
endif()

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
    # Enable CMake support for ASM and C languages (required by CubeMX)
    enable_language(C ASM)

    # Tell STM32CubeMX cmake files that our target is lora_app
    set(CMAKE_PROJECT_NAME lora_app)
    
    # Add executable for STM32 (excluding root main.cpp since CubeMX provides main.c)
    add_executable(lora_app src/platform_stm32/hal_stm32.cpp src/common/lora_app.cpp src/common/config.cpp)

    # Include STM32CubeMX subdirectory which attaches sources to lora_app
    add_subdirectory(src/platform_stm32/cmake/stm32cubemx)

    # Include directories for the driver
    target_include_directories(lora_app PRIVATE 
        ${sx126x_driver_SOURCE_DIR}
        src/common
    )

    # Link the driver and JSON library
    target_link_libraries(lora_app 
        sx126x_driver
        ArduinoJson
    )
    
    target_compile_definitions(lora_app PRIVATE PLATFORM_STM32)
endif()