#ifndef __COMMON_DEBUG_H__
#define __COMMON_DEBUG_H__ 
#include "esp_log.h"
#include "string.h"
#include "stdio.h"

extern char TAG[128];

#define DEBUG 

#ifdef DEBUG

#define FILE_NAME strrchr(__FILE__,'/') ? strrchr(__FILE__,'/') + 1  : __FILE__
#define FILE_NAME2 strrchr(FILE_NAME,'\\') ? strrchr(FILE_NAME,'\\') + 1  : FILE_NAME

#define MyLogE( format, ... ) { \
    sprintf(TAG,"[%20s:%d] ",FILE_NAME2,__LINE__); \
    ESP_LOGE(TAG, format, ##__VA_ARGS__) ;\
}
#define MyLogW( format, ... ) { \
    sprintf(TAG,"[%20s:%d] ",FILE_NAME2,__LINE__); \
    ESP_LOGW(TAG, format, ##__VA_ARGS__); \
}
#define MyLogI( format, ... ) { \
    sprintf(TAG,"[%20s:%d] ",FILE_NAME2,__LINE__); \
    ESP_LOGI(TAG, format, ##__VA_ARGS__); \
}
#else

#endif

#endif