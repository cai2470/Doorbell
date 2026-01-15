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
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    App_Communication_Init();
    key_init();
    App_Bell_Init();
}