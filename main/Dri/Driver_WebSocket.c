#include "Driver_WebSocket.h" // 注意文件名大小写，根据实际情况调整
#include "esp_log.h"

static const char *TAG = "WS_DRI";
static WebsocketReceiveCallback receiveCb = NULL;

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket 已连接!");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket 已断开");
        break;
    case WEBSOCKET_EVENT_DATA:
        // 收到服务器发来的数据 (Opcode=0x2 表示二进制)
        if (data->op_code == 0x2 && data->data_len > 0)
        {
            if (receiveCb)
            {
                receiveCb((char *)data->data_ptr, data->data_len);
            }
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket 出错");
        break;
    default:
        break;
    }
}

// 创建客户端句柄
void Driver_Websocket_Create(char *uri, esp_websocket_client_handle_t *client, bool flag)
{
    if (uri == NULL || client == NULL)
        return;

    ESP_LOGI(TAG, "初始化 WS 客户端: %s", uri);

    const esp_websocket_client_config_t websocket_cfg = {
        .uri = uri,
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 3000,
        .network_timeout_ms = 3000,
        
    };

    *client = esp_websocket_client_init(&websocket_cfg);

    if (flag && *client)
    {
        esp_websocket_register_events(*client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)*client);
    }
}

// 开启连接
void Driver_Websocket_Open(esp_websocket_client_handle_t *client)
{
    if (client && *client)
    {
        esp_websocket_client_start(*client);
    }
}

// 关闭连接
void Driver_Websocket_Close(esp_websocket_client_handle_t *client)
{
    if (client && *client)
    {
        // 参数 1000 是超时时间 (portTICK_PERIOD_MS)，大约等待 1秒
        esp_websocket_client_close(*client, pdMS_TO_TICKS(1000));

        // 2. 无论服务器回不回，都强制停止本地客户端任务
        esp_websocket_client_stop(*client);
    }
}

// 检查连接状态
bool Driver_Websocket_IsConnected(esp_websocket_client_handle_t *client)
{
    if (client && *client)
    {
        return esp_websocket_client_is_connected(*client);
    }
    return false;
}

// 发送二进制数据
void Driver_Websocket_SendBin(esp_websocket_client_handle_t *client, char *datas, int len)
{
    if (client && *client && esp_websocket_client_is_connected(*client))
    {
        esp_websocket_client_send_bin(*client, datas, len, portMAX_DELAY);
    }
}

// 注册接收回调
void Driver_Websocket_RegisterReceiveCallback(WebsocketReceiveCallback cb)
{
    receiveCb = cb;
}