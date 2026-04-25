set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# If compiling natively on the OrangePi, these might not be strictly necessary, 
# but they are useful if cross-compiling from an x86 PC to Orange Pi.
# set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
# set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Set the platform variable for CMakeLists.txt
set(PLATFORM "ORANGEPI" CACHE STRING "Target Platform")
