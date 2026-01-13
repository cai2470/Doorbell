#ifndef __DRIVER_WEBSOCKET_H__
#define __DRIVER_WEBSOCKET_H__

#include <stdbool.h>
#include "esp_websocket_client.h" // 必须包含这个，否则不认识 esp_websocket_client_handle_t

// 定义接收数据的回调函数类型
typedef void (*WebsocketReceiveCallback)(char *data, int len);

/**
 * @brief 创建 WebSocket 客户端句柄
 * * @param uri 连接地址 (例如: ws://192.168.1.100:8000/ws/audio)
 * @param client [输出] 客户端句柄指针
 * @param flag 是否自动注册默认事件监听 (建议 true)
 */
void Driver_Websocket_Create(char *uri, esp_websocket_client_handle_t *client, bool flag);

/**
 * @brief 开启连接 (启动客户端)
 * * @param client 客户端句柄
 */
void Driver_Websocket_Open(esp_websocket_client_handle_t *client);

/**
 * @brief 关闭连接
 * * @param client 客户端句柄
 */
void Driver_Websocket_Close(esp_websocket_client_handle_t *client);

/**
 * @brief 检查是否已连接
 * * @param client 客户端句柄
 * @return true 已连接, false 未连接
 */
bool Driver_Websocket_IsConnected(esp_websocket_client_handle_t *client);

/**
 * @brief 发送二进制数据
 * * @param client 客户端句柄
 * @param datas 数据指针
 * @param len 数据长度
 */
void Driver_Websocket_SendBin(esp_websocket_client_handle_t *client, char *datas, int len);

/**
 * @brief 注册接收数据的回调函数 (全局)
 * 当任何一个客户端收到二进制数据时，都会调用这个回调
 * * @param cb 回调函数
 */
void Driver_Websocket_RegisterReceiveCallback(WebsocketReceiveCallback cb);

#endif /* __DRIVER_WEBSOCKET_H__ */