#include "App_Communication.h"
#include "esp_log.h"
#include "esp_system.h" // 用于 esp_restart()
#include "Driver_WebSocket.h"
#include "Inf_ES8311.h"
#include "freertos/ringbuf.h" // 【必须】引入环形缓冲区库

static const char *TAG_APP = "APP_COM";

// 定义你要订阅的主题 (如果 Common_Config.h 里没定义，这里兜底定义一下)
#ifndef TOPIC_NAME
#define TOPIC_NAME "atguigu"
#endif

// 定义传输的接口
#define VIDEO_WEBSOCKET_URI "ws://192.168.54.29:8000/ws/image"                    // 视频流的传输地址
#define AUDIO_VISITOR_WEBSOCKET_URI "ws://192.168.54.29:8000/ws/from_esp/visitor" // 访客音频（ESP说 -> 网页听
#define AUDIO_MASTER_WEBSOCKET_URI "ws://192.168.54.29:8000/ws/from_esp/master"   // 主人音频（网页说 -> ESP听

// 定义环形缓冲区大小：16k采样率 * 2字节(16bit) * 0.2秒 = 约6KB
// 建议给 8KB 或 10KB，保证网络卡顿时有足够缓冲
#define RECORD_BUFFER_SIZE (8 * 1024)

// 全局变量
RingbufHandle_t audio_ring_buf = NULL;   // 环形缓冲区句柄
TaskHandle_t captureTaskHandle = NULL;   // 任务1：从麦克风抓数据
TaskHandle_t senderTaskHandle = NULL;    // 任务2：往网络发数据
static bool is_recording_active = false; // 录音状态标志

// 定义 3 个 WebSocket 客户端句柄
esp_websocket_client_handle_t video_ws_client = NULL;         // 视频流
esp_websocket_client_handle_t audio_visitor_ws_client = NULL; // 上行音频，ESP-> 网页
esp_websocket_client_handle_t audio_master_ws_client = NULL;  // 下行音频，网页 -> ESP

/* --- 内部回调函数声明 --- */
static void OnWifiConnected(void);                     // WiFi 连上后干嘛
static void OnMqttConnected(void);                     // MQTT 连上后干嘛
static void OnMqttReceived(char *data, int len);       // 收到消息后干嘛
static void OnResetButtonPress(void *arg, void *data); // 按下重置键后干嘛

// 创建一个MQTT任务执行函数
static void App_Communication_MqttTaskFunc(void *pvParameters);
TaskHandle_t mqttHandle;

static void OnWebsocketDataReceived(char *data, int len);

// 创建音频接收任务函数
static void App_Communication_AudioReceiveTaskFunc(void *pvParameters);

// 创建音频发送任务函数
static void App_Communication_AudioSendTaskFunc(void *pvParameters);

/* --- 1. 初始化入口 --- */
void App_Communication_Init(void)
{
    ESP_LOGI(TAG_APP, "通信模块正在初始化...");

    // [步骤 A] 先注册 WiFi 连接成功的回调
    // 意思：告诉 WiFi 驱动，“等你拿到 IP 了，记得运行 OnWifiConnected 这个函数”
    Driver_WIFI_RegisterConnectedCallback(OnWifiConnected);

    // [步骤 B] 注册按键回调 (假设长按 SW2 重置配网)
    // 注意：这里需要根据你 Inf_key.h 里的实际宏来写，这里我假设用 BUTTON_LONG_PRESS_UP
    // 如果你暂时不想测试按键，可以先把这行注释掉
    Inf_key_RegisterKey2Callbacks(BUTTON_LONG_PRESS_UP, OnResetButtonPress, NULL);

    // 创建三个WebSocket客户端
    Driver_Websocket_Create(VIDEO_WEBSOCKET_URI, &video_ws_client, true);
    Driver_Websocket_Create(AUDIO_VISITOR_WEBSOCKET_URI, &audio_visitor_ws_client, true);
    Driver_Websocket_Create(AUDIO_MASTER_WEBSOCKET_URI, &audio_master_ws_client, true);
    ESP_LOGI(TAG_APP, "WebSocket 客户端已创建 (等待指令连接)");

    // [步骤 C] 启动 WiFi
    // 启动后，它会自动去连网。一旦连上，就会触发上面的 OnWifiConnected
    Driver_WIFI_Init();

    // 创建MQTT任务，用来处理MQTT消息，比如收到指令后开启或关闭音视频传输
    xTaskCreate(App_Communication_MqttTaskFunc, "MqttTask", 4096, NULL, 5, &mqttHandle);
}

/* --- 2. WiFi 连接成功后的连锁反应 --- */
static void OnWifiConnected(void)
{
    ESP_LOGI(TAG_APP, ">> WiFi 已连接！正在启动 MQTT...");

    // [步骤 D] 只有网通了，才启动 MQTT
    Driver_MQTT_Init();

    // [步骤 E] 注册 MQTT 的回调
    // 意思：告诉 MQTT 驱动，“连上服务器喊 OnMqttConnected，收到信喊 OnMqttReceived”
    Driver_MQTT_RegisterCallback(OnMqttConnected, OnMqttReceived);

    // 告诉驱动：以后收到数据，就去调用上面写的 OnWebsocketDataReceived
    Driver_Websocket_RegisterReceiveCallback(OnWebsocketDataReceived);

    ESP_LOGI(TAG_APP, "WebSocket 回调已注册，等待 MQTT 指令开启连接...");
}

/* --- 3. MQTT 连接成功后的动作 --- */
static void OnMqttConnected(void)
{
    ESP_LOGI(TAG_APP, ">> MQTT 服务器已连接！正在订阅主题: %s", TOPIC_NAME);

    // [步骤 F] 连上服务器后，马上订阅主题
    Driver_MQTT_Subscribe(TOPIC_NAME, 0);
}

/* --- 4. 收到数据的处理 --- */
static void OnMqttReceived(char *data, int len)
{
    // 这里打印收到的数据
    ESP_LOGI(TAG_APP, "📩 收到 MQTT 消息: %.*s", len, data);

    //{"cmd": "off", "type": "audio", "dir": "esp2client"}
    //{"cmd": "on", "type": "audio", "dir": "esp2client"}
    //{"cmd": "off", "type": "audio", "dir": "client2esp"}
    //{"cmd": "on", "type": "audio", "dir": "client2esp"}
    //{"cmd": "off", "type": "video", "dir": "esp2client"}
    //{"cmd": "on", "type": "video", "dir": "esp2client"}
    // 解析收到的json数据
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (root == NULL)
    {
        ESP_LOGE(TAG_APP, "解析 JSON 失败");
        return;
    }
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *dir = cJSON_GetObjectItemCaseSensitive(root, "dir");
    if (cmd == NULL || !cJSON_IsString(cmd) || type == NULL || !cJSON_IsString(type) || dir == NULL || !cJSON_IsString(dir))
    {
        ESP_LOGE(TAG_APP, "JSON 格式错误");
        cJSON_Delete(root);
        return;
    }
    if (strcmp(type->valuestring, "audio") == 0)
    {
        if (strcmp(dir->valuestring, "esp2client") == 0)
        {
            if (strcmp(cmd->valuestring, "on") == 0)
            {
                ESP_LOGI(TAG_APP, "开启音频传输 esp2client");
                // 开启音频传输 esp2client 的代码
                xTaskNotify(mqttHandle, ESP_2_CLIENT_AUDIO_ON, eSetValueWithOverwrite);
            }
            else if (strcmp(cmd->valuestring, "off") == 0)
            {
                ESP_LOGI(TAG_APP, "关闭音频传输 esp2client");
                // 关闭音频传输 esp2client 的代码
                xTaskNotify(mqttHandle, ESP_2_CLIENT_AUDIO_OFF, eSetValueWithOverwrite);
            }
        }
        else if (strcmp(dir->valuestring, "client2esp") == 0)
        {
            if (strcmp(cmd->valuestring, "on") == 0)
            {
                ESP_LOGI(TAG_APP, "开启音频传输 client2esp");
                // 开启音频传输 client2esp 的代码
                xTaskNotify(mqttHandle, CLIENT_2_ESP_AUDIO_ON, eSetValueWithOverwrite);
            }
            else if (strcmp(cmd->valuestring, "off") == 0)
            {
                ESP_LOGI(TAG_APP, "关闭音频传输 client2esp");
                // 关闭音频传输 client2esp 的代码
                xTaskNotify(mqttHandle, CLIENT_2_ESP_AUDIO_OFF, eSetValueWithOverwrite);
            }
        }
    }
    else if (strcmp(type->valuestring, "video") == 0)
    {
        if (strcmp(cmd->valuestring, "on") == 0)
        {
            ESP_LOGI(TAG_APP, "开启视频传输 esp2client");
            // 开启视频传输 esp2client 的代码
            xTaskNotify(mqttHandle, ESP_2_CLIENT_VIDEO_ON, eSetValueWithOverwrite);
        }
        else if (strcmp(cmd->valuestring, "off") == 0)
        {
            ESP_LOGI(TAG_APP, "关闭视频传输 esp2client");
            // 关闭视频传输 esp2client 的代码
            xTaskNotify(mqttHandle, ESP_2_CLIENT_VIDEO_OFF, eSetValueWithOverwrite);
        }
    }

    cJSON_Delete(root);

    // TODO: 在这里写你的控制逻辑
    // 比如：if (strncmp(data, "open", len) == 0) { 开门(); }
}

/* --- 5. 重置配网按键处理 --- */
static void OnResetButtonPress(void *arg, void *data)
{
    ESP_LOGW(TAG_APP, "检测到重置按键！正在擦除配网信息并重启...");

    // 1. 擦除 WiFi 密码
    Driver_WIFI_ResetProvisioning();

    // 2. 重启设备 (比 abort() 更温柔、更标准)
    esp_restart();
}

// 创建一个MQTT任务执行函数
static void App_Communication_MqttTaskFunc(void *pvParameters)
{

    // MQTT任务开始调度
    ESP_LOGW(TAG_APP, "MQTT任务已启动，开始处理MQTT消息...");

    uint32_t value = 0;

    while (1)
    {

        xTaskNotifyWait(pdTRUE, pdFALSE, &value, portMAX_DELAY);

        switch (value)
        {
            /* --- 音频上行 (ESP说 -> 网页听) --- */
        case ESP_2_CLIENT_AUDIO_ON:
            ESP_LOGE(TAG_APP, "指令: 开启访客音频 (ESP->Web)");
            Inf_ES8311_Open(); // 打开ES8311解码器
            // 连接 WebSocket
            if (!Driver_Websocket_IsConnected(&audio_visitor_ws_client))
            {
                Driver_Websocket_Open(&audio_visitor_ws_client);
            }
            // (未来在这里开启录音任务)
            if (!is_recording_active)
            {
                is_recording_active = true;

                // A. 创建环形缓冲区 (字节流模式)
                if (audio_ring_buf == NULL)
                {
                    // 这就是你要找的创建函数！
                    audio_ring_buf = xRingbufferCreate(RECORD_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
                    if (audio_ring_buf == NULL)
                    {
                        ESP_LOGE(TAG_APP, "内存不足，无法创建缓冲区!");
                        break; // 退出 case
                    }
                }

                // B. 创建录音采集任务
                xTaskCreate(App_Communication_AudioReceiveTaskFunc, "AudioReceiveTask", 4096, NULL, 5, &captureTaskHandle);

                // C. 创建发送任务 (优先级中)
                // 创建一个任务来处理音频发送到网页 通过websocket的手段
                xTaskCreate(App_Communication_AudioSendTaskFunc, "AudioSendTask", 4096, NULL, 5, &senderTaskHandle);
            }
            break;
            break;
        case ESP_2_CLIENT_AUDIO_OFF:
            ESP_LOGE(TAG_APP, "指令: 关闭访客音频 (ESP->Web)");
            // 1. 停止标志位
            is_recording_active = false;

            // 2. 给一点时间让那两个任务退出循环
            vTaskDelay(pdMS_TO_TICKS(100));
            // 3. 销毁环形缓冲区 (释放内存)
            if (audio_ring_buf != NULL)
            {
                vRingbufferDelete(audio_ring_buf);
                audio_ring_buf = NULL;
            }
            Driver_Websocket_Close(&audio_visitor_ws_client);
            Inf_ES8311_Close(); // 计数器-1
            break;

            /* --- 音频下行 (网页说 -> ESP听) --- */

        case CLIENT_2_ESP_AUDIO_ON:
            ESP_LOGE(TAG_APP, "指令: 开启主人音频 (Web->ESP)");

            if (!Driver_Websocket_IsConnected(&audio_master_ws_client))
            {
                Inf_ES8311_Open(); // 打开ES8311解码器
                Driver_Websocket_Open(&audio_master_ws_client);
            }

            break;
        case CLIENT_2_ESP_AUDIO_OFF:
            ESP_LOGE(TAG_APP, "指令: 关闭主人音频 (Web->ESP)");
            Driver_Websocket_Close(&audio_master_ws_client);
            Inf_ES8311_Close(); // 计数器-1
            break;

            /* --- 视频流 --- */

        case ESP_2_CLIENT_VIDEO_ON:
            ESP_LOGE(TAG_APP, "指令: 开启视频流");
            if (!Driver_Websocket_IsConnected(&video_ws_client))
            {
                Driver_Websocket_Open(&video_ws_client);
            }
            break;
        case ESP_2_CLIENT_VIDEO_OFF:
            ESP_LOGE(TAG_APP, "指令: 关闭视频流");
            Driver_Websocket_Close(&video_ws_client);
            break;
        default:
            break;
        }
    }
}

static void OnWebsocketDataReceived(char *data, int len)
{
    // 打印一下，证明收到数据了
    ESP_LOGI(TAG_APP, "🔥 收到 WebSocket 数据, 长度: %d", len);
    // 写到编码器里面去
    Inf_ES8311_Write((uint8_t *)data, len);
}

static void App_Communication_AudioReceiveTaskFunc(void *pvParameters)
{
    // 临时缓存，用于从 I2S 读取一次数据
    // 建议 512 或 1024 字节
    size_t chunk_size = 512;
    uint8_t *temp_buffer = malloc(chunk_size);

    ESP_LOGI(TAG_APP, "🎙️ 录音采集任务启动");

    while (is_recording_active)
    {
        // 1. 从 ES8311 驱动读取数据 (阻塞读取)
        // 注意：Inf_ES8311_Read 内部应该是调用 i2s_read
        int ret = Inf_ES8311_Read(temp_buffer, chunk_size);

        if (ret == ESP_OK)
        {
            // 2. 将数据推送到环形缓冲区
            // portMAX_DELAY 表示如果缓冲区满了，我就死等，直到有空间
            // 这样保证了不会丢数据，但如果网络彻底断了，这里会阻塞
            UBaseType_t res = xRingbufferSend(audio_ring_buf, temp_buffer, chunk_size, pdMS_TO_TICKS(100));

            if (res != pdTRUE)
            {
                ESP_LOGW(TAG_APP, "⚠️ 缓冲区溢出 (网络发送太慢)");
            }
        }
        else
        {
            // 硬件读取失败，稍微休息一下
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    free(temp_buffer); // 任务结束，释放临时内存
    ESP_LOGI(TAG_APP, "🎙️ 录音采集任务退出");
    vTaskDelete(NULL);
}

/* --- 任务 B：消费者 (从环形缓冲区 -> WebSocket) --- */
void App_Communication_AudioSendTaskFunc(void *pvParameters)
{

    size_t item_size;
    char *item_data;

    ESP_LOGI(TAG_APP, "📡 发送任务启动");

    while (is_recording_active)
    {
        // 1. 从缓冲区取数据
        // 参数2: 拿到数据的长度指针
        // 参数3: 等待时间 100ms
        item_data = (char *)xRingbufferReceive(audio_ring_buf, &item_size, pdMS_TO_TICKS(100));

        // 2. 检查是否取到了数据
        if (item_data != NULL)
        {
            // 3. 发送给 WebSocket
            if (Driver_Websocket_IsConnected(&audio_visitor_ws_client))
            {
                esp_websocket_client_send_bin(audio_visitor_ws_client, item_data, item_size, portMAX_DELAY);
            }

            // 4. 【重要】用完必须归还内存给缓冲区！否则内存瞬间泄露完
            vRingbufferReturnItem(audio_ring_buf, (void *)item_data);
        }
    }

    vTaskDelete(NULL); // 任务自杀
}
