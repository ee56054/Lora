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

FetchContent_MakeAvailable(sx126x_driver)

# Add executable
add_executable(lora_app src/main.cpp)

# Include directories for the driver
target_include_directories(lora_app PRIVATE ${sx126x_driver_SOURCE_DIR})

# Link the sx126x_driver library
target_link_libraries(lora_app sx126x_driver)