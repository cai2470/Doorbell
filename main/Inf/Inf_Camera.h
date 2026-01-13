#ifndef __INF_CAMERA_H__
#define __INF_CAMERA_H__
#include "esp_camera.h"

void Inf_Camera_Init(void);

void Inf_Camera_GetImage(uint8_t **img, size_t *len);

void Inf_Camera_Return(void);

#endif

