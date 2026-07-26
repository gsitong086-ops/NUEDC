/* USER CODE BEGIN Includes */
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "hmi.h"
#include "math.h"
/* USER CODE END Includes */

/* 参数定义 ---------------------------------------------------------------*/
#define WAVE_BUF_LEN   1024       // ADC DMA 循环缓冲大小
#define WAVE_WIDTH     400        // 屏上波形宽度（像素）
#define N_CYC_TARGET   3          // 期望屏上显示约几个周期（自适应用）
#define R_DISP_MAX     600        // 115200波特下最大推送速率（点/秒）
#define F_SAMPLE       10000      // ADC 内部采样率 10k SPS（由TIM2决定）
#define MID_ADC        2048       // 12-bit ADC 中点（对应1.65V）
#define VREF           3.3f       // 参考电压

/* 全局变量 ---------------------------------------------------------------*/
uint16_t adc_buf[WAVE_BUF_LEN];   // ADC DMA 循环缓冲

/* 触摸回传变量 -----------------------------------------------------------*/
uint8_t  rx_byte;                  // 接收单字节
uint8_t  rx_buf[32];               // 接收缓冲（字符串模式，足够长）
uint8_t  rx_idx = 0;               // 缓冲索引
volatile uint8_t capture_on = 1;   // 采集开关（触摸按钮切换）

/* 私有函数声明 -----------------------------------------------------------*/
static float measure_freq(uint16_t *buf, uint32_t n);
static float measure_vpp(uint16_t *buf, uint32_t n);
static void   HMI_RxHandler(uint8_t b);

/* 系统时钟配置 */
void SystemClock_Config(void);

/**
 * @brief  主函数
 */
int main(void)
{
  /* MCU 初始化 */
  HAL_Init();
  SystemClock_Config();

  /* 外设初始化（由 CubeMX 生成） */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_ADC1_Init();

  /* 启动 TIM2 触发 ADC1 + DMA 循环采集 */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, WAVE_BUF_LEN);
  HAL_TIM_Base_Start(&htim2);

  /* 启动串口中断接收（触摸回传） */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 切换屏幕到显示页面 */
  HMI_Page("0");
  HAL_Delay(200);

  /* 初始状态提示 */
  HMI_SetText("t_info", "采集运行中");

  /* 主循环变量 */
  uint32_t last_push  = 0;   // 上次推送波形点的时间戳
  uint32_t last_meas  = 0;   // 上次测量频率/Vpp 的时间戳
  float    freq       = 0;   // 当前测得频率
  float    vpp        = 0;   // 当前测得峰峰值

  while (1)
  {
    /* ---- 每 300ms 测量一次频率和峰峰值 ---- */
    if (HAL_GetTick() - last_meas >= 300)
    {
      last_meas = HAL_GetTick();

      if (capture_on)
      {
        freq = measure_freq(adc_buf, WAVE_BUF_LEN);
        vpp  = measure_vpp(adc_buf, WAVE_BUF_LEN);
      }

      /* 无论采集启停，都更新文本显示（暂停时保持最后值） */
      char str[32];
      if (freq > 0)
      {
        snprintf(str, sizeof(str), "%.1f Hz", freq);
      }
      else
      {
        snprintf(str, sizeof(str), "DC / No Signal");
      }
      HMI_SetText("t_freq", str);

      if (vpp > 0.005f)
      {
        snprintf(str, sizeof(str), "%.3f V", vpp);
      }
      else
      {
        snprintf(str, sizeof(str), "~0 V");
      }
      HMI_SetText("t_vpp", str);
    }

    /* ---- 自适应推送波形点到串口屏 ---- */
    if (capture_on)
    {
      /* 根据频率计算推送间隔，使屏上约显示 N_CYC_TARGET 个周期 */
      uint32_t r_disp;  // 推送速率（点/秒）
      if (freq > 0)
      {
        r_disp = (uint32_t)((float)N_CYC_TARGET * freq * WAVE_WIDTH);
      }
      else
      {
        r_disp = 200;  // 直流或无信号时，默认慢速滚动
      }

      /* 限制推送速率在安全范围内 */
      if (r_disp > R_DISP_MAX) r_disp = R_DISP_MAX;
      if (r_disp < 30)         r_disp = 30;

      uint32_t interval = 1000 / r_disp;  // 推送间隔（ms）

      if (HAL_GetTick() - last_push >= interval)
      {
        last_push = HAL_GetTick();

        /* 取 DMA 缓冲中最新的采样值 */
        uint16_t adc_val = adc_buf[0];
        uint8_t  v       = (uint8_t)(adc_val >> 4);  // 12bit -> 8bit (0~255)

        HMI_AddWave("wave0", 0, v);
      }
    }
    else
    {
      /* 暂停时同步时间戳，避免恢复后积压大量点 */
      last_push = HAL_GetTick();
    }
  }
}

/**
 * @brief  软件过零检测测量信号频率
 * @param  buf ADC 采样缓冲
 * @param  n   缓冲长度
 * @return 频率 (Hz)，0 表示无信号或直流
 */
static float measure_freq(uint16_t *buf, uint32_t n)
{
  uint32_t cross_count = 0;
  uint32_t idx_first = 0, idx_second = 0;

  /* 找两次同向过零点（上升沿或下降沿均可） */
  for (uint32_t i = 1; i < n; i++)
  {
    int prev_above = (buf[i - 1] >= MID_ADC);
    int curr_above = (buf[i]     >= MID_ADC);

    if (prev_above != curr_above)
    {
      cross_count++;
      if (cross_count == 1)
        idx_first = i;
      else if (cross_count == 2)
      {
        idx_second = i;
        break;
      }
    }
  }

  /* 至少需要两次过零（即至少一个完整半周期跨过的两次过零） */
  if (cross_count < 2)
    return 0.0f;

  uint32_t samples_per_half_period = idx_second - idx_first;
  if (samples_per_half_period < 2)
    return 0.0f;

  /* 两次相邻过零 = 半个周期，所以周期 = 2 * samples_per_half_period */
  float period_sec = (float)(2 * samples_per_half_period) / (float)F_SAMPLE;
  if (period_sec < 1e-6f)
    return 0.0f;

  return 1.0f / period_sec;
}

/**
 * @brief  测量信号峰峰值（Vpp）
 * @param  buf ADC 采样缓冲
 * @param  n   缓冲长度
 * @return 峰峰值 (V)
 */
static float measure_vpp(uint16_t *buf, uint32_t n)
{
  uint16_t max_val = 0;
  uint16_t min_val = 4095;

  for (uint32_t i = 0; i < n; i++)
  {
    if (buf[i] > max_val) max_val = buf[i];
    if (buf[i] < min_val) min_val = buf[i];
  }

  uint16_t peak_to_peak_adc = max_val - min_val;
  return (float)peak_to_peak_adc * VREF / 4095.0f;
}

/**
 * @brief  淘晶驰触摸回传解析（字符串模式）
 *
 * 按钮事件配置方式（淘晶驰上位机）：
 *   按钮 → 事件 → 触摸事件 → 弹起事件 → 打印 → 输入标识字符串
 *   - 开始按钮：打印 "START"
 *   - 暂停按钮：打印 "PAUSE"
 *   - 清除按钮：打印 "CLEAR"
 *
 * 屏收到触摸后，会通过串口原样发回这些字符串。
 * 这里用换行符 \n 或回车 \r 作为一条指令结束标志。
 */
static void HMI_RxHandler(uint8_t b)
{
  /* 以换行或回车作为指令结束符 */
  if (b == '\n' || b == '\r')
  {
    if (rx_idx > 0)
    {
      rx_buf[rx_idx] = '\0';  // 字符串结束

      /* 匹配指令 */
      if (strcmp((char *)rx_buf, "START") == 0)
      {
        capture_on = 1;
        HMI_SetText("t_info", "采集运行中");
      }
      else if (strcmp((char *)rx_buf, "PAUSE") == 0)
      {
        capture_on = 0;
        HMI_SetText("t_info", "采集已暂停");
      }
      else if (strcmp((char *)rx_buf, "CLEAR") == 0)
      {
        HMI_ClearWave("wave0", 0);
        HMI_SetText("t_info", "波形已清除");
      }

      rx_idx = 0;  // 重置缓冲
    }
  }
  else if (rx_idx < sizeof(rx_buf) - 1)
  {
    rx_buf[rx_idx++] = b;  // 逐字节存入缓冲
  }
}

/**
 * @brief  USART 接收中断回调
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    HMI_RxHandler(rx_byte);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  // 重新开启接收
  }
}

/**
 * @brief  系统时钟配置（168MHz）
 *         HSE(8MHz) → PLLM=8, PLLN=336, PLLP=2 → SYSCLK=168MHz
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* 配置 HSE */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM       = 8;
  RCC_OscInitStruct.PLL.PLLN       = 336;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ       = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /* 配置时钟总线 */
  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;      // AHB = 168MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;        // APB1 = 42MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;        // APB2 = 84MHz
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}
