file(GLOB_RECURSE SOURCE_FILES Source/*.c*)

idf_component_register(
    SRCS ${SOURCE_FILES}
    INCLUDE_DIRS "Source"
    REQUIRES Tactility esp_lvgl_port esp_io_expander esp_io_expander_tca95xx_16bit BQ24295 XPT2046
)
