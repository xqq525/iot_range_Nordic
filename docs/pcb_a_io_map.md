# PCB-A nRF54L15 QFN48 IO 分配表

> 来源：`测距传感器nRF54L15_QFN48_IO配置功能文件_v1.0.docx` V1.3  
> 提取日期：2026-06-23  
> 主控：Nordic nRF54L15 QFN48 | 供电 3.3V | BLE + NFC

---

## 1. 固定功能引脚

| QFN48 脚号 | nRF54L15 引脚 | 原理图网络名 | 方向 | 功能 |
|-----------|---------------|-------------|------|------|
| 1 | P1.00 / XL1 | OSC32IN | 输入 | 32.768kHz 晶振 |
| 2 | P1.01 / XL2 | OSC32OUT | 输出 | 32.768kHz 晶振 |
| 3 | P1.02 / NFC1 | NFC1 | 双向 | NFC 天线 |
| 4 | P1.03 / NFC2 / CK | NFC2 | 双向 | NFC 天线 |
| 25 | SWDIO | SWDIO | 双向 | 调试/烧录 |
| 26 | SWDCLK | SWDCLK | 输入 | 调试/烧录 |
| 30 | nRESET | NRESET | 输入 | 硬件复位（测试点） |
| 31 | ANT | BT_ANT1 | RF | 2.4GHz BLE 天线 |
| 34 | XC1 | OSC1IN | 输入 | 32MHz 晶振 |
| 35 | XC2 | OSC1OUT | 输出 | 32MHz 晶振 |

---

## 2. Port 0 — 外设分配

| QFN48 脚号 | IO | 网络名 | 方向 | 外设/功能 | 实现方式 | 备注 |
|-----------|----|--------|------|----------|---------|------|
| 23 | P0.00 | NTC_INT | 输入 | GPIO Wake | 硬件 GPIO | TP2021 温度报警中断，低功耗唤醒 |
| 24 | P0.01 | REST_KEY | 输入 | GPIO Wake | 软件复位按键 | 低有效，软件去抖 20~50ms |
| 27 | P0.02 | ACC_INT | 输入 | GPIO Wake | 硬件 GPIO | BMA510 中断唤醒 |
| 28 | P0.03 / CK | ACC_I2C_SDA | 双向 | TWIM30 SDA | 硬件 I2C | BMA510 I2C 数据线，上拉 3.3V |
| 29 | P0.04 / CK | ACC_I2C_SCL | 输出 | TWIM30 SCL | 硬件 I2C | BMA510 I2C 时钟线 |

---

## 3. Port 1 — 外设分配

| QFN48 脚号 | IO | 网络名 | 方向 | 外设/功能 | 实现方式 | 备注 |
|-----------|----|--------|------|----------|---------|------|
| 5 | P1.04 / AIN0 / CK | NTC_ADC | 输入 | SAADC AIN0 | 硬件 ADC | NTC 分压采样 |
| 6 | P1.05 / AIN1 | VBAT_ADC | 输入 | SAADC AIN1 | 硬件 ADC | 电池电压采样 |
| 7 | P1.06 / AIN2 | US_RX | 输入 | GPIO 软件 UART RX | 软件 UART | 超声波模块 TX → MCU RX |
| 8 | P1.07 / AIN3 | US_TX | 输出 | GPIO 软件 UART TX | 软件 UART | MCU TX → 超声波模块 RX |
| 9 | P1.08 / EXTREF / CLK16M / CK | LED_PWM | 输出 | GPIO / 软件时序 | WS2812B LED | W2812B 数据脚 |
| 37 | P1.09 | MK_EN | 输出 | GPIO | 电源控制 | 默认关断，通信/测量前打开 |
| 38 | P1.10 | CC_I2C_SDA | 双向 | TWIM22 SDA | 硬件 I2C | ATECC608C I2C 数据线 |
| 39 | P1.11 / AIN4 / CK | CC_I2C_SCL | 输出 | TWIM22 SCL | 硬件 I2C | ATECC608C I2C 时钟线 |
| 40 | P1.12 / AIN5 / CK | TOF_I2C_SCL | 输出 | TWIM20 SCL | 硬件 I2C | TOF 传感器 I2C 时钟线 |
| 41 | P1.13 / AIN6 | TOF_I2C_SDA | 双向 | TWIM20 SDA | 硬件 I2C | TOF 传感器 I2C 数据线 |
| 42 | P1.14 / AIN7 | TOF_INT | 输入 | GPIO Wake / 中断 | 硬件 GPIO | TOF 中断输入 |

---

## 4. Port 2 — 外设分配

| QFN48 脚号 | IO | 网络名 | 方向 | 外设/功能 | 实现方式 | 备注 |
|-----------|----|--------|------|----------|---------|------|
| 11 | P2.00 | LPWA_POWER | 输出 | GPIO | 电源开机控制 | 控制 LPWA PWRKEY 时序 |
| 12 | P2.01 / CK | SPI1_CLK_FLASH | 输出 | SPIM00 SCK | 硬件 SPI | W25Q32JVSSIQ 时钟线 |
| 13 | P2.02 | SPI1_MOSI_FLASH | 输出 | SPIM00 MOSI | 硬件 SPI | MCU → Flash DI/IO0 |
| 14 | P2.03 | LPWA_EN | 输出 | GPIO | 电源控制 | LPWA 电源开关使能 |
| 15 | P2.04 | SPI1_MISO_FLASH | 输入 | SPIM00 MISO | 硬件 SPI | Flash DO/IO1 → MCU |
| 16 | P2.05 | SPI1_CS_FLASH | 输出 | SPIM00 CSN / GPIO CS | 硬件 SPI | Flash 片选 |
| 17 | P2.06 / CK | LPWA_PSM | 输出 | GPIO | PSM 控制 | PON_TRIG / PSM 相关逻辑 |
| 18 | P2.07 / SWO | MAIN_LPWA_RXD | 输入 | UARTE21 RXD | 硬件 UART | LPWA_TXD → MCU_RXD，与 SWO 复用 |
| 19 | P2.08 | MAIN_LPWA_TXD | 输出 | UARTE21 TXD | 硬件 UART | MCU_TXD → LPWA_RXD |
| 20 | P2.09 | MAIN_GNSS_TXD | 输出 | GPIO 软件 UART TX | 软件 UART | 预留，~1 月/次低速通信 |
| 21 | P2.10 | MAIN_GNSS_RXD | 输入 | GPIO 软件 UART RX | 软件 UART | 预留，不可做唤醒源 |

---

## 5. 串行 IP 块分配总结

| 功能模块 | 外设实例 | 使用引脚 | 实现方式 |
|---------|---------|---------|---------|
| BMA510 加速度计 I2C | TWIM30 | P0.04 SCL / P0.03 SDA | 硬件 I2C |
| TOF 传感器 I2C | TWIM20 | P1.12 SCL / P1.13 SDA | 硬件 I2C |
| ATECC608C 加密芯片 I2C | TWIM22 | P1.11 SCL / P1.10 SDA | 硬件 I2C |
| W25Q32JVSSIQ Flash SPI | SPIM00 | P2.01 SCK / P2.02 MOSI / P2.04 MISO / P2.05 CS | 硬件 SPI |
| LPWA 主串口 | UART21 | P2.08 TXD / P2.07 RXD | 硬件 UART |
| 超声波模块串口 | GPIO bit-bang | P1.07 TX / P1.06 RX | 软件 UART |
| GNSS 定位串口 | GPIO bit-bang | P2.09 TX / P2.10 RX | 软件 UART（预留） |

---

## 6. 低功耗状态建议

| 场景 | 涉及 IO | 建议状态 |
|------|--------|---------|
| 系统休眠 / MK_VCC 关闭 | TOF_I2C、CC_I2C、US_TX、LED_PWM | 输入高阻 / 无内部上拉 |
| Flash 不访问 | SPI1_CLK/MOSI/MISO/CS | Flash Deep Power-down，CS 无效 |
| LPWA 关闭 | LPWA_POWER/EN/PSM、MAIN_LPWA_TXD/RXD | 按默认态，UART 高阻 |
| GNSS 不使用 | MAIN_GNSS_TXD/RXD | GPIO 输入高阻 |
| Reset_Key | P0.01 | 输入，上拉，低有效，唤醒检测 |

---

## 7. 关键备注

- P2.07 与 SWO 复用 → 使用 LPWA 主串口时禁用 SWO Trace
- P2.10 无 GPIOTE → 不可做唤醒源，只能轮询
- P2 整个端口无 GPIOTE → P2 信号不可用于中断唤醒
- SPI Flash MOSI/MISO 已按正确方向修正（V1.3）
- Reset_Key 接 P0.01（软件复位），不接 nRESET（保留为硬件测试点）
- MK_EN 控制模块电源域，默认关断
- NFC P1.02/P1.03 是固定功能引脚，启用 NFC 后不可作普通 GPIO
