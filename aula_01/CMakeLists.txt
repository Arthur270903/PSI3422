# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)

list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# fix error kl25z
add_compile_options(-include ${CMAKE_CURRENT_SOURCE_DIR}/tpm_fix.h)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(aula_01)

target_sources(
    app PRIVATE 
    sources/main.c
    lib/tpm_dt/tpm_dt.c
    lib/ultrasound/ultrasound.c
)

target_include_directories(
    app PRIVATE
    lib/tpm_dt
    lib/ultrasound
)