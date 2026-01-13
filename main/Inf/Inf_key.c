/* Inf_key.c */
#include "Inf_key.h"

#include "esp_log.h"
#include "driver/gpio.h" // 只要用到GPIO相关的宏通常都要加这个
#include "button_adc.h"

button_handle_t key1Handle;
button_handle_t key2Handle;

void key_init(void)
{

    // 1. 通用配置：ADC按键类型，长按/短按时间
    button_config_t btn_common_cfg = {

        .long_press_time = 1500, // 长按 1.5s
        .short_press_time = 50,  // 短按 50ms (配合硬件电容消抖)
    };

    // 按键的ADC配置
    button_adc_config_t btn_adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = ADC_CHANNEL_7, // GPIO8对应的引脚是ADC1通道7
        .button_index = 1             // 按键索引，用去区分同一通道的多个按键
    };
    // 创建按键1  (按键接地 -> 0V)
    btn_adc_cfg.min = 0;   // 0mV
    btn_adc_cfg.max = 200; // 200mV

    iot_button_new_adc_device(&btn_common_cfg, &btn_adc_cfg, &key1Handle);

    // 创建按键2  (1k + 1k 分压 -> 1.65V)
    btn_adc_cfg.button_index = 2; 
    btn_adc_cfg.min = 1450; // 1450mV
    btn_adc_cfg.max = 1850; // 1850mV

    iot_button_new_adc_device(&btn_common_cfg, &btn_adc_cfg, &key2Handle);
}

void Inf_key_RegisterKey1Callbacks(button_event_t event, button_cb_t cb, void *data)
{
    iot_button_register_cb(key1Handle, event, NULL, cb, data);
}

void Inf_key_RegisterKey2Callbacks(button_event_t event, button_cb_t cb, void *data)
{

    iot_button_register_cb(key2Handle, event, NULL, cb, data);
}