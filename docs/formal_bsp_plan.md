# 正式工程 BSP 设计草案

> 版本：v0.1-draft  
> 日期：2026-06-23  
> 主控：Nordic nRF54L15 QFN48  
> 参考：`测距传感器nRF54L15_QFN48_IO配置功能文件_v1.0.docx` V1.3  
> 前置文档：[pcb_a_io_map.md](pcb_a_io_map.md) | [pcb_a_pin_audit.md](pcb_a_pin_audit.md)

---

## 1. 概述

### 1.1 文档目的

本文档为正式工程的 BSP（Board Support Package）层设计草案，定义 overlay 文件架构、中断源映射、时钟源、电源域和外设启用顺序。不包含具体代码实现，仅给出结构设计和决策依据。

### 1.2 当前阶段

- **Phase 0（当前）**：writable_ndef_msg 测试工程，仅启用 NFC + BLE，用于 App 联调
- **Phase 1（下一步）**：正式工程建立，基于本文档实施完整 BSP

### 1.3 PCB 变体

| 变体 | 状态 | IO 配置文档 | 说明 |
|------|:----:|------------|------|
| PCB-A | ✅ 已确认 | `测距传感器nRF54L15_QFN48_IO配置功能文件_v1.0.docx` | 当前唯一目标板 |
| PCB-B | ⬜ 待定 | 无 | 引脚分配未输出，与 PCB-A 差异未知 |

> **注意：** PCB-B 的文档未找到，本文档中 PCB-B 相关内容仅为结构模板。需 PCB-B 的 IO 配置文件到位后才能填充。

---

## 2. Overlay 文件架构

### 2.1 文件组织

```
正式工程/
└── boards/
    ├── pcb_a_nrf54l15_cpuapp.overlay       # PCB-A 完整 BSP
    ├── pcb_a_nrf54l15_cpuapp.conf          # PCB-A Kconfig
    ├── pcb_b_nrf54l15_cpuapp.overlay       # PCB-B 完整 BSP（待定）
    └── pcb_b_nrf54l15_cpuapp.conf          # PCB-B Kconfig（待定）
```

**设计决策**：不沿用 DK 板名 `nrf54l15dk`，使用自定义板名 `pcb_a`/`pcb_b` 区分变体。需要：
- 在 `boards/` 下添加板级定义文件（`board.cmake`、`Kconfig.board` 等）
- 或者在 CMakeLists.txt 中通过 `-DBOARD_ROOT` 指定自定义板目录
- **替代方案**：保持 `nrf54l15dk` 板名，通过构建参数选择不同 overlay（`-DDTC_OVERLAY_FILE=...`）。简单但无法自动切换 Kconfig。

### 2.2 Overlay 分层结构

每个 PCB 的 overlay 按以下顺序组织：

```
┌──────────────────────────────────────────────┐
│ 1. 删除 DK 默认节点（LED/按键/别名）          │
├──────────────────────────────────────────────┤
│ 2. 禁用冲突的 DK 外设（uart20/pwm20/spi22）  │
├──────────────────────────────────────────────┤
│ 3. 使能与 PCB 一致的 SDK 外设（NFC/BLE/GPIO）│
├──────────────────────────────────────────────┤
│ 4. 配置 PCB 专用 pinctrl（按串行 IP 块分组）  │
├──────────────────────────────────────────────┤
│ 5. 配置外设节点（I2C/SPI/UART/ADC 设备树）   │
├──────────────────────────────────────────────┤
│ 6. 配置 GPIO 设备（使能脚/中断脚/LED）        │
├──────────────────────────────────────────────┤
│ 7. 清理 chosen 节点（控制台/Shell/mcumgr）    │
└──────────────────────────────────────────────┘
```

### 2.3 `.conf` 文件结构

```
┌──────────────────────────────────────────────┐
│ 1. 电源管理（PM_DEVICE / PM / POWEROFF）     │
├──────────────────────────────────────────────┤
│ 2. 调试（RTT Console / Logging）             │
├──────────────────────────────────────────────┤
│ 3. 总线驱动（I2C / SPI / UART）              │
├──────────────────────────────────────────────┤
│ 4. 外设驱动（NFC / BLE / ADC / PWM）         │
├──────────────────────────────────────────────┤
│ 5. 子系统（Settings / Flash / 看门狗）        │
├──────────────────────────────────────────────┤
│ 6. 协议栈配置（BT MTU / 缓冲区 / 安全）       │
└──────────────────────────────────────────────┘
```

---

## 3. 时钟源配置

### 3.1 时钟树

```
nRF54L15 时钟树（PCB-A）

32.768kHz 外部晶振（P1.00/XL1, P1.01/XL2）
  ├── LFCLK → RTC / WDT / 低功耗定时器
  ├── BLE 定时（连接间隔/广告间隔）
  ├── NFC 场检测定时
  └── 系统低功耗唤醒定时器

32MHz 外部晶振（XC1, XC2）
  ├── HFCLK → CPU / 外设总线 / 高速外设
  ├── RADIO（BLE 2.4GHz PLL 参考）
  ├── SPI / I2C / UART 波特率生成
  └── NFC 内部逻辑时钟
```

### 3.2 LFCLK 源选择

| 源 | 精度 | 功耗 | 适用场景 |
|----|:----:|:----:|---------|
| 外部 32.768kHz XTAL | ±20ppm | 低 | **默认 — PCB-A 已有外部晶振** |
| 内部 RC | ±500ppm | 最低 | 仅无外部晶振的极低成本设计 |
| HFCLK 合成 | ±20ppm | 高（需 HF 运行） | 可作临时 fallback |

**决策**：PCB-A 使用外部 32.768kHz 晶振，在 `nrf54l_05_10_15_cpuapp_common.dtsi` 中已配置 `&lfxo { load-capacitors = "internal"; }`，无需在 overlay 中覆盖。如实际 PCB 负载电容不同，需覆盖 `load-capacitance-femtofarad`。

### 3.3 HFCLK 源选择

**决策**：使用外部 32MHz 晶振（`&hfxo`），DK 默认已配置，无需覆盖。

---

## 4. 电源域

### 4.1 nRF54L15 电源结构

```
VDD (3.3V) ── Main Supply

  ├── VREGMAIN (DC/DC 或 LDO) ── 内核 + 外设域
  │   ├── CPU (Cortex-M33)
  │   ├── RAM
  │   ├── 外设（SPI/I2C/UART/ADC/GPIO）
  │   └── GPIO P0/P1/P2（部分 P0 可常开唤醒）
  │
  ├── RADIO 域 ── BLE 2.4GHz PHY
  │   └── 独立于外设域，bt_disable() 可完全断电
  │
  └── NFC 域 ── NFC Tag 模拟前端
      ├── 可从 NFC 场取电（能量采集）
      └── 或从 VDD 供电
```

### 4.2 PCB-A 模块电源域

| 电源域 | 控制脚 | 供电模块 | 默认状态 |
|--------|--------|---------|:----:|
| VDD (常开) | 无 | nRF54L15、BMA510 (P0 I2C)、NTC | 常开 |
| MK_VCC | P1.09 MK_EN | TOF、ATECC608C、WS2812B、超声波 | **关断** |
| LPWA_VCC | P2.03 LPWA_EN | LPWA 模组 | **关断** |
| Flash_VCC | 无（常开或与 VDD 同域） | W25Q32JVSSIQ | 常开 |

### 4.3 系统功耗状态

| 状态 | CPU | 外设 | 唤醒源 | 典型功耗 |
|------|:---:|------|--------|:-------:|
| System ON — 全速 | Run | 全部使能 | — | ~5-10mA |
| System ON — 空闲 | WFI | 仅必要外设 | 任意中断 | ~1-2mA |
| System ON — BLE 连接休眠 | WFI | BLE LL + RTC | BLE 连接事件 | ~50-200µA |
| System OFF | 断电 | 仅常开域 GPIO | P0.00/P0.01/P0.02 唤醒 | ~1-2µA |

### 4.4 上电/掉电时序

```
上电流程:
VDD 稳定 → GPIO 默认输入 → Firmware 启动 → 配置引脚 →
  需要外设时: MK_EN 拉高 → 等待外设 LDO 稳定(≥1ms) → 初始化外设

掉电流程（进入休眠）:
外设进入低功耗 → MK_EN 拉低 → Flash Deep Power-down →
  LPWA_EN 拉低 → UART/SPI/I2C 引脚高阻 → 进入 System OFF/ON 休眠
```

---

## 5. 中断源与 GPIOTE 通道分配

### 5.1 nRF54L15 GPIOTE 资源

| GPIOTE 实例 | 关联端口 | 通道数 | 地址 |
|------------|---------|:------:|--------|
| GPIOTE20 | P1 (16 pins) | 8 | 0xda000 |
| GPIOTE30 | P0 (7 pins) | 8 | 0x10c000 |
| — | P2 (11 pins) | **0** | 不支持 |

> **关键限制**：P2 端口无 GPIOTE。所有需要中断/唤醒的信号必须分配到 P0 或 P1。

### 5.2 PCB-A 中断源分配

| 优先级 | 信号 | 引脚 | GPIOTE 通道 | 触发方式 | 用途 |
|:------:|------|------|:----------:|---------|------|
| 0 (最高) | REST_KEY | P0.01 | gpiote30 ch0 | 下降沿 latch | 软件复位（长按 3s） |
| 1 | NFC_FIELD | P1.02 | 内部 NFC 硬件 | 自动检测 | NFC 场检测 |
| 2 | ACC_INT | P0.02 | gpiote30 ch1 | 任意边沿 | BMA510 运动/震动唤醒 |
| 3 | TOF_INT | P1.14 | gpiote20 ch0 | 下降沿 | TOF 测距完成/异常 |
| 4 | NTC_INT | P0.00 | gpiote30 ch2 | 边沿 | 温度阈值报警 |

**剩余 GPIOTE 通道**：
- GPIOTE20：ch1-ch7 (7 channels free)
- GPIOTE30：ch3-ch7 (5 channels free)

### 5.3 中断优先级策略

```
ARM NVIC 优先级（0=最高）:

0:  RTC（蓝牙 LL 定时）
1:  RADIO（蓝牙 ISR）
2:  GPIOTE（按键/传感器唤醒）
3:  UART21（LPWA 数据接收）
4:  SPIM00（Flash DMA 完成）
5:  TWIM20/TWIM22/TWIM30（I2C 完成）
6:  SAADC（ADC 采样完成）
7:  RTT（调试日志，最低优先级）
```

### 5.4 P2 不可中断信号的处理

| P2 信号 | 功能 | 解决方案 |
|---------|------|---------|
| P2.07 MAIN_LPWA_RXD | LPWA UART 接收 | 使用硬件 UART21，其内部 FIFO 和 DMA 不依赖 GPIO 中断（UART 模块自带 RX 中断） |
| P2.10 MAIN_GNSS_RXD | GNSS 软件 UART | **必须轮询**。MCU 定时（如 RTC 闹钟每月一次）主动拉高 P2.09 发送指令，在固定时间窗口内轮询 P2.10 接收。不可期望 GNSS 模组主动唤醒 MCU。 |
| P2.06 LPWA_PSM | LPWA PSM 状态 | 只做输出控制，无需中断。若要检测模组状态，考虑通过 UART AT 命令查询 |

---

## 6. 串行 IP 块分配

### 6.1 nRF54L15 可用串行 IP

| IP 块 | 地址 | 可选模式 | PCB-A 分配 | PCB-B 分配 |
|-------|------|---------|-----------|-----------|
| SPIM00 / UART00 | 0x04a000 | SPI / UART | **SPIM00** — Flash SPI | 待定 |
| SPIM20 / UART20 / TWIM20 | 0x0c6000 | SPI / UART / I2C | **TWIM20** — TOF I2C | 待定 |
| SPIM21 / UART21 / TWIM21 | 0x0c7000 | SPI / UART / I2C | **UART21** — LPWA 主串口 | 待定 |
| SPIM22 / UART22 / TWIM22 | 0x0c8000 | SPI / UART / I2C | **TWIM22** — ATECC608C I2C | 待定 |
| SPIM30 / UART30 / TWIM30 | 0x104000 | SPI / UART / I2C | **TWIM30** — BMA510 I2C | 待定 |

### 6.2 软件 GPIO 外设（不占串行 IP）

| 外设 | 引脚 | 类型 | 说明 |
|------|------|------|------|
| 超声波模块 | P1.07 TX / P1.06 RX | 软件 UART | 9600bps，低频使用 |
| GNSS 模块 | P2.09 TX / P2.10 RX | 软件 UART | 预留，~1月/次 |
| WS2812B LED | P1.08 | 软件时序 | 800kHz bit-bang，关中断保护 |

### 6.3 分配原则

1. **高频/实时** → 硬件串行 IP（Flash SPI, LPWA UART, 传感器 I2C）
2. **低速/低频** → GPIO bit-bang（超声波、GNSS）
3. **时序严格** → 软件 bit-bang 时关全局中断（WS2812B 800kHz 需要 ±150ns 精度）
4. **I2C 隔离** → 三个 I2C 设备各占独立总线，地址冲突不影响，故障隔离

---

## 7. 外设启用顺序

### 7.1 启动初始化顺序

```
Phase 0: 硬件复位
  ├── nRESET 释放
  ├── 内部 LDO/DC-DC 启动
  └── LFCLK/HFCLK 起振

Phase 1: 基础 GPIO（启动后立即，~1ms）
  ├── 配置 MK_EN → 输出低（关断模块电源域）
  ├── 配置 LPWA_EN → 输出低（关断 LPWA）
  ├── 配置 LPWA_POWER → 输出低
  └── 配置其他 GPIO 为默认安全态（高阻或确定电平）

Phase 2: 系统内核（~10ms）
  ├── 日志系统初始化（RTT 后端）
  ├── Settings 子系统（Flash 持久化）
  └── 看门狗启动

Phase 3: NFC（~20ms）
  ├── NFC T4T 库初始化
  ├── NDEF 文件加载
  └── NFC 场检测使能（P1.02/P1.03 开始监听）

Phase 4: BLE（~50ms）
  ├── bt_enable() — 启动 BLE 协议栈 + RADIO
  ├── NUS 服务注册
  ├── 广播名称设置
  └── 广播启动

Phase 5: 传感器域上电（按需，~10ms）
  ├── MK_EN 拉高（使能 TOF/ATECC/超声波电源域）
  ├── 等待电源稳定 ≥1ms
  ├── TWIM20 初始化 → TOF 配置
  ├── TWIM22 初始化 → ATECC608C 唤醒 + 配置
  ├── 超声波 GPIO 初始化
  └── WS2812B 初始化

Phase 6: 通信模组（按需，~100ms）
  ├── LPWA_EN 拉高
  ├── LPWA_POWER 按模组时序拉动
  ├── 等待模组注册网络
  └── UART21 初始化 → AT 命令通道建立

Phase 7: 传感器数据采集（周期性）
  ├── SAADC 使能 → NTC + VBAT 采样
  ├── TWIM30 → BMA510 读取加速度
  ├── TOF 触发测距 → 等待 TOF_INT
  ├── 超声波触发 → 等待回波时间
  └── 数据聚合 → NDEF 消息更新
```

### 7.2 休眠/唤醒流程

```
进入休眠:
1. 完成当前业务周期
2. 传感器进入低功耗模式
3. MK_EN 拉低（关断传感器电源域）
4. LPWA_EN 拉低 / LPWA_POWER 拉低（关断 LPWA）
5. Flash Deep Power-down
6. 外设引脚配置为高阻或低功耗态（参考 pcb_a_io_map.md §6）
7. CONFIG_PM_DEVICE 管理的设备自动挂起
8. System OFF 或 System ON Idle

唤醒:
1. GPIO 唤醒（REST_KEY / ACC_INT / NTC_INT / NFC_FIELD）
2. 恢复时钟
3. 恢复外设引脚默认态
4. 按需重新初始化外设（Phase 5+）
5. 执行业务逻辑
6. 回到休眠判断
```

### 7.3 依赖关系图

```
RTT Console ── 无依赖，最先初始化
    │
Flash/Settings ── 依赖 Flash 驱动
    │
NFC ── 依赖 Settings（NDEF 文件加载）、无外部引脚依赖
    │
BLE ── 依赖 Settings（bonding 数据）、无外部引脚依赖
    │
I2C Buses ── 依赖 MK_EN（上电）、GPIO
    │
SPI Flash ── 独立，可直接初始化（Flash 常供电）
    │
UART ── 依赖 LPWA_EN/LPWA_POWER（上电）
    │
ADC ── 独立，可直接初始化
```

---

## 8. PCB-A 完整 Overlay 结构（设计版）

### 8.1 pinctrl 配置清单

```
需要新增的 pinctrl:

  uart21_default / uart21_sleep
    P2.08 TXD / P2.07 RXD（硬件 UART，无流控）

  twim20_default / twim20_sleep
    P1.12 SCL / P1.13 SDA（TOF I2C）

  twim22_default / twim22_sleep
    P1.11 SCL / P1.10 SDA（ATECC608C I2C）

  twim30_default / twim30_sleep
    P0.04 SCL / P0.03 SDA（BMA510 I2C）

需要保留的 DK pinctrl（已确认一致）:
  spi00_default / spi00_sleep — Flash SPIM00（P2.01/02/04/05）

需要禁用/删除的 DK pinctrl:
  uart20_default / uart20_sleep — 释放 P1.04~P1.07
  pwm20_default / pwm20_sleep — 释放 P1.10
  grtc_default / grtc_sleep — 释放 P1.08, P0.04
  spi22_default / spi22_sleep — 释放 P1.06/07/11
```

### 8.2 外设节点清单

```
需要新增的 peripheral node:

  &uart21 — LPWA 主串口
    绑定 uart21_default / uart21_sleep

  &i2c20 { compatible = "nordic,nrf-twim"; } — TOF
    绑定 twim20_default / twim20_sleep

  &i2c22 { compatible = "nordic,nrf-twim"; } — ATECC608C
    绑定 twim22_default / twim22_sleep

  &i2c30 { compatible = "nordic,nrf-twim"; } — BMA510
    绑定 twim30_default / twim30_sleep

需要修改的 DK node:

  &spi00 — 替换 mx25r64 为 w25q32jv
    compatible = "jedec,spi-nor"
    jedec-id = [ef 40 16]（W25Q32JV 标准 ID）
    size = <33554432>（32Mbit = 4MB）
    去掉 reset-gpios（PCB-A 无硬件 reset 脚连 Flash）

需要禁用的 DK node:

  &uart20 { status = "disabled"; };
  &pwm20 { status = "disabled"; };
  &spi22 { status = "disabled"; };
  &grtc { /delete-property/ pinctrl-0; ... };  // 仅清除 pinctrl
```

### 8.3 GPIO 设备树绑定

```dts
/* 独立 GPIO（不需要 pinctrl，直接 gpio-leds/gpio-keys 或应用层操作）*/

/* 电源控制 */
mk_en: mk-en { gpios = <&gpio1 9 GPIO_ACTIVE_HIGH>; };
lpwa_en: lpwa-en { gpios = <&gpio2 3 GPIO_ACTIVE_HIGH>; };
lpwa_power: lpwa-power { gpios = <&gpio2 0 GPIO_ACTIVE_HIGH>; };
lpwa_psm: lpwa-psm { gpios = <&gpio2 6 GPIO_ACTIVE_LOW>; };

/* 用户按键（软件复位） */
rest_key: rest-key {
    gpios = <&gpio0 1 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
};

/* 中断输入 */
ntc_int: ntc-int { gpios = <&gpio0 0 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>; };
acc_int: acc-int { gpios = <&gpio0 2 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>; };
tof_int: tof-int { gpios = <&gpio1 14 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>; };

/* WS2812B LED */
ws2812b: ws2812b { gpios = <&gpio1 8 0>; };
```

### 8.4 Chosen 节点清理

```dts
/ {
    chosen {
        /delete-property/ zephyr,console;       /* 无 UART 控制台 */
        /delete-property/ zephyr,shell-uart;     /* 无 Shell */
        /delete-property/ zephyr,uart-mcumgr;    /* 无 mcumgr */
        /delete-property/ zephyr,bt-mon-uart;    /* 无 BT monitor */
        /delete-property/ zephyr,bt-c2h-uart;    /* 无 BT C2H */
    };
};
```

---

## 9. PCB-B 结构模板

### 9.1 前置条件

PCB-B 需要以下输入资料（截至本文档写作时均未提供）：

- [ ] PCB-B 原理图（含引脚分配）
- [ ] PCB-B IO 配置功能文件（类似 PCB-A 的 DOCX）
- [ ] 确认与 PCB-A 的差异（传感器组合/通信模组/电源架构）

### 9.2 模板框架

PCB-B 的 overlay 结构应与 PCB-A 保持一致的分层顺序（见 §2.2），差异部分集中在：

1. **pinctrl 配置** — 引脚分配不同时重新生成
2. **外设节点使能/禁用** — 根据实际器件调整
3. **GPIO 设备树** — 电源控制脚/中断脚重新映射
4. **串行 IP 块分配** — 如传感器组合不同需重新规划

### 9.3 可能的差异维度

| 维度 | PCB-A | PCB-B（猜测） | 影响 |
|------|-------|-------------|------|
| 传感器 | TOF + 超声波 + BMA510 | 可能增减 | I2C/UART 数量变化 |
| LPWA 模组 | 有 | 可能不同型号或无 | UART21 配置 |
| GNSS | 软件 UART（预留） | 可能硬件 UART 或无 | 串行 IP 需求变化 |
| 加密芯片 | ATECC608C | 可能有/无 | TWIM22 分配 |
| 电源域 | MK_EN 单域 | 可能多域 | GPIO 控制脚增加 |

---

## 10. Kconfig 结构

### 10.1 `.conf` 最小集（Phase 0 — NFC/BLE 调试）

```ini
# 已在 writable_ndef_msg/prj.conf 验证的配置
CONFIG_NFC_T4T_NRFXLIB=y
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_NUS=y
CONFIG_USE_SEGGER_RTT=y
CONFIG_LOG_BACKEND_RTT=y
CONFIG_DK_LIBRARY=n
# CONFIG_UART_CONSOLE=n
# CONFIG_SHELL=n
```

### 10.2 `.conf` 完整集（Phase 1 — 全外设启用）

需新增：

```ini
# 电源管理
CONFIG_PM_DEVICE=y
CONFIG_PM_DEVICE_RUNTIME=y
CONFIG_PM=y
CONFIG_POWEROFF=y

# 总线
CONFIG_I2C=y
CONFIG_UART_ASYNC_API=y  # LPWA UART DMA

# 传感器
CONFIG_ADC=y

# 文件系统 + Flash
CONFIG_FILE_SYSTEM=y
CONFIG_FILE_SYSTEM_LITTLEFS=y
CONFIG_SPI_NOR=y          # W25Q32JV
```

---

## 11. 从 DK 迁移到 PCB-A Checklist

| # | 检查项 | DK 默认 | PCB-A | 操作 |
|---|--------|---------|-------|------|
| 1 | DK LED 引脚 | P2.09/P1.10/P2.07/P1.14 | 业务引脚 | 删除 |
| 2 | DK 按键引脚 | P1.13/P1.09/P1.08/P0.04 | 业务引脚 | 删除 |
| 3 | UART20 控制台 | P1.04-P1.07 | NTC_ADC/VBAT_ADC/US_RX/US_TX | 禁用 |
| 4 | PWM20 | P1.10 | CC_I2C_SDA | 禁用 |
| 5 | SPI22 (expansion) | P1.06/07/11 | US_RX/US_TX/CC_I2C_SCL | 禁用 |
| 6 | GRTC CLKOUT | P1.08/P0.04 | LED_PWM/ACC_I2C_SCL | 清除 pinctrl |
| 7 | Flash 型号 | mx25r64 (64Mb) | W25Q32JVSSIQ (32Mb) | 修改 SPI NOR 节点 |
| 8 | Chosen console | &uart20 | (删除) | 删除 |
| 9 | Chosen shell-uart | &uart20 | (删除) | 删除 |
| 10 | NFC | 已使能 | 已使能 | 无变更 |
| 11 | BLE RADIO | 已使能 | 已使能 | 无变更 |
| 12 | GPIO0/1/2 | 已使能 | 已使能 | 无变更 |
| 13 | GPIOTE20/30 | 已使能 | 已使能 | 无变更 |

---

## 12. 附录

### 12.1 引脚快速索引

| 引脚 | PCB-A 功能 | 中断 | GPIOTE | 外设实例 |
|------|-----------|:----:|:------:|---------|
| P0.00 | NTC_INT | ✅ | gpiote30 ch2 | GPIO |
| P0.01 | REST_KEY | ✅ | gpiote30 ch0 | GPIO |
| P0.02 | ACC_INT | ✅ | gpiote30 ch1 | GPIO |
| P0.03 | ACC_I2C_SDA | — | — | TWIM30 |
| P0.04 | ACC_I2C_SCL | — | — | TWIM30 |
| P1.04 | NTC_ADC | — | — | SAADC AIN0 |
| P1.05 | VBAT_ADC | — | — | SAADC AIN1 |
| P1.06 | US_RX | — | — | GPIO 软件 UART |
| P1.07 | US_TX | — | — | GPIO 软件 UART |
| P1.08 | LED_PWM | — | — | GPIO |
| P1.09 | MK_EN | — | — | GPIO |
| P1.10 | CC_I2C_SDA | — | — | TWIM22 |
| P1.11 | CC_I2C_SCL | — | — | TWIM22 |
| P1.12 | TOF_I2C_SCL | — | — | TWIM20 |
| P1.13 | TOF_I2C_SDA | — | — | TWIM20 |
| P1.14 | TOF_INT | ✅ | gpiote20 ch0 | GPIO |
| P2.00 | LPWA_POWER | — | — | GPIO |
| P2.01 | SPI1_CLK | — | — | SPIM00 |
| P2.02 | SPI1_MOSI | — | — | SPIM00 |
| P2.03 | LPWA_EN | — | — | GPIO |
| P2.04 | SPI1_MISO | — | — | SPIM00 |
| P2.05 | SPI1_CS | — | — | SPIM00 |
| P2.06 | LPWA_PSM | — | — | GPIO |
| P2.07 | MAIN_LPWA_RXD | ⚠️ UART RX int | — | UART21 |
| P2.08 | MAIN_LPWA_TXD | — | — | UART21 |
| P2.09 | MAIN_GNSS_TXD | ❌ | ❌ | GPIO 软件 UART |
| P2.10 | MAIN_GNSS_RXD | ❌ | ❌ | GPIO 软件 UART |

> ❌ = P2 无 GPIOTE，无法配置中断；⚠️ = 通过 UART 模块内部中断实现，不依赖 GPIOTE

### 12.2 参考文档

- nRF54L15 Datasheet v1.0
- NCS v3.3.0 Zephyr Device Tree 文档
- `pcb_a_io_map.md` — PCB-A 完整引脚分配表
- `pcb_a_pin_audit.md` — 当前工程 vs PCB-A 核对审计
- `Zephyr设备树与驱动配置指南.md` — Zephyr 驱动配置知识库
