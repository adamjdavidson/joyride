// Adapted from Waveshare 03_LVGL_V8_Test (MIT, Copyright 2024 Waveshare).
#include <Arduino.h>
#include "lvgl.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "panel.h"
#include "panel_commands.h"

static lv_disp_draw_buf_t buffers;
static lv_disp_drv_t driver;
static esp_lcd_panel_handle_t panel;
static bool transferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *ctx) {
  lv_disp_flush_ready(static_cast<lv_disp_drv_t *>(ctx)); return false;
}
static void flush(lv_disp_drv_t *, const lv_area_t *area, lv_color_t *pixels) {
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels));
  // Release the buffer only when the actual DMA transfer completes.
}
static void roundArea(lv_disp_drv_t *, lv_area_t *area) {
  area->x1 &= ~1; area->y1 &= ~1; area->x2 |= 1; area->y2 |= 1;
}
static void update(lv_disp_drv_t *) {
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));
}
static void tick(void *) { lv_tick_inc(2); }
void panelBegin() {
  const spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(47, 18, 7, 48, 5, 536 * 240 * 2);
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_handle_t io;
  const esp_lcd_panel_io_spi_config_t config = SH8601_PANEL_IO_QSPI_CONFIG(6, transferDone, &driver);
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &config, &io));
  sh8601_vendor_config_t vendor = {};
  vendor.init_cmds = lcd_init_cmds; vendor.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]); vendor.flags.use_qspi_interface = 1;
  esp_lcd_panel_dev_config_t device = {};
  device.reset_gpio_num = 17; device.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  device.bits_per_pixel = 16; device.vendor_config = &vendor;
  ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &device, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel)); ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
  // Brighter presentation for the camera-focused palette.
  const uint8_t brightness = 230;
  ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, (0x02 << 24) | (0x51 << 8), &brightness, 1));
  lv_init();
  auto first = static_cast<lv_color_t *>(heap_caps_malloc(536 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA));
  auto second = static_cast<lv_color_t *>(heap_caps_malloc(536 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA));
  assert(first && second);
  lv_disp_draw_buf_init(&buffers, first, second, 536 * 40);
  lv_disp_drv_init(&driver); driver.hor_res = 536; driver.ver_res = 240;
  driver.flush_cb = flush; driver.rounder_cb = roundArea; driver.drv_update_cb = update;
  driver.draw_buf = &buffers; driver.user_data = panel; lv_disp_drv_register(&driver);
  const esp_timer_create_args_t timerArgs = {.callback = tick, .name = "initiative_tick"};
  esp_timer_handle_t timer;
  ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &timer)); ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 2000));
}
