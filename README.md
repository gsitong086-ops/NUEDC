# 串口屏按键 → STM32 代码 对接指南

## 一条链路讲清楚

```
你在屏幕按 [基础]
       ↓
屏幕弹起事件: printh 01     ← 你在 USART HMI 里写的
       ↓ 串口 TX 发出
STM32 的 RX 收到 0x01       ← 一个字节
       ↓ 中断自动存入环形缓冲区
主循环 tjc_read_cmd() 读到 0x01
       ↓
process_command(0x01)
       ↓
switch 跳到 case CMD_BASIC:
       ↓
执行你的代码 ← 这里就是你要写的地方
```

---

## 你加一个按钮的操作步骤

假设你要加一个 [开始测量] 按钮：

### 第一步：USART HMI

拖一个按钮 → 弹起事件写：

```
printh 31
```

`31` 是你给按钮分配的编号，= 页3 + 按钮0。

### 第二步：Keil main.c

在 `process_command()` 的 `switch` 里加：

```c
case 0x31:
    My_Start_Measure();          // ← 你已有的函数，直接调用
    break;
```

就这样，**屏幕按钮就跳转到你的函数了**。

---

## 一个完整例子

假设你已有一个 `My_Signal_Generator.c` 实现了信号发生器，有这些函数：

```c
void SigGen_SetWave(uint8_t type);   // 0=正弦 1=方波
void SigGen_SetFreq(uint32_t hz);
void SigGen_Start(void);
void SigGen_Stop(void);
```

在 main.c 开头加上：

```c
#include "My_Signal_Generator.h"
```

然后在 `process_command()` 里：

```c
case 0x11:                                    // 屏幕按了 [正弦]
    SigGen_SetWave(0);
    tjc_send_string("t2.txt=\"SINE\"");       // 屏幕显示反馈
    break;

case 0x12:                                    // 屏幕按了 [方波]
    SigGen_SetWave(1);
    tjc_send_string("t2.txt=\"SQUARE\"");
    break;

case 0x16:                                    // 屏幕按了 [开始/停止]
    SigGen_Start();
    tjc_send_string("t3.txt=\"ON\"");
    break;
```

---

## 屏幕文字回显

刚才的例子最后一行 `tjc_send_string("t2.txt=\"SINE\"")` 就是**让屏幕显示变化**。

发送的是淘晶驰的指令格式：

```
控件名.属性="值" + \xFF\xFF\xFF
```

`\xFF\xFF\xFF` 由 `tjc_send_string()` 自动追加，你不需要管。

常用：

```c
tjc_send_val("n0", "val", 3000);           // 数字控件 n0 显示 3000
tjc_send_string("t0.txt=\"已完成\"");       // 文本控件 t0 显示 "已完成"
tjc_send_string("t1.bco=GREEN");           // t1 背景变绿
tjc_switch_page(PAGE_COVER);               // 返回封面
```

---

## 实时波形数据

比如你 ADC 采样了一个信号，要实时画在屏幕上：

```c
// 在 while(1) 或定时器中断里
uint16_t val = ADC_Read();                          // 你的 ADC 值
uint16_t scaled = val * 320 / 4096;                 // 缩放到屏幕高度
tjc_wave_add(1, scaled);                            // 画到屏幕 s0 控件
if (++point_count >= 256) {
    point_count = 0;
    tjc_wave_clear(1);                               // 满了清屏
}
```

前提是 USART HMI 里放了 s0 波形控件。

---

## 总结

| 步骤 | 在哪做 | 做什么 |
|------|--------|--------|
| ① | USART HMI | 按钮弹起事件写 `printh XX` |
| ② | main.c 的 `process_command()` | 加 `case 0xXX: 你的函数(); break;` |
| ③ | 你的函数里 | 用 `tjc_send_xxx()` 更新屏幕显示 |

你要做的就是：**把 XX 当做按钮的唯一身份证号，case 里面去调你写好的函数。**
