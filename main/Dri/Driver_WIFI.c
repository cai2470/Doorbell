#include "Driver_WIFI.h" 

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "qrcode.h"

static const char *TAG = "WIFI_DRI";
static bool g_is_connected = false; 

// 定义一个静态变量，用来存回调函数
// 注意：类型名 WifiConnectedCallback 必须与 .h 文件一致
static WifiConnectedCallback s_wifi_cb = NULL;

#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_BLE "ble"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

/* --- 事件处理回调 --- */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "配网已启动");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "收到凭证 SSID: %s", (const char *)wifi_sta_cfg->ssid);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "配网失败: %s",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "密码错误" : "找不到热点");
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "配网成功");
                break;
            case WIFI_PROV_END:
                wifi_prov_mgr_deinit();
                break;
            default: break;
        }
    } else if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG, "WiFi 断开，重连中...");
            g_is_connected = false;
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "已连接! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_is_connected = true;
        
        // 【关键点】触发回调通知上层
        if (s_wifi_cb) {
            s_wifi_cb();
        }
    }
}

/* --- 打印二维码 --- */
static void wifi_prov_print_qr(const char *name, const char *pop, const char *transport)
{
    if (!name || !transport) return;
    char payload[150] = {0};
    if (pop) {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, pop, transport);
    } else {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, transport);
    }

    ESP_LOGI(TAG, "请扫描二维码或手动连接 (密码: %s)", pop);
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);
    ESP_LOGI(TAG, "备用链接: %s?data=%s", QRCODE_BASE_URL, payload);
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "PROV_%02X%02X%02X", eth_mac[3], eth_mac[4], eth_mac[5]);
}

/* --- 【修正1】实现回调注册函数 (名字必须和 .h 以及 App_Communication.c 一致) --- */
void Driver_WIFI_RegisterConnectedCallback(WifiConnectedCallback cb)
{
    s_wifi_cb = cb;
}

/* --- 【修正2】补全重置配网函数 --- */
void Driver_WIFI_ResetProvisioning(void)
{
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    // 必须先 Init 才能 Reset，防止空指针崩溃
    wifi_prov_mgr_init(config); 
    
    ESP_LOGW(TAG, "正在擦除 WiFi 配网信息...");
    wifi_prov_mgr_reset_provisioning();
}

bool Driver_WIFI_IsConnected(void)
{
    return g_is_connected;
}

void Driver_WIFI_Init(void)
{
    // 标准初始化流程
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "未配网，开始 BLE 配网...");
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));
        
        const char *pop = "12345678"; 
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, (const void *)pop, service_name, NULL));
        wifi_prov_print_qr(service_name, pop, PROV_TRANSPORT_BLE);
    } else {
        ESP_LOGI(TAG, "已配网，启动 WiFi");
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}