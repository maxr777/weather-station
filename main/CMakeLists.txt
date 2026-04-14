idf_component_register(
    SRCS "weather_station.c"
    INCLUDE_DIRS ""
    PRIV_REQUIRES spi_flash
    REQUIRES driver
)
