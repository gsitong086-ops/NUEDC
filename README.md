# 信号调制度测量装置

基于 STM32F407，实现 AM/FM 信号调制度的自动测量与显示。

## 硬件连接

| STM32 引脚 | 连接 |
|---|---|
| **PA0** | FM 中频信号输入（混频器 → LPF → 偏置电路） |
| **PC0** | AM 检波信号输入（二极管包络检波器输出） |
| **PA15** | 模式切换按键（按一下翻转 AM↔FM） |
| **PE6** | 继电器控制（高=FM 通路，低=AM 通路） |
| **PF0(SDA) / PF1(SCL)** | OLED 显示屏（SSD1306, 128×64, I2C） |
| **PB0(FSYNC) / PB1(SCLK) / PB2(SDATA)** | AD9833 DDS 模块（输出解调信号） |

### 信号通路

```
输入信号 ─→ 混频器 ─→ 500kHz LPF ─→ 继电器 ─┬─ NC(低电平) → 检波器 → PC0(AM)
                                              └─ NO(高电平) → 偏置电路 → PA0(FM)
```

### 电源

| 模块 | 供电 |
|---|---|
| AD9833 | 5V |
| OLED | 3.3V |
| 继电器 | 3.3V（控制信号 PE6） |

---

## 功能

| 模式 | OLED 显示 | 说明 |
|---|---|---|
| **AM** | `ma: XX%` / `fm: X.XXkHz` | 调幅度 + 调制频率 |
| **FM** | `mf: X.XX` / `df: X.XXkHz` / `fm: X.XXkHz` | 调频度 + 最大频偏 + 调制频率 |
| **CW** | `CW Signal (Unmodulated)` | 未调载波自动识别 |

- **AD9833**：自动输出与调制频率同步的正弦波
- **VOFA+**：FireWater 协议，115200 波特率，实时显示 ADC 波形

---

## 软件架构

### 核心算法

| 测量项 | 方法 |
|---|---|
| AM 调制度 ma | FFT 频域法：取调制频率分量的幅值 / DC 分量 |
| FM 调频度 mf | 过零频率计法：统计瞬时频率标准差，mf = Δf_peak / fm |
| 调制频率 fm | 1024 点 Radix-2 FFT + 抛物线插值 |
| 模式切换 | PA15 下降沿触发，volatile fm_mode 防编译器优化 |

### 关键参数（`Core/Src/main.c`）

```c
#define MA_CALIB    2.0f    // AM 调制度校准系数
#define FM_SENS     22.0f   // FM 鉴频灵敏度（Hz/ADC）
#define MEASURE_FM  1       // 0=AM, 1=FM（编译时默认）
```

### 编译器注意事项

ARMCLANG V6.16 在 -O4 优化下存在分支预测 bug，AM/FM 路径放在两个独立 `if` 块中，`fm_mode` 声明为 `volatile` 以绕过优化。

---

## 使用说明

1. **上电**：默认 AM 模式，OLED 显示 `ma` 和 `fm`
2. **按 PA15**：切换到 FM 模式，OLED 显示 `mf`、`df`、`fm`
3. **再按 PA15**：切回 AM 模式
4. **信号发生器**：
   - AM：载波 10MHz，调制频率 1~5kHz，调制度 20%~100%
   - FM：载波 10MHz，调制频率 3~5kHz，频偏 3~30kHz（mf 1~6）
5. **VOFA+**：协议 FireWater，波特率 115200

---

## 已知限制

- **AM 调制度精度**：继电器触点增加了检波器输出阻抗，低调制时信噪比下降。50% 附近精度最佳
- **PA15 按键**：需在 Keil 调试设置中将 ST-Link 端口设为 **SW**（非 JTAG），否则 PA15 被 JTAG 占用

---

## 文件结构

```
ADC/
├── Core/
│   ├── Inc/
│   │   ├── main.h / adc.h / dma.h / gpio.h / tim.h / usart.h
│   │   └── oled.h
│   └── Src/
│       ├── main.c          # 主程序（OLED、按键、AM/FM 逻辑）
│       ├── adc.c           # ADC1 DMA 配置（PA0）
│       ├── dma.c / gpio.c / tim.c / usart.c
│       └── stm32f4xx_it.c
├── Hardware/
│   ├── AD9833.h            # DDS 模块驱动
│   └── AD9833.c
├── Drivers/                # STM32 HAL 库
└── MDK-ARM/
    └── 2.uvprojx           # Keil 工程文件
```

---

## 编译 & 下载

Keil MDK 打开 `MDK-ARM/2.uvprojx`，编译后通过 ST-Link 下载到 STM32F407 开发板。
