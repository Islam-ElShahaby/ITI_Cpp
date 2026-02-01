set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Replace these paths with your real paths from 'which aarch64-rpi3-linux-gnu-gcc'
set(CMAKE_C_COMPILER   /home/mrdark/x-tools/aarch64-rpi3-linux-gnu/bin/aarch64-rpi3-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /home/mrdark/x-tools/aarch64-rpi3-linux-gnu/bin/aarch64-rpi3-linux-gnu-g++)

# Point to where you installed Boost
set(CMAKE_FIND_ROOT_PATH  /home/mrdark/rpi_build/install/) 

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
