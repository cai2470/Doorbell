#ifndef __INF_ES8311_H__
#define __INF_ES8311_H__

#include <stdint.h>

void Inf_ES8311_Init(void);
void Inf_ES8311_SetVolume(int volume);
void Inf_ES8311_Open(void);
void Inf_ES8311_Close(void);
int Inf_ES8311_Read(uint8_t *datas, int len);
int Inf_ES8311_Write(uint8_t *datas, int len);

#endif /* __INT_ES8311_H__ */