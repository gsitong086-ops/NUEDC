# STM32F407 + 淘晶驰TJC串口屏 — 函数信号发生器波形显示

## 功能
- STM32F407ZGT6 通过 ADC1(PA0) 采集函数信号发生器波形
- 实时显示波形在淘晶驰TJC8048X270_011C串口屏上（曲线/波形控件，ID=5，objname=wave0）
- 自动测量并显示频率（Hz，过零检测多周期平均）和峰峰值（Vpp）
- 触摸按钮控制：开始采集、暂停采集、清除波形

## 硬件连接

| STM32F407ZGT6 | 淘晶驰屏 | 说明 |
|---|---|---|
| PA9 (USART1_TX) | RX | 主控 → 屏 |
| PA10 (USART1_RX) | TX | 屏 → 主控（触摸回传）|
| GND | GND | **必须共地** |
| 5V 外接电源 | VCC | 屏单独供5V，勿从ST-Link取电 |
| PA0 (ADC1_IN0) | — | 接函数信号发生器输出 |

> ⚠️ **信号发生器输出必须在 0~3.3V 内**（建议 Vpp ≤ 3V，偏置 = 1.5V），否则会削波甚至损坏ADC引脚。

## CubeMX 配置参数

| 配置项 | 参数 |
|---|---|
| MCU | STM32F407ZGT6 |
| SYS Debug | Serial Wire |
| RCC HSE | Crystal/Ceramic Resonator |
| SYSCLK | 168MHz |
| USART1 | Asynchronous, 115200, 8N1, PA9/PA10 |
| USART1 NVIC | 开启全局中断 |
| ADC1 IN0 (PA0) | 12-bit, External Trigger=TIM2 TRGO Rising, DMAContinuousRequests=ENABLE |
| ADC1 DMA | Circular, Half Word, DMA2 Stream0 Channel0 |
| ADC1 SamplingTime | 84 Cycles |
| TIM2 | Prescaler=84-1, Period=100-1 → 10kHz触发 |
| TIM2 TRGO | Master Trigger = Update Event |

## 淘晶驰 UI 控件

| 控件 | objname | id | 说明 |
|---|---|---|---|
| 频率显示 | t_freq | — | 文本控件，显示 "xxx.x Hz" |
| Vpp显示 | t_vpp | — | 文本控件，显示 "x.xxx V" |
| 波形 | wave0 | 5 | 曲线/波形控件，通道数=1 |
| 状态提示 | t_info | — | 文本控件 |
| 开始按钮 | b_start | — | printh: 53 54 41 52 54 0D 0A |
| 暂停按钮 | b_pause | — | printh: 50 41 55 53 45 0D 0A |
| 清除按钮 | b_clear | — | printh: 43 4C 45 41 52 0D 0A |

## 文件结构

```
├── Core/
│   ├── Inc/
│   │   └── hmi.h              # 串口屏驱动头文件
│   └── Src/
│       ├── hmi.c              # 串口屏驱动（逐字节发送，FF FF FF结束符）
│       └── main.c             # 主程序（ADC+DMA采集、过零测频、Vpp测量、波形推送、按键解析）
├── 淘晶驰UI/
│   └── UI配置说明.txt         # 淘晶驰上位机UI详细配置
├── CubeMX配置表.txt           # CubeMX 逐项配置
├── 排错清单.txt               # 常见问题排查
└── README.md                  # 本文件
```

## 关键实现细节

### 通信协议
- 每条指令以 3 字节 `0xFF 0xFF 0xFF` 结束（淘晶驰标准）
- 发送方式：逐字节阻塞发送（参考官方例程），等待 TC 标志
- 波形指令格式：`add 5,0,val` + FF FF FF（控件ID=5，通道0，数值0~255）
- 清除波形：`cle 5,0` + FF FF FF

### ADC 采集
- TIM2 触发 ADC1，10k SPS，DMA 循环缓冲 2048 点
- 从缓冲区后半段取数据测量（避免 DMA 正在写的区域）

### 频率测量
- 过零检测，自动计算信号中点，多周期平均

### Vpp 测量
- 缓冲区 max-min，转换为电压值

### 按键解析
- 兼容两种方式：0x65 标准触摸帧 + printh ASCII 字符串（`\r\n` 结束）

## 使用步骤

1. 在 CubeMX 中按上表配置生成 Keil 工程
2. 将 `hmi.c/h` 复制到 `Core/Src/` 和 `Core/Inc/`
3. 将 `main.c` 中的 USER CODE 段代码合并到工程
4. 在淘晶驰上位机中按 `UI配置说明.txt` 创建界面并烧录到屏
5. 信号发生器输出接 PA0，设置 Vpp=1.5V，偏置=1.5V（波形范围 0.75V~2.25V）
6. 编译下载，屏和STM32同时上电，等待约3秒后自动开始采集

## 注意事项

- 屏和STM32需要同时上电，或屏先上电
- 115200波特率下推送速率约 20 点/秒
- 信号幅度务必在 0~3.3V 内
- 频率测量范围约 1Hz~2kHz（10kSPS 下）
- 频率/峰峰值每 300ms 刷新一次
