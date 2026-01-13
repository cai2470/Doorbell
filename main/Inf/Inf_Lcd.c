#include "Inf_Lcd.h"
#include "Common_Debug.h"


esp_lcd_panel_handle_t panel_handle = NULL;
uint16_t* panel_buff;

SemaphoreHandle_t semHandle;
// Using SPI2 in the example
#define LCD_HOST SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_SCLK 47
#define PIN_NUM_MOSI 48
#define PIN_NUM_MISO 18
#define PIN_NUM_LCD_DC 45
#define PIN_NUM_LCD_RST 16
#define PIN_NUM_LCD_CS 21
#define PIN_NUM_BK_LIGHT 40

#define LCD_H_RES 240
#define LCD_V_RES 320

// Bit number used to represent command and parameter
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

/**
 * @brief 传输完成回调
 * 
 * @param panel_io 
 * @param edata 
 * @param user_ctx 
 * @return true 
 * @return false 
 */
bool transmit_over_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx){

    //释放信号量告知外界传输完成
    xSemaphoreGive(semHandle);

    return true;
}

void Inf_Lcd_Init(void)
{

    // 初始化背光引脚
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // 初始化SPI
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 初始化面板IO句柄
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,

    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 创建面板
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16, //颜色的位数
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    //复位
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    //初始化面板
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    //开启屏幕显示
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    //开启背光
    gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);

    //申请一块内存存储整个屏幕像素颜色
    panel_buff =  (uint16_t*)malloc( LCD_H_RES * LCD_V_RES * sizeof(uint16_t) );
    //创建一个信号量
    vSemaphoreCreateBinary(semHandle);
    //注册传输完成回调
    esp_lcd_panel_io_callbacks_t cb = {
        .on_color_trans_done = transmit_over_callback
    };
    esp_lcd_panel_io_register_event_callbacks( io_handle, &cb, NULL);

}
/**
 * @brief 交换大小端
 * 
 * @param color 
 * @return uint16_t 
 */
static uint16_t change(uint16_t color){

    return (color >> 8) | (color << 8);
}
/**
 * @brief 刷屏
 * 
 * @param color 
 */
void Inf_Lcd_Refresh( uint16_t color ){

    memset(panel_buff, 0 , LCD_H_RES * LCD_V_RES * sizeof(uint16_t));
    for(uint32_t i=0; i < LCD_H_RES * LCD_V_RES; i++){
        panel_buff[i] = change(color);  //因为MCU是小端存储 SPI是先发大端
    }

    for(uint32_t i=0; i < LCD_V_RES ; i ++){
        //等待上一次传输完成
        xSemaphoreTake(semHandle, portMAX_DELAY);
        esp_lcd_panel_draw_bitmap(panel_handle,0,i, LCD_H_RES, i + 1, &panel_buff[i * 240] );
    }

}