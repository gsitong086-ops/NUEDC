#ifndef __HMI_H
#define __HMI_H

#include "usart.h"
#include "string.h"
#include "stdio.h"

extern UART_HandleTypeDef huart1;

/**
 * @brief 发送原始字符串指令 + 0xFF 0xFF 0xFF 结束符
 * @param str 指令字符串（不含结束符）
 */
void HMI_SendRaw(const char *str);

/**
 * @brief 设置文本控件的内容
 * @param obj 控件名称，如 "t0"、"t_freq"
 * @param text 文本内容
 */
void HMI_SetText(const char *obj, const char *text);

/**
 * @brief 往波形控件追加一个数据点
 * @param wave 波形控件名称或ID，如 "wave0"
 * @param ch 通道号（0起始）
 * @param val 数值 0~255
 */
void HMI_AddWave(const char *wave, uint8_t ch, uint8_t val);

/**
 * @brief 切换页面
 * @param page 页面名称或ID，如 "0"
 */
void HMI_Page(const char *page);

/**
 * @brief 清空波形控件指定通道
 * @param wave 波形控件名称或ID
 * @param ch 通道号（0起始）
 */
void HMI_ClearWave(const char *wave, uint8_t ch);

#endif /* __HMI_H */
