#ifndef DRIVER_WIFI_H
#define DRIVER_WIFI_H

#include <stdbool.h>

/* --- 1. 定义回调函数的类型 --- */
// 定义一个函数指针类型：没有参数，没有返回值
typedef void (*WifiConnectedCallback)(void);

/* --- 2. 函数声明 (菜单) --- */



// 初始化
void Driver_WIFI_Init(void);

// 检查是否已连接
bool Driver_WIFI_IsConnected(void);

// 【新增】注册连接成功的回调函数
void Driver_WIFI_RegisterConnectedCallback(WifiConnectedCallback cb);

// 【新增】重置配网信息 (擦除密码)
void Driver_WIFI_ResetProvisioning(void);

#endif