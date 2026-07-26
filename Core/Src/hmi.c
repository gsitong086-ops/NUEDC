#include "hmi.h"

/**
 * @brief 发送原始字符串 + 淘晶驰协议结束符 0xFF 0xFF 0xFF
 */
void HMI_SendRaw(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), 100);
    uint8_t end[3] = {0xFF, 0xFF, 0xFF};
    HAL_UART_Transmit(&huart1, end, 3, 100);
}

/**
 * @brief 设置文本控件内容
 * 格式: obj.txt="text" + FF FF FF
 */
void HMI_SetText(const char *obj, const char *text)
{
    char buf[128];         
    snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", obj, text);
    HMI_SendRaw(buf);
}

/**
 * @brief 往波形控件追加数据点
 * 格式: add wave,ch,val + FF FF FF
 * val 范围 0~255
 */
void HMI_AddWave(const char *wave, uint8_t ch, uint8_t val)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "add %s,%d,%d", wave, ch, val);
    HMI_SendRaw(buf);
}

/**
 * @brief 切换页面
 * 格式: page id + FF FF FF
 */
void HMI_Page(const char *page)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "page %s", page);
    HMI_SendRaw(buf);
}

/**
 * @brief 清空波形控件指定通道
 * 格式: cle wave,ch + FF FF FF
 */
void HMI_ClearWave(const char *wave, uint8_t ch)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "cle %s,%d", wave, ch);
    HMI_SendRaw(buf);
}
