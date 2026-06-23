# PCB-A 引脚配置核对审计

> 核对日期：2026-06-23  
> 核对范围：`prj.conf` / `boards/*.overlay` / `ble_uart.c` / `main.c` / DK 默认设备树  
> 参考文档：[pcb_a_io_map.md](pcb_a_io_map.md)  

---

## 1. 当前工程已启用的外设

### 1.1 从 prj.conf 解析

| 功能 | 配置项 | 状态 |
|------|--------|:----:|
| NFC | `CONFIG_NFC_T4T_NRFXLIB=y` | ✅ 启用 |
| BLE | `CONFIG_BT=y` `CONFIG_BT_PERIPHERAL=y` | ✅ 启用 |
| DK 库（按键+LED） | `CONFIG_DK_LIBRARY=y` | ⚠️ 启用（冲突） |
| UART 控制台 | `CONFIG_UART_CONSOLE=y` | ⚠️ 启用（冲突） |
| Shell CLI | `CONFIG_SHELL=y` `CONFIG_SHELL_BACKEND_SERIAL=y` | ⚠️ 依赖 UART |
| RTT 日志 | `CONFIG_USE_SEGGER_RTT=y` `CONFIG_LOG_BACKEND_RTT=y` | ✅ 可用 |
| Flash | `CONFIG_FLASH=y` `CONFIG_FLASH_PAGE_LAYOUT=y` | ✅ (NDEF 存储) |

### 1.2 从 overlay 解析

- **当前 overlay：** `nrf54h20dk_nrf54h20_cpuapp.overlay`（仅 nRF54H20 平台，与 nRF54L15 无关）
- **nRF54L15 用的 overlay：** 不存在。工程完全依赖 SDK 默认设备树

### 1.3 从代码使用解析

| 代码位置 | 使用的 DK 符号 | 对应 DK 引脚 | 实际功能 |
|----------|---------------|-------------|---------|
| [main.c:31-33](src/main.c#L31-L33) | `DK_LED1` | P2.09 | NFC 场检测指示灯 |
| [main.c:32](src/main.c#L32) | `DK_LED2` | P1.10 | NFC 写入指示灯 |
| [main.c:33](src/main.c#L33) | `DK_LED4` | P1.14 | NFC 读取指示灯 |
| [ble_uart.c:34](src/ble_uart.c#L34) | `DK_LED3` | P2.07 | BLE 连接指示 |
| [main.c:36](src/main.c#L36) | `DK_BTN1_MSK` | P1.13 | 恢复默认 NDEF |

---

## 2. DK 默认引脚 vs PCB-A 冲突清单

### 2.1 LED 冲突（4/4 全冲突）

| DK LED | DK 引脚 | PCB-A 网络名 | PCB-A 功能 | 冲突后果 |
|--------|--------|-------------|-----------|---------|
| `DK_LED1` | P2.09 | MAIN_GNSS_TXD | GNSS 软件 UART TX | LED 翻转会向 GNSS 发噪声 🟡 |
| `DK_LED2` | P1.10 | CC_I2C_SDA | ATECC608C I2C SDA | LED 翻转会干扰 I2C 🔴 |
| `DK_LED3` | P2.07 | MAIN_LPWA_RXD | LPWA 硬件 UART RXD | LED 翻转干扰 LPWA 接收 🔴 |
| `DK_LED4` | P1.14 | TOF_INT | TOF 中断输入 | LED 输出 vs 中断输入冲突 🔴 |

### 2.2 按键冲突（4/4 全冲突）

| DK Button | DK 引脚 | PCB-A 网络名 | PCB-A 功能 | 冲突后果 |
|----------|--------|-------------|-----------|---------|
| `DK_BTN1` | P1.13 | TOF_I2C_SDA | TOF I2C SDA | GPIO 读按键干扰 I2C 🔴 |
| `DK_BTN2` | P1.09 | MK_EN | 模块电源使能 | 按键检测会意外使能/关断电源 🔴 |
| `DK_BTN3` | P1.08 | LED_PWM | WS2812B LED 数据 | GPIO 读按键干扰 LED 时序 🟡 |
| `DK_BTN4` | P0.04 | ACC_I2C_SCL | BMA510 I2C SCL | GPIO 读按键干扰 I2C 🔴 |

### 2.3 DK 串口冲突

| DK 外设 | DK 引脚 | PCB-A 网络名 | PCB-A 功能 | 冲突后果 |
|---------|--------|-------------|-----------|---------|
| uart20 TX | P1.04 | NTC_ADC | NTC 温度采样 | UART TX 干扰 ADC 🔴 |
| uart20 RX | P1.05 | VBAT_ADC | 电池电压采样 | UART RX 干扰 ADC 🔴 |
| uart20 RTS | P1.06 | US_RX | 超声波 UART RX | RTS 输出干扰串口接收 🔴 |
| uart20 CTS | P1.07 | US_TX | 超声波 UART TX | CTS 输入读取串口发送 🔴 |
| shell-uart | =&uart20 | 同上 | 同上 | Shell 回显干扰全 P1.04-P1.07 🔴 |

### 2.4 DK 其他 pin ctrl 冲突

| DK 外设 | DK 引脚 | PCB-A 网络名 | PCB-A 功能 | 评估 |
|---------|--------|-------------|-----------|:--:|
| pwm20 OUT0 | P1.10 | CC_I2C_SDA | ATECC608C I2C | 🔴 |
| grtc CLKOUT_FAST | P1.08 | LED_PWM | WS2812B LED 数据 | 🔴 |
| grtc CLKOUT_32K | P0.04 | ACC_I2C_SCL | BMA510 I2C SCL | 🔴 |
| spi22 SCK | P1.11 | CC_I2C_SCL | ATECC608C I2C | 🟡（未启用时无害） |
| spi22 MISO | P1.07 | US_TX | 超声波 TX | 🟡 |
| spi22 MOSI | P1.06 | US_RX | 超声波 RX | 🟡 |
| spi00 SCK/MOSI/MISO | P2.01/02/04 | SPI1_CLK/MOSI/MISO_FLASH | Flash SPI | ✅ 一致 |

---

## 3. 无冲突外设

| 功能 | 引脚 | 状态 |
|------|------|:----:|
| NFC | P1.02 / P1.03 | ✅ 固定功能，与 PCB-A 完全一致 |
| BLE | ANT（内部射频） | ✅ 不走 GPIO，无引脚冲突 |
| Flash SPIM00 | P2.01/02/04/05 | ✅ DK 默认与 PCB-A 一致 |

---

## 4. 当前状态结论

**若直接在 PCB-A 板上运行当前固件（不添加 overlay），会导致：**

1. **NFC** — 正常工作（P1.02/P1.03 固定功能）
2. **BLE** — 正常工作（内部射频）
3. **4 个 LED** — 全部输出到 PCB-A 业务引脚（I2C/UART/GPIO），可能损坏外设或导致通信异常
4. **按键检测** — 读取 I2C 总线/MK_EN/LED 时序引脚状态，可能意外触发复位或改变电源状态
5. **UART 控制台** — 在 NTC/VBAT ADC 引脚上输出 UART 信号，ADC 读数异常
6. **Shell CLI** — 与 UART 控制台绑定，同上冲突

**一句话：当前固件在 PCB-A 上 NFC + BLE 功能可工作，但 DK_LIBRARY 和 UART_CONSOLE 未禁用，GPIO 乱翻存在风险。**

---

## 5. 最小修改建议（NFC + BLE 调试阶段）

### 5.1 原则

- **只留 NFC 和 BLE** 两个可调试功能
- **禁用所有冲突的 DK 默认外设**（LED/按键/串口控制台/pinctrl）
- **不做删除，只用覆盖方式关掉**
- **其他外设的引脚信息保留为预留注释**

### 5.2 方案：新建 `boards/nrf54l15dk_nrf54l15_cpuapp.overlay`

```dts
/*
 * PCB-A nRF54L15 引脚覆盖 — NFC + BLE 调试阶段
 * 其余外设引脚仅做预留说明，暂不启用。
 */

/* ── 删除 DK 默认 LED / 按钮 / 别名（覆盖为空） ── */

/delete-node/ &leds;
/delete-node/ &buttons;

/ {
    /delete-property/ aliases;
};

/* ── 删除 DK 默认 pinctrl 引用（释放引脚） ── */
/* uart20 释放 P1.04~P1.07（PCB-A: NTC_ADC, VBAT_ADC, US_RX, US_TX）*/
&uart20 {
    /delete-property/ pinctrl-0;
    /delete-property/ pinctrl-1;
    /delete-property/ pinctrl-names;
    status = "disabled";
};

/* pwm20 释放 P1.10（PCB-A: CC_I2C_SDA — ATECC608C）*/
&pwm20 {
    /delete-property/ pinctrl-0;
    /delete-property/ pinctrl-1;
    /delete-property/ pinctrl-names;
    status = "disabled";
};

/* spi22 释放 P1.06/P1.07/P1.11（暂不使用）*/
&spi22 {
    /delete-property/ pinctrl-0;
    /delete-property/ pinctrl-1;
    /delete-property/ pinctrl-names;
    status = "disabled";
};

/* gpiote20 — 释放 GRTC pin ctrl 冲突（P1.08, P0.04）*/
&gpiote20 {
    /delete-property/ pinctrl-0;
    /delete-property/ pinctrl-1;
    /delete-property/ pinctrl-names;
};

/* ── 保留已确认一致的外设 ── */
/* Flash SPIM00 无需覆盖 — DK 默认与 PCB-A 一致 */

/* ── NFC 显式使能 ── */
&nfct {
    status = "okay";
};

/* ── 预留外设（文档说明，不启用） ── */
/*
 * 以下外设的引脚分配已记录在 docs/pcb_a_io_map.md，待调试通过后再启用：
 *
 *   TWIM30 (BMA510):  P0.04 SCL / P0.03 SDA
 *   TWIM20 (TOF):     P1.12 SCL / P1.13 SDA
 *   TWIM22 (ATECC):   P1.11 SCL / P1.10 SDA
 *   UART21 (LPWA):    P2.08 TXD / P2.07 RXD
 *   SAADC:            P1.04 (NTC) / P1.05 (VBAT)
 *   软件 UART:         P1.06/P1.07 (超声波), P2.09/P2.10 (GNSS)
 *   GPIO:             P0.00 (NTC_INT), P0.01 (REST_KEY), P0.02 (ACC_INT)
 *                     P1.08 (LED_PWM), P1.09 (MK_EN), P1.14 (TOF_INT)
 *                     P2.00 (LPWA_POWER), P2.03 (LPWA_EN), P2.06 (LPWA_PSM)
 */
```

### 5.3 方案：修改 `prj.conf`

```diff
-# DK buttons and LEDs
-CONFIG_DK_LIBRARY=y
+# DK buttons and LEDs — 禁用（PCB-A 无 DK 外设，所有 DK 引脚已分配给业务功能）
+# CONFIG_DK_LIBRARY=y

-# UART 控制台 & Shell（PCB-A 无空闲 UART 用于调试串口）
-CONFIG_UART_CONSOLE=y
+# CONFIG_UART_CONSOLE=y

-# Shell CLI
-CONFIG_SHELL=y
-CONFIG_SHELL_BACKEND_SERIAL=y
+# Shell CLI — 禁用（依赖 UART 控制台）
+# CONFIG_SHELL=y
+# CONFIG_SHELL_BACKEND_SERIAL=y

-# CONFIG_SERIAL=y  # 保留，Flash/NFC 可能间接依赖
```

> **注意：** RTT 日志（`CONFIG_USE_SEGGER_RTT=y`）已配置，无需 UART 控制台即可通过 J-Link 查看日志。

### 5.4 方案：修改 `main.c`

需要处理 `dk_leds_init()` 和 `dk_buttons_and_leds.h` 相关的宏。两种方案：

**方案 A（最小侵入 — 推荐）**：添加条件编译宏

```c
// main.c 顶部
#ifdef CONFIG_DK_LIBRARY
#include <dk_buttons_and_leds.h>
#define NFC_FIELD_LED    DK_LED1
#define NFC_WRITE_LED    DK_LED2
#define NFC_READ_LED     DK_LED4
#define NDEF_RESTORE_BTN_MSK  DK_BTN1_MSK
#else
// PCB-A 无 DK LED — 暂用空宏替代
#define NFC_FIELD_LED    0
#define NFC_WRITE_LED    0
#define NFC_READ_LED     0
#define NDEF_RESTORE_BTN_MSK  0
static inline int dk_leds_init(void) { return 0; }
static inline void dk_set_led_on(uint8_t n) { (void)n; }
static inline void dk_set_led_off(uint8_t n) { (void)n; }
static inline void dk_read_buttons(uint32_t *s, uint32_t *c) { *s=0; if(c)*c=0; }
#endif
```

**方案 B（更简单 — 首版调试推荐）**：直接移除 LED/按键引用

直接在 main.c 中注释或删除 `dk_*` 调用行。代码简单但改动涉及多处。

---

## 6. 核对总结

| 检查项 | 结论 |
|--------|:----:|
| NFC 引脚 (P1.02/P1.03) | ✅ 一致 |
| BLE 射频 (ANT) | ✅ 内部，无冲突 |
| Flash SPIM00 (P2.01/02/04/05) | ✅ DK 默认与 PCB-A 一致 |
| DK LED0 (P2.09) | 🔴 冲突 PCB-A MAIN_GNSS_TXD |
| DK LED1 (P1.10) | 🔴 冲突 PCB-A CC_I2C_SDA |
| DK LED2 (P2.07) | 🔴 冲突 PCB-A MAIN_LPWA_RXD |
| DK LED3 (P1.14) | 🔴 冲突 PCB-A TOF_INT |
| DK BTN0 (P1.13) | 🔴 冲突 PCB-A TOF_I2C_SDA |
| DK BTN1 (P1.09) | 🔴 冲突 PCB-A MK_EN |
| DK BTN2 (P1.08) | 🔴 冲突 PCB-A LED_PWM |
| DK BTN3 (P0.04) | 🔴 冲突 PCB-A ACC_I2C_SCL |
| DK UART20 (P1.04-P1.07) | 🔴 冲突 PCB-A ADC/US |
| DK PWM20 (P1.10) | 🔴 冲突 PCB-A CC_I2C_SDA |
| DK GRTC (P1.08, P0.04) | 🔴 冲突 PCB-A LED_PWM, ACC_I2C_SCL |

**冲突率：12/14 DK 默认外设与 PCB-A 冲突。仅 Flash SPI 和 NFC 一致。**

### 最小可行的修改范围

| 文件 | 操作 | 说明 |
|------|------|------|
| `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` | **新建** | 删除 DK LED/按键节点，禁用冲突 pinctrl，禁用 uart20/pwm20 |
| `prj.conf` | **修改 4 行** | 注释 `CONFIG_DK_LIBRARY` `CONFIG_UART_CONSOLE` `CONFIG_SHELL` `CONFIG_SHELL_BACKEND_SERIAL` |
| `main.c` | **修改 ~15 行** | 用条件编译或 stub 替代 `dk_*` 宏/函数 |
| `ble_uart.c` | **可能需要小块修改** | `DK_LED3` 和 `DK_BTN*` 引用 |

> **注意：以上建议为只读审计输出，用户确认后再实施。**
