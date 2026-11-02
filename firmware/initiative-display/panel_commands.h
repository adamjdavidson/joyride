// From Waveshare 03_LVGL_V8_Test, MIT, Copyright 2024 Waveshare.
#pragma once
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {

  {0x11, (uint8_t []){0x00}, 0, 120},
   // {0x44, (uint8_t []){0x01, 0xD1}, 2, 0},
   // {0x35, (uint8_t []){0x00}, 1, 0},
  {0x36, (uint8_t []){0xF0}, 1, 0},
  {0x3A, (uint8_t []){0x55}, 1, 0},  //16bits-RGB565
  {0x2A, (uint8_t []){0x00,0x00,0x02,0x17}, 4, 0},
  {0x2B, (uint8_t []){0x00,0x00,0x00,0xEF}, 4, 0},
  {0x51, (uint8_t []){0x00}, 1, 10},
  {0x29, (uint8_t []){0x00}, 0, 10},
  {0x51, (uint8_t []){0xFF}, 1, 0},
};
