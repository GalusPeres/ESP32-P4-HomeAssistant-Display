#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>

esp_err_t waveshare_touch_lcd_8_init_pmic(i2c_master_bus_handle_t bus);
