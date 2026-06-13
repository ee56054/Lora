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

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)

FetchContent_Declare(
    libmodbus
    GIT_REPOSITORY https://github.com/stephane/libmodbus.git
    GIT_TAG v3.1.12
)

FetchContent_MakeAvailable(sx126x_driver json httplib)

FetchContent_GetProperties(libmodbus)
if(NOT libmodbus_POPULATED)
    FetchContent_Populate(libmodbus)

    set(LIBMODBUS_VERSION_MAJOR 3)
    set(LIBMODBUS_VERSION_MINOR 1)
    set(LIBMODBUS_VERSION_MICRO 12)
    set(LIBMODBUS_VERSION 3.1.12)

    configure_file(
        ${libmodbus_SOURCE_DIR}/src/modbus-version.h.in
        ${libmodbus_BINARY_DIR}/modbus-version.h
        @ONLY
    )

    set(LIBMODBUS_CONFIG_H "${libmodbus_BINARY_DIR}/config.h")
    file(WRITE "${LIBMODBUS_CONFIG_H}" "/* Auto-generated config.h for libmodbus */\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#define HAVE_GAI_STRERROR 1\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#define HAVE_NETINET_IN_H 1\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#define HAVE_NETINET_IP_H 1\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#define HAVE_DECL_TIOCM_RTS 1\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#define HAVE_DECL_TIOCSRS485 1\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#ifndef IPTOS_LOWDELAY\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#define IPTOS_LOWDELAY 0x10\n")
    file(APPEND "${LIBMODBUS_CONFIG_H}" "#endif\n")

    add_library(libmodbus STATIC
        ${libmodbus_SOURCE_DIR}/src/modbus.c
        ${libmodbus_SOURCE_DIR}/src/modbus-data.c
        ${libmodbus_SOURCE_DIR}/src/modbus-rtu.c
        ${libmodbus_SOURCE_DIR}/src/modbus-tcp.c
    )
    
    target_include_directories(libmodbus PUBLIC
        ${libmodbus_SOURCE_DIR}/src
        ${libmodbus_BINARY_DIR}
    )
endif()

# Add executable
add_executable(lora_app src/main.cpp src/hal.cpp src/lora_app.cpp src/config.cpp src/diagnostics.cpp src/modbus_lora.cpp src/web_server.cpp)

# Include directories for the driver and wiringOP
target_include_directories(lora_app PRIVATE 
    ${sx126x_driver_SOURCE_DIR}
    /usr/local/include
)

# Link the sx126x_driver and wiringOP libraries
target_link_libraries(lora_app 
    sx126x_driver
    wiringPi
    nlohmann_json::nlohmann_json
    libmodbus
    httplib::httplib
)