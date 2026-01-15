#include "Driver_MQTT.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MQTT_DRI";


// 1. 协议头改成 ws://
// 2. 端口改成你刚才在 conf 里配置的 8083
// 3. 后面通常要加 /mqtt 路径 (Mosquitto 默认通常是 / 或 /mqtt)
#define BROKER_URI "ws://192.168.1.3:8083/mqtt"

/* --- 内部变量 --- */
static esp_mqtt_client_handle_t s_client = NULL; // MQTT 客户端句柄
static MqttConnectSuccessCallback s_connect_cb = NULL; // 存连接回调
static MqttReceiveCallback s_receive_cb = NULL;        // 存接收回调

/* --- MQTT 事件处理函数 (底层保安) --- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT 已连接服务器！");
        // 如果上层注册了“连接成功回调”，就执行它
        if (s_connect_cb != NULL) {
            s_connect_cb();
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 断开连接，正在尝试重连...");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "收到数据 Topic=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "数据内容=%.*s", event->data_len, event->data);
        
        // 如果上层注册了“接收回调”，就把数据传上去
        if (s_receive_cb != NULL) {
            // 注意：event->data 不一定以 \0 结尾，所以必须传长度
            s_receive_cb(event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT 出错");
        break;
        
    default:
        break;
    }
}

/* --- 1. 初始化函数 --- */
void Driver_MQTT_Init(void)
{
    if (s_client != NULL) {
        ESP_LOGW(TAG, "MQTT 已经初始化过了，无需重复初始化");
        return;
    }

    ESP_LOGI(TAG, "正在初始化 MQTT，连接地址: %s", BROKER_URI);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URI,
        // .credentials.username = "user", // 如果有密码就在这填
        // .credentials.authentication.password = "pass",
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    
    // 注册事件处理
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // 启动客户端
    esp_mqtt_client_start(s_client);
}

/* --- 2. 注册回调函数 --- */
void Driver_MQTT_RegisterCallback(MqttConnectSuccessCallback on_connect, MqttReceiveCallback on_receive)
{
    s_connect_cb = on_connect;
    s_receive_cb = on_receive;
}

/* --- 3. 订阅主题 --- */
void Driver_MQTT_Subscribe(const char *topic, int qos)
{
    if (s_client == NULL) return;
    ESP_LOGI(TAG, "订阅主题: %s", topic);
    esp_mqtt_client_subscribe(s_client, topic, qos);
}

/* --- 4. 发送数据 (Publish) --- */
void Driver_MQTT_Publish(const char *topic, const char *data)
{
    if (s_client == NULL) return;
    // 参数：客户端, 主题, 数据, 长度(0表示自动计算字符串长度), QoS, Retain
    esp_mqtt_client_publish(s_client, topic, data, 0, 1, 0);
    ESP_LOGI(TAG, "发送数据 -> %s: %s", topic, data);
}