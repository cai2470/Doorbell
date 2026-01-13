#ifndef __INF_LCD_H__
#define __INF_LCD_H__
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_log.h"

void Inf_Lcd_Init(void);

void Inf_Lcd_Refresh(uint16_t color);

#endif

