# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)

list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(aula_05_06)

target_sources(app PRIVATE
    sources/main.c
    sources/radio_link.c
    sources/car_control.c
    sources/calibration.c
)

add_subdirectory(lib/motor_control)
add_subdirectory(lib/nrf24l01)
add_subdirectory(lib/ultrasound)
add_subdirectory(lib/encoder)