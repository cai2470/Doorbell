
#ifndef __INF_KEY_H__
#define __INF_KEY_H__

#include "esp_err.h"
#include "iot_button.h"

typedef enum {
    KEY1 = 1,
    KEY2 = 2
} KeyNum;


void key_init(void);

void Inf_key_RegisterKey1Callbacks(button_event_t event, button_cb_t cb, void *data);

void Inf_key_RegisterKey2Callbacks(button_event_t event, button_cb_t cb, void *data);

#endif /* __INF_KEY_H__ */