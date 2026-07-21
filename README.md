# ADC 波形采集 + VOFA+ 上位机显示

基于 STM32F407，通过 ADC 采集 PA0 引脚的模拟信号，经串口实时发送到 VOFA+ 上位机显示波形。

## 硬件

| 项目 | 配置 |
|---|---|
| MCU | STM32F407 |
| 信号输入 | **PA0**（ADC1_IN0） |
| 串口 | USART1，PA9(TX) / PA10(RX) |
| ADC 触发 | TIM3 TRGO，700kHz 采样 |
| 数据传输 | DMA2 Stream0，双缓冲循环模式 |

## 串口参数

```
波特率：115200
协议：  FireWater（VOFA+ 原生支持）
帧格式：每行一个浮点数（单位 V），\n 结尾
```

## VOFA+ 设置

1. 打开 VOFA+，点击左侧连接图标
2. 协议选择 **FireWater**
3. 波特率选 **115200**
4. 选择对应的 COM 口，点连接
5. 拖动右侧波形控件到显示区即可看到波形

## 关键参数

在 `Core/Src/main.c` 中可调：

```c
#define VOFA_BATCH_SIZE  150   // 每帧点数（增大=更光滑但帧率降低）
#define VOFA_BUF_SIZE    1200  // 发送缓冲区（需 ≥ BATCH_SIZE × 7）
```

## 编译 & 下载

Keil MDK 打开 `MDK-ARM/2.uvprojx`，编译后下载到开发板。

## 工作原理

```
PA0 信号 → ADC1 采集（TIM3 700kHz 触发）
         → DMA 双缓冲写入 adc_raw[2048]
         → 半满/全满中断触发
         → memcpy 快速拷贝到 adc_safe（防 DMA 撕裂）
         → 均匀下采样 150 点 → 转电压值
         → FireWater 格式通过 UART 发送
         → VOFA+ 实时绘图
```
