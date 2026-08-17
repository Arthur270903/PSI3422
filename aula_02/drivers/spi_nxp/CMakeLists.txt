zephyr_include_directories(${ZEPHYR_BASE}/drivers/spi)

target_sources_ifdef(CONFIG_SPI_NXP_KINETIS app PRIVATE spi_nxp_kinetis_l.c)