/* main.c */
#include "App_Communication.h"
#include "Inf_key.h" // 引入按键头文件
#include "nvs_flash.h"
#include "Inf_ES8311.h"
#include "App_Bell.h"

void app_main(void)
{
    // 1. NVS 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 2. 【关键】初始化按键驱动 (开启定时器扫描按键)
    // 如果不加这行，按键就是死的，没有任何反应
    key_init(); 
    App_Bell_Init();

    

    // 3. 初始化通信模块 (里面会注册按键回调)
    App_Communication_Init(); 

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}