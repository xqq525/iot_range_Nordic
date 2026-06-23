# App ↔ 设备 联调测试表

**设备型号**: ULP_RS100  
**固件版本**: 1.0.0  
**协议版本**: V1.7  
**测试日期**: ________  

---

## 1. NFC 读设备信息

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 靠近设备 NFC 天线后能正确读取设备完整信息（63 字节应答） |
| **App 操作步骤** | 1. APP 进入设备详情页 2. 手机 NFC 天线贴近设备 3. APP 自动发送 NDEF 消息 `TNF=0x04, Type=ulpinternal:cmd, Payload=[01, TS...]`（附带当前 Unix 时间戳）4. APP 重新读取 NDEF 消息，解析 `Type=ulpinternal:info` 记录 |
| **预期设备响应** | NDEF 应答 Payload 63 字节：CMD=0x01, STATUS=0x00, MODEL=ULP_RS100, SN=123456, FW=1.0.0, HW=1.0.0, BATTERY/ TEMPERATURE/ DISTANCE/ MODE/ INTERVAL/ GNSS 字段均有值 |
| **通过标准** | APP 界面显示：设备型号 ULP_RS100、序列号、固件版本、电量、温度、距离、模式、上报间隔等全部字段，且数值在合理范围；STATUS=0x00，CMD 回显 0x01 |
| **失败时优先检查** | [app_config.c:423](../src/app_config.c#L423) `app_config_handle_ndef()` — NDEF 解析入口；[app_config.c:223](../src/app_config.c#L223) `ndef_parse_ext_cmd()` — TNF/Type/Payload 解析逻辑；[app_config.c:304](../src/app_config.c#L304) `ndef_encode_info_response()` — 应答编码；[通信对接文档.md:102-122](../通信对接文档.md#L102-L122) — 应答 Payload 字段定义 |
| **NFC 天线接触不良** | `app_config.c:433` —检查 NDEF 消息是否可读 |
| **APP 字段解析错误** | `通信对接文档.md:102-122` 核对大端序/偏移量 |
| **STATUS 非 0x00** | `app_config.c:215` —检查 CMD 是否被错误路由到 default 分支 |

---

## 2. NFC 写配置：mode

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 通过 NFC 写入设备模式（快递箱/垃圾桶/货架）后，设备正确保存并返回更新值 |
| **App 操作步骤** | 1. APP 设置页面选择目标模式（如 垃圾桶 0x02 → 货架 0x03）2. 手机 NFC 贴近设备 3. APP 写入 NDEF 消息 `Payload=[02, 01, 03]`（CMD=0x02, param_id=0x01, mode=0x03）4. APP 重新读取设备信息验证 mode 字段 |
| **预期设备响应** | 设备返回 63 字节应答：CMD=0x02, STATUS=0x00, MODE(offset 49)=0x03 |
| **通过标准** | APP 收到 STATUS=0x00；再次读取设备信息时 mode 显示为"货架" |
| **失败时优先检查** | [app_config.c:362](../src/app_config.c#L362) `PARAM_MODE` case — mode 写入逻辑；[app_config.c:187](../src/app_config.c#L187) — MODE 应答字段构建 |

---

## 3. NFC 写配置：interval

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 通过 NFC 写入上报间隔（分钟） |
| **App 操作步骤** | 1. APP 设置页面输入上报间隔（如 30 分钟）2. 手机 NFC 贴近设备 3. APP 写入 `Payload=[02, 02, 00, 1E]`（CMD=0x02, param_id=0x02, interval=0x001E=30min）4. APP 重新读取验证 UPDATE_TIME 字段 |
| **预期设备响应** | STATUS=0x00；再次读取时 UPDATE_TIME=30（0x001E） |
| **通过标准** | APP 收到 STATUS=0x00；UPDATE_TIME 为 30 min |
| **失败时优先检查** | [app_config.c:369](../src/app_config.c#L369) `PARAM_REPORT_INTERVAL` case — interval 写入逻辑（需 args_len≥3）；[app_config.c:191](../src/app_config.c#L191) — UPDATE_TIME 字段构建 |

---

## 4. NFC 写配置：position

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 通过 NFC 写入安装位置坐标（LAT/LNG） |
| **App 操作步骤** | 1. APP 地图页面选择位置（如 24.60878°N, 118.06893°E）2. 手机 NFC 贴近设备 3. APP 写入 `Payload=[02, 03, 0E, AA, 5C, F8, 46, 60, E1, 94]`（param_id=0x03, LAT=246087800, LNG=1180689300）4. APP 重新读取验证 GNSS_FIX / LATITUDE / LONGITUDE 字段 |
| **预期设备响应** | STATUS=0x00；GNSS_FIX=0x01, LATITUDE/LONGITUDE 为写入值 |
| **通过标准** | APP 收到 STATUS=0x00；地图显示写入的位置正确 |
| **失败时优先检查** | [app_config.c:381](../src/app_config.c#L381) `PARAM_INSTALL_POS` case — 坐标写入逻辑（需 args_len≥9）；[app_config.c:195](../src/app_config.c#L195) — GNSS/LAT/LNG 应答字段构建 |

---

## 5. BLE 扫描

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证设备上电后持续 BLE 广播，APP 能在扫描列表中看到设备 |
| **App 操作步骤** | 1. 确保设备已上电（LED 指示或恢复后自动广播）2. APP 进入 BLE 扫描页面 3. 等待设备出现在扫描列表中 |
| **预期设备响应** | 广播名称为 `ULP_RS100_123456`（MODEL_SN），广播数据含 NUS UUID |
| **通过标准** | APP 扫描列表中可见 `ULP_RS100_123456` 设备；信号强度 RSSI 正常（通常 > -80 dBm） |
| **失败时优先检查** | [ble_uart.c:132](../src/ble_uart.c#L132) `advertising_start()` — 广播启动入口；[ble_uart.c:114](../src/ble_uart.c#L114) `adv_work_handler()` — 广播数据处理；[ble_uart.c:376](../src/ble_uart.c#L376) `ble_name` 格式 — 确认 MODEL+SN 拼接无误；[prj.conf](../prj.conf) — 确认 CONFIG_BT_PERIPHERAL=y |
| **设备未上电/程序未运行** | 检查供电和 `west flash --recover` 是否已解除读保护 |
| **广播名不对** | `ble_uart.c:376` —检查 MODEL/Sn 拼接格式 |

---

## 6. BLE 连接

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 能成功连接设备 BLE，NUS 服务就绪 |
| **App 操作步骤** | 1. APP 扫描到 `ULP_RS100_123456` 2. 点击连接 3. 等待连接建立并发现 NUS 服务 4. APP 使能 TX Char 的 CCCD（Notification） |
| **预期设备响应** | 设备 `connected()` 回调触发；`nus_send_enabled_cb` 回调触发（订阅就绪后）；设备 LED 指示连接状态（如有） |
| **通过标准** | APP 显示已连接；NUS TX/RX Characteristic 发现成功；CCCD 订阅成功后设备 TX Notify 可用 |
| **失败时优先检查** | [ble_uart.c:137](../src/ble_uart.c#L137) `connected()` — 连接回调；[ble_uart.c:103](../src/ble_uart.c#L103) `nus_send_enabled_cb()` — NUS 订阅回调；[ble_uart.c:201](../src/ble_uart.c#L201) `conn_callbacks` — 回调注册表；[prj.conf](../prj.conf) — CONFIG_BT_MAX_CONN=1, CONFIG_BT_NUS=y |
| **连接超时** | `ble_uart.c:46` —KEY_PASSKEY_ACCEPT=0（PCA无按键），需确认安全配置 |
| **GATT 服务发现失败** | `ble_uart.c:385` `bt_nus_init` —检查 NUS 初始化 |

---

## 7. BLE 读设备信息

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 通过 BLE Write RX Char 发送 CMD 0x01 后，设备通过 Notify TX Char 返回完整 63 字节应答 |
| **App 操作步骤** | 1. BLE 已连接且 NUS 已订阅 2. APP 发送 Write RX `[01, TS_3, TS_2, TS_1, TS_0]`（附带 Unix 时间戳）3. 等待 Notify 回调 |
| **预期设备响应** | Notify TX Char 返回 63 字节：CMD=0x01, STATUS=0x00, 完整设备信息 |
| **通过标准** | APP 解析 63 字节应答，所有字段与 NFC 读取结果一致；STATUS=0x00 |
| **失败时优先检查** | [app_config.c:497](../src/app_config.c#L497) `app_config_handle_ble()` — BLE 命令入口；[ble_uart.c:311](../src/ble_uart.c#L311) `bt_receive_cb()` — NUS RX 回调（先判断 len≥1，再访问 data[0]）；[app_config.c:555](../src/app_config.c#L555) CMD=0x01 BLE 分支 — 时间戳解析 |
| **无 Notify 返回** | `ble_uart.c:73` `send_pending_resp()` —检查 retry 逻辑和 MTU 大小 |
| **Notify 数据截断** | `prj.conf:47` MTU=498 —检查 ATT MTU 协商 |

---

## 8. BLE 写配置：mode

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 通过 BLE 写入设备模式 |
| **App 操作步骤** | 1. BLE 已连接 2. APP 发送 Write RX `[02, 01, 01]`（设为快递箱）3. 等待 Notify 应答 4. 再发送 CMD 0x01 验证 |
| **预期设备响应** | Notify 返回 STATUS=0x00, MODE=0x01 |
| **通过标准** | 应答 MODE=0x01；再次读取确认 mode 已切换 |
| **失败时优先检查** | [app_config.c:497](../src/app_config.c#L497) `app_config_handle_ble()` — BLE CMD 0x02 分发；[app_config.c:566](../src/app_config.c#L566) BLE CMD 0x02 分支 entry |

---

## 9. BLE 写配置：interval

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 通过 BLE 写入上报间隔 |
| **App 操作步骤** | 1. APP 发送 Write RX `[02, 02, 00, 3C]`（60 分钟）2. 等待 Notify 3. 发送 CMD 0x01 验证 |
| **预期设备响应** | STATUS=0x00；再次读取 UPDATE_TIME=60min |
| **通过标准** | UPDATE_TIME 为 60 min |
| **失败时优先检查** | [app_config.c:369](../src/app_config.c#L369) `PARAM_REPORT_INTERVAL` — 同 NFC，确认 args_len≥3；大端序编码：interval→`[hi, lo]` |

---

## 10. BLE 写配置：position

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 通过 BLE 写入安装位置坐标 |
| **App 操作步骤** | 1. APP 发送 Write RX `[02, 03, LAT_BE(4), LNG_BE(4)]` 2. 等待 Notify 3. 发送 CMD 0x01 验证 GNSS 字段 |
| **预期设备响应** | STATUS=0x00；GNSS_FIX=0x01，LAT/LNG 为写入值 |
| **通过标准** | 定位数据与写入一致 |
| **失败时优先检查** | [app_config.c:381](../src/app_config.c#L381) `PARAM_INSTALL_POS` — 需 10 字节完整 payload |

---

## 11. BLE OTA 模拟：start / chunk / finish / status

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 BLE OTA 固件升级完整流程（开始→流式数据→结束→校验） |
| **App 操作步骤** | 1. BLE 已连接 2. 选择 .bin 固件（如 12288 字节）3. 发送 `[10, 00, 00, 30, 00]`（CMD=0x10, SIZE=0x3000）4. 等待设备 ACK `[10, 00]` 5. 流式发送所有 CMD 0x11 数据包（每包 492 字节），不等 ACK：第 0 包 `[11, 00, 00, data(492)]`，第 1 包 `[11, 00, 01, data(492)]` … 最后一包可能不足 492 字节 6. 全部发完发送 `[12]`（CMD=0x12）7. 等待设备 ACK `[12, 00]` |
| **预期设备响应** | CMD 0x10 ACK: `[10, 00]`（就绪）；CMD 0x11 不逐包应答（流式）；CMD 0x12 ACK: `[12, 00]`（校验通过） |
| **通过标准** | APP 收到 CMD 0x10 和 0x12 的 STATUS=0x00；OTA 状态查询显示 bytes_received=固件总大小；进度条完整走完 100% |
| **失败时优先检查** | [app_config.c:511](../src/app_config.c#L511) `CMD_OTA_START` — 开始、total_size 解析；[app_config.c:527](../src/app_config.c#L527) `CMD_OTA_DATA` — 数据块写入（offset=idx×492）；[app_config.c:544](../src/app_config.c#L544) `CMD_OTA_END` — 结束校验；[ble_uart.c:73](../src/ble_uart.c#L73) `send_pending_resp()` — 应答发送 retry；[app_config.h:45](../src/app_config.h#L45) OTA_BUF_SIZE(32KB) — 缓冲区上限 |
| **CMD 0x10 无 ACK** | MTU 协商：`prj.conf:47` ATT MTU=498; `ble_uart.c:80` retry 逻辑——检查 RESP_RETRY_MAX |
| **CMD 0x12 STATUS≠0x00** | `app_config.c:527` —检查每包 data_len 和 idx 连续性；total_size 与实际固件是否一致 |
| **缓冲区溢出** | `app_config.h:45` OTA_BUF_SIZE=32KB——固件不能超过 32KB（当前调试阶段） |

---

## 12. 异常场景：设备型号不匹配

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 写入配置前先通过 param_id=0x04 校验设备型号，型号不匹配时设备正确拒绝 |
| **App 操作步骤** | 1. APP 使用 `ULP_RS002` 模板配置设备（实际设备为 ULP_RS100）2. APP 发送 `[02, 04, "ULP_RS002\0"]`（param_id=0x04）3. 等待 Notify / NFC 应答 |
| **预期设备响应** | STATUS=0x05（配置参数与设备型号不匹配） |
| **通过标准** | APP 收到 STATUS=0x05，停止后续写入并提示用户"设备型号不匹配" |
| **失败时优先检查** | [app_config.c:403](../src/app_config.c#L403) `PARAM_MODEL_CHECK` case — 型号比对逻辑（memcmp 长度=app下发字符串长度，最大 16 字节）；[app_config.c:16](../src/app_config.c#L16) `g_model` — 设备端型号定义 "ULP_RS100" |
| **STATUS 始终为 0x00** | 检查 `APP_CONFIG_STATUS_MODEL_MISMATCH=0x05` 是否正确返回 `app_config.c:413` |
| **NFC 下未返回 0x05** | `app_config.c:466` wstatus 是否正确传入 response_build |

---

## 13. 异常场景：BLE 断连

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 BLE 连接意外断开后，设备能自动恢复广播，APP 能重新连接 |
| **App 操作步骤** | 1. BLE 已连接 2. 主动断开（APP 端断开或超出连接范围）3. 等待 3-5 秒 4. APP 重新扫描 |
| **预期设备响应** | 设备 `disconnected()` 回调触发；清理 current_conn、resp_pending、nus_tx_subscribed；自动恢复 BLE 广播（`recycled_cb`→`advertising_start`） |
| **通过标准** | APP 扫描列表中重新出现设备（5 秒内）；重新连接后可正常通信 |
| **失败时优先检查** | [ble_uart.c:154](../src/ble_uart.c#L154) `disconnected()` — 连接清理逻辑；[ble_uart.c:178](../src/ble_uart.c#L178) `recycled_cb()` — 广播恢复触发；[ble_uart.c:201](../src/ble_uart.c#L201) `.recycled` 回调注册 |
| **断连后无法扫描到** | `ble_uart.c:181` advertising_start 是否被调用 |
| **重连后 NUS 无响应** | `ble_uart.c:174` resp_pending/nus_tx_subscribed 是否正确重置 |

---

## 14. 异常场景：NFC 超时

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 NFC 通信中 APP 移开手机后，设备 NDEF 缓冲区状态正确，不影响下一次 NFC 交互 |
| **App 操作步骤** | 1. APP 贴近设备触发 NDEF 写入命令（CMD 0x02）2. 不等设备完成应答，立即移开手机（模拟超时）3. 5 秒后重新贴近设备读取信息 |
| **预期设备响应** | 设备端无超时感知（T4T 是纯被动标签）；NDEF 文件操作在 APP 移开前已原子完成；下一次读取返回当前最新数据 |
| **通过标准** | 第 2 次 NFC 交互正常，返回完整设备信息，STATUS=0x00 |
| **失败时优先检查** | [app_config.c:423](../src/app_config.c#L423) `app_config_handle_ndef()` — NDEF 事件处理整体流程；[app_config.c:473](../src/app_config.c#L473) NDEF 编码写入 ——确认缓冲区内存在 NFC_T4T_EVENT_NDEF_UPDATED 事件中已被完全更新；NFC 硬件驱动层：T4T NDEF 文件 256 字节缓冲区 |
| **T4T 缓冲区脏数据** | `app_config.c:483` nfc_t4t_ndef_file_encode 返回负值 ——检查 NLEN 写入 |
| **APP 读取到旧数据** | T4T 是纯被动标签，APP 移开手机时 NDEF 更新可能未完成。检查 APP 侧是否有读重试 |

---

## 15. 异常场景：写入非法 interval

| 项目 | 内容 |
|------|------|
| **测试目的** | 验证 APP 写入非法 interval 值（如 0、负数）时设备行为 |
| **App 操作步骤** | 1. APP 发送 Write RX `[02, 02, 00, 00]`（interval=0 min）2. 等待 Notify 应答 3. 发送 CMD 0x01 验证实际保存值 |
| **预期设备响应** | 当前固件端无输入校验——interval=0 被直接写入（`g_update_time=0`）；STATUS=0x00 |
| **通过标准** | APP 收到 STATUS=0x00，UPDATE_TIME=0；APP 端应有前端校验：interval 必须 ≥1 min |
| **失败时优先检查** | [app_config.c:369](../src/app_config.c#L369) `PARAM_REPORT_INTERVAL` case — 当前无范围校验，如需后端校验需在此添加；APP 前端校验逻辑 |
| **interval=0 含义** | 0 分钟 = 禁用上报，业务上是否允许需确认 |
| **建议改进** | 固件 `apply_write_config` PARAM_REPORT_INTERVAL 分支增加 `if (val < 1 || val > 1440) return ERROR` |

---

## 附录 A：快速诊断命令（Shell）

连接 J-Link RTT Viewer（buffer 1）可执行以下调试命令：

| 命令 | 用途 |
|------|------|
| `config get` | 读取设备全部配置并打印 |
| `config set mode <0x01-0x03>` | 修改设备模式 |
| `config set interval <min>` | 修改上报间隔 |
| `config set battery <0-100>` | 模拟电量 |
| `config set temperature <val>` | 模拟温度（×0.1°C） |
| `config set distance <cm>` | 模拟距离 |
| `config set latitude <val×1e7>` | 模拟纬度 |
| `config set longitude <val×1e7>` | 模拟经度 |
| `ota info` | 查看 OTA 接收状态 |
| `ota dump <offset> <len>` | 十六进制查看 OTA 缓冲区 |

---

## 附录 B：关键文件索引

| 文件 | 职责 | 涉及的测试项 |
|------|------|-------------|
| [src/app_config.c](../src/app_config.c) | 命令处理核心（NFC/BLE 共用） | 全部 |
| [src/app_config.h](../src/app_config.h) | 命令码/错误码/常量定义 | 全部 |
| [src/ble_uart.c](../src/ble_uart.c) | BLE NUS 服务 + OTA 收发 | 5, 6, 7, 8, 9, 10, 11, 13 |
| [src/shell_cmd.c](../src/shell_cmd.c) | 调试 Shell 命令 | 附录 A |
| [通信对接文档.md](../通信对接文档.md) | 通信协议定义 V1.7 | 全部 |
| [prj.conf](../prj.conf) | Kconfig（BLE/Shell/Log 配置） | 5, 6, 11 |
