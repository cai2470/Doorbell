#ifndef DRIVER_MQTT_H
#define DRIVER_MQTT_H

// 必须引用这个，不然编译器不认识 esp_mqtt_client_handle_t
#include "mqtt_client.h" 

/* --- 1. 定义回调函数的类型 --- */
// 连接成功回调
typedef void (*MqttConnectSuccessCallback)(void);
// 接收数据回调
typedef void (*MqttReceiveCallback)(char *data, int len);

/* --- 2. 函数声明 (菜单) --- */

// 【新增】初始化 MQTT
void Driver_MQTT_Init(void);

// 【新增】注册回调函数
void Driver_MQTT_RegisterCallback(MqttConnectSuccessCallback on_connect, MqttReceiveCallback on_receive);

// 【新增】订阅主题
void Driver_MQTT_Subscribe(const char *topic, int qos);

// 【新增】发布消息 (送你的，以后会用)
void Driver_MQTT_Publish(const char *topic, const char *data);

#endif