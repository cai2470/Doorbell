#include "App_Bell.h"
#include "Inf_ES8311.h"
#include "Inf_Key.h"

//_binary_文件名_文件后缀_start  : 文件开头
//_binary_文件名_文件后缀_end  : 文件结尾
extern uint8_t dingdongStart[] asm("_binary_dingdong_pcm_start");
extern uint8_t dingdongEnd[] asm("_binary_dingdong_pcm_end");

/**
 * @brief 按键事件回调
 *
 * @param button_handle
 * @param usr_data
 */
static void App_Bell_KeyCallback(void *button_handle, void *usr_data)
{

    // 打开ES8311
    Inf_ES8311_Open();

    // dingdong声音响起
    Inf_ES8311_Write(dingdongStart, dingdongEnd - dingdongStart);

    // 关闭通道
    Inf_ES8311_Close();
}

void App_Bell_Init(void)
{

    // 给按键1注册事件回调
    Inf_key_RegisterKey1Callbacks(BUTTON_SINGLE_CLICK, App_Bell_KeyCallback, (void *)KEY1);
    // 初始化ES8311
    Inf_ES8311_Init();
}