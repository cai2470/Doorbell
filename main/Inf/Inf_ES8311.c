#include "Inf_ES8311.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#define USE_IDF_I2C_MASTER

#define SDA_PIN 0
#define SCL_PIN 1

#define BCK_PIN 2
#define MCK_PIN 3
#define DI_PIN 4
#define WS_PIN 5
#define DO_PIN 6
#define PA_EN_PIN 7


// 增加一个标记，记录硬件是否已经初始化过
static bool is_hardware_initialized = false;

// 定义一个静态变量来记录有几个模块正在使用音频
static int s_audio_usage_count = 0;

// 创建I2C总线的句柄
i2c_master_bus_handle_t i2c_bus_handle;

// 麦克风通道
i2s_chan_handle_t mic_handle; // 收声的
// 扬声器通道
i2s_chan_handle_t speaker_handle; // 放声的
// 编解码设备句柄
esp_codec_dev_handle_t play_dev;

// I2C 配置结构体  初始化I2C总线配置
static void Inf_ES8311_I2CInit(void)
{

    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = 1, // 启动内部上拉
        }};
    // 创建I2C总线
    i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
}

static void Inf_ES8311_I2SInit(void)
{
    // 获取I2S通道的默认配置
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    // 配置I2S通道参数
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_MONO), // 采样深度16bit  单声道
        .gpio_cfg = {
            .mclk = MCK_PIN,
            .bclk = BCK_PIN,
            .ws = WS_PIN,
            .dout = DO_PIN,
            .din = DI_PIN,
        }};
    // 创建读取和发送通道
    i2s_new_channel(&chan_cfg, &speaker_handle, &mic_handle);
    // 初始化通道为标准模式
    i2s_channel_init_std_mode(speaker_handle, &std_cfg);
    i2s_channel_init_std_mode(mic_handle, &std_cfg);

    // 使能通道
    i2s_channel_enable(speaker_handle);
    i2s_channel_enable(mic_handle);
}

// 初始化编解码函数
static void Inf_ES8311_CodecInit(void)
{
    // i2s通道配置
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = mic_handle,
        .tx_handle = speaker_handle,
        .port = I2S_NUM_0 // 一定要和Inf_ES8311_I2SInit里面的I2S端口一致
    };
    // 初始化I2S接口
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    // 初始化I2C接口
    audio_codec_i2c_cfg_t i2c_cfg = {
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_handle, // i2c句柄[旧版本I2C可以忽略]
        .port = I2C_NUM_0             // 必须与初始化I2C的时候端口一致
    };

    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    // 创建GPIO接口
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    // 创建编解码器接口
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH, // 支持ADC和DAC[支持读取麦克风数据和写入数据到扬声器]
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = PA_EN_PIN,
        .use_mclk = true,
    };
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg);
    // 创建设备
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = out_codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT, // ES8311既读又写
    };
    play_dev = esp_codec_dev_new(&dev_cfg);

    // 设置默认输出音量60
    esp_codec_dev_set_out_vol(play_dev, 60.0);
    // 设置输入信号增益
    esp_codec_dev_set_in_gain(play_dev, 30.0);
}

void Inf_ES8311_Init(void)
{
    Inf_ES8311_I2CInit();
    Inf_ES8311_I2SInit();
    Inf_ES8311_CodecInit();
}

void Inf_ES8311_SetVolume(int volume)
{

    esp_codec_dev_set_out_vol(play_dev, volume);
}



void Inf_ES8311_Open(void)
{

    if (s_audio_usage_count == 0)
    {
        esp_codec_dev_sample_info_t fs = {
            .sample_rate = 16000,  // 采样率16K[和之前保持一致]
            .channel = 1,          // 通道数[我们只有一个麦克风]
            .bits_per_sample = 16, // 采样深度[和之前保持一致]
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0)};
        esp_codec_dev_open(play_dev, &fs);
    }
    s_audio_usage_count++; // 使用者+1
}

void Inf_ES8311_Close(void)
{
    if (s_audio_usage_count > 0)
    {
        s_audio_usage_count--; // 使用者-1
    }

    if (s_audio_usage_count == 0)
    {
        // 只有当没人用了，才真的关闭硬件
        esp_codec_dev_close(play_dev);
        ESP_LOGI("ES8311", "硬件已关闭 (省电模式)");
    }
    else
    {
        ESP_LOGI("ES8311", "仍有模块在使用，暂不关闭硬件");
    }
}

int Inf_ES8311_Read(uint8_t *datas, int len)
{

    // 只有当设备存在、len>0 、存储数据的容器不为Null
    if (play_dev && datas && len)
    {

        return esp_codec_dev_read(play_dev, datas, len);
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}

int Inf_ES8311_Write(uint8_t *datas, int len)
{
    if (play_dev && datas && len)
    {

        return esp_codec_dev_write(play_dev, datas, len);
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}