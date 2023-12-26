#include "board_2432s024_display.h"

#include <esp_lcd_ili9341.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* TAG = "2432s024_ili9341";

static SemaphoreHandle_t refresh_finish = NULL;

#define SELECTED_SPI_HOST SPI2_HOST
#define PIN_SCLK GPIO_NUM_14
#define PIN_MOSI GPIO_NUM_13
#define PIN_CS GPIO_NUM_15
#define PIN_DC GPIO_NUM_2
#define PIN_BACKLIGHT GPIO_NUM_27

#define HORIZONTAL_RESOLUTION 240
#define VERTICAL_RESOLUTION 320
#define BITS_PER_PIXEL 16
#define DRAW_BUFFER_HEIGHT 80

IRAM_ATTR static bool prv_on_color_trans_done(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

static bool prv_create_display(nb_display_t* display) {
    ESP_LOGI(TAG, "creating display");

    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(PIN_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    const spi_bus_config_t bus_config = ILI9341_PANEL_BUS_SPI_CONFIG(
        PIN_SCLK,
        PIN_MOSI,
        HORIZONTAL_RESOLUTION * DRAW_BUFFER_HEIGHT * (BITS_PER_PIXEL / 8)
    );

    if (spi_bus_initialize(SELECTED_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGD(TAG, "spi bus init failed");
        return false;
    }

    const esp_lcd_panel_io_spi_config_t panel_io_config = ILI9341_PANEL_IO_SPI_CONFIG(
        PIN_CS,
        PIN_DC,
        prv_on_color_trans_done,
        NULL
    );

    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SELECTED_SPI_HOST, &panel_io_config, &display->io_handle) != ESP_OK) {
        ESP_LOGD(TAG, "failed to create panel");
        return false;
    }

    ESP_LOGI(TAG, "install driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = BITS_PER_PIXEL,
    };
    if (esp_lcd_new_panel_ili9341(display->io_handle, &panel_config, &display->display_handle) != ESP_OK) {
        ESP_LOGD(TAG, "failed to create ili9341");
        return false;
    }


    if (esp_lcd_panel_reset(display->display_handle) != ESP_OK) {
        ESP_LOGD(TAG, "failed to reset panel");
        return false;
    }

    if (esp_lcd_panel_init(display->display_handle) != ESP_OK) {
        ESP_LOGD(TAG, "failed to init panel");
        return false;
    }

    if (esp_lcd_panel_mirror(display->display_handle, true, false) != ESP_OK) {
        ESP_LOGD(TAG, "failed to set panel to mirror");
        return false;
    }

    if (esp_lcd_panel_disp_on_off(display->display_handle, true) != ESP_OK) {
        ESP_LOGD(TAG, "failed to turn display on");
        return false;
    }

    if (gpio_set_level(PIN_BACKLIGHT, 1) != ESP_OK) {
        ESP_LOGD(TAG, "failed to turn backlight on");
        return false;
    }

    display->horizontal_resolution = HORIZONTAL_RESOLUTION;
    display->vertical_resolution = VERTICAL_RESOLUTION;
    display->draw_buffer_height = DRAW_BUFFER_HEIGHT;
    display->bits_per_pixel = BITS_PER_PIXEL;
    display->mirror_x = true;
    display->mirror_y = false;

    return true;
}

nb_display_driver_t board_2432s024_create_display_driver() {
    return (nb_display_driver_t) {
        .name = "ili9341_2432s024",
        .create_display = &prv_create_display
    };
}
