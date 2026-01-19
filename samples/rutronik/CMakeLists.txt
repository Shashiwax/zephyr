# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rutronik_app_s1v3g340)

# 1. Define where the driver code lives
set(DRIVER_DIR "../drivers/audio/RutEpsonDriver/s1v3g340/src")

# 2. Add ALL sources to the 'app' target directly
target_sources(app PRIVATE 
    src/main.c
    ${DRIVER_DIR}/spi_api.c 
    ${DRIVER_DIR}/isc_msgs.c
)

# 3. Add Include Paths so main.c can find "spi_api.h"
target_include_directories(app PRIVATE 
    src 
    ${DRIVER_DIR}
)