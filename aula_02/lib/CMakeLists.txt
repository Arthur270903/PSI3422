zephyr_include_directories(${ZEPHYR_BASE}/lib)

target_sources(app PRIVATE nrf24l01.c)