#include "Inf_Camera.h"

#define CAM_PIN_XCLK 37
#define CAM_PIN_SIOD 0
#define CAM_PIN_SIOC 1
#define CAM_PIN_D7 38
#define CAM_PIN_D6 36
#define CAM_PIN_D5 35
#define CAM_PIN_D4 33
#define CAM_PIN_D3 10
#define CAM_PIN_D2 12
#define CAM_PIN_D1 11
#define CAM_PIN_D0 9
#define CAM_PIN_VSYNC 41
#define CAM_PIN_HREF 39
#define CAM_PIN_PCLK 34

camera_fb_t * pic;

static camera_config_t camera_config = {
    .pin_pwdn = -1,
    .pin_reset = -1,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0, //用于生成主时钟的定时器
    .ledc_channel = LEDC_CHANNEL_0, //用于生成主时钟的定时器的channel

    .pixel_format = PIXFORMAT_JPEG, //输出格式
    .frame_size = FRAMESIZE_QVGA,    //帧的大小

    .jpeg_quality = 20, //数据质量 值越小质量越高
    .fb_count = 2,        //缓冲个数 
    .fb_location = CAMERA_FB_IN_PSRAM, //缓冲存储位置,采样外部sram
    .grab_mode = CAMERA_GRAB_LATEST, //自始至终抓起最新的图像

    .sccb_i2c_port = 0
};

void Inf_Camera_Init(void){

    esp_camera_init(&camera_config);
}
/**
 * @brief 获取图片
 * 
 * @param img 
 * @param len 
 */
void Inf_Camera_GetImage(uint8_t** img, size_t* len ){
    pic = esp_camera_fb_get();

    *len = 0;
    if(pic && pic->format == PIXFORMAT_JPEG){
        *img = pic->buf;
        *len = pic->len;
    }
}

/**
 * @brief 回收图片内存
 * 
 */
void Inf_Camera_Return(void){
    if(pic){
        esp_camera_fb_return(pic);
        pic = NULL;
    }
}