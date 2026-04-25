set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Set the cross-compilers
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# Common flags for STM32 (Example for STM32L4, adjust as needed)
# set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
# set(CMAKE_C_FLAGS "${CPU_FLAGS} -Os -ffunction-sections -fdata-sections" CACHE INTERNAL "")
# set(CMAKE_CXX_FLAGS "${CPU_FLAGS} -Os -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti" CACHE INTERNAL "")
# set(CMAKE_EXE_LINKER_FLAGS "${CPU_FLAGS} -Wl,--gc-sections --specs=nano.specs" CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Set the platform variable for CMakeLists.txt
set(PLATFORM "STM32" CACHE STRING "Target Platform")
