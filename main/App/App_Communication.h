#ifndef APP_COMMUNICATION_H
#define APP_COMMUNICATION_H

/* --- 1. 引入依赖的头文件 --- */
// 这样你在 main.c 里只要 include "App_Communication.h"，
// 就能间接引用到 WIFI 和 MQTT 的定义，非常方便
#include "Driver_WIFI.h"
#include "Driver_MQTT.h"
#include "Inf_key.h"    // 引入按键的驱动文件
#include "cJSON.h"
#include "Common_Config.h"
#include "Common_Debug.h"
#include "esp_log.h"
#include "Inf_Camera.h"


// 如果你以后要用按键重置功能，记得把按键的头文件也加进来
// #include "Int_key.h" 
// #include "Common_Debug.h" // 如果你有自定义的打印宏

/* --- 2. 公开函数声明 --- */

/**
 * @brief 通信模块初始化
 * @note  调用此函数后，将自动执行以下流程：
 * 1. 启动 WiFi (若未配网则开启蓝牙配网)
 * 2. WiFi 连接成功后 -> 自动启动 MQTT
 * 3. MQTT 连接成功后 -> 自动订阅 Topic
 */
void App_Communication_Init(void);

#endif /* APP_COMMUNICATION_H */