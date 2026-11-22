# STM32 工业协议网关

> 一套完整的小系统：**PC 上位机 → F407 网关 → F103 下位机**。  
> 上位机和网关之间走 Modbus TCP，网关和下位机之间走 CAN。

一句话说明：

> PC 通过网线读写 F407 网关里的寄存器；网关把风扇、LED 命令转成 CAN 发给 F103；F103 执行后回 ACK，并周期上报温度、光照、风扇和 LED 状态。

---

## 1. 这个项目是什么

项目分为三个可独立运行的部分：

- `host-computer/`：PC 上位机，Python + PySide6，显示状态、控制风扇和 LED；
- `gateway-f407/`：F407 网关，运行 FreeRTOS + lwIP，提供 Modbus TCP 服务，同时收发 CAN；
- `node-f103/`：F103 下位机，裸机运行，接收 CAN 命令，控制风扇/LED，采集传感器。

另外有一个命令行测试工具：

- `Test/`：不依赖界面的 Modbus TCP 测试脚本，方便先验证通信。

### 已经能做什么

- PC 读取 7 个寄存器：温度、光照、风扇转速、LED 状态、命令结果、在线状态、协议版本；
- PC 写风扇转速 `0~3000` 和 LED 状态 `0/1`；
- F407 把写命令转成 CAN `0x301`；
- F103 执行命令，回 CAN ACK `0x401`；
- F103 默认每 3 秒上报状态 `0x101`，每 500 ms 发心跳 `0x201`；
- F407 缓存下位机状态，PC 读寄存器时直接读缓存。

### 暂时不做什么

- 不是量产项目，没有登录、权限、加密、OTA；
- 没有掉电保存，重启后寄存器恢复默认值；
- 温度/光照目前是 **ADC 原始值**，还没有做传感器标定；
- 风扇是开环控制，没有测速反馈；
- 上位机已经可以打包，但打包结果只适用于当前平台，Windows 需要重新打包。

> 这是一个个人项目，先把整条链路跑通，再考虑看门狗、掉电保存、异常恢复这些。

---

## 2. 系统结构

```text
PC 上位机 ──Modbus TCP──> F407 网关 ──CAN 500kbps──> F103 下位机
                                                          │
                                                          ├─ PWM  → 风扇
                                                          ├─ GPIO → LED
                                                          └─ ADC  → 热敏/光敏

F103 ──0x101 状态 / 0x201 心跳 / 0x401 ACK──> F407
```

| 部分 | 作用 |
|---|---|
| `host-computer` | PC 上位机，读状态、写风扇/LED |
| `gateway-f407` | Modbus TCP 服务器 + CAN 主站，缓存设备状态 |
| `node-f103` | CAN 从机，执行命令、采集传感器、回 ACK |
| `Test` | 命令行 Modbus TCP 测试工具 |

---

## 3. 目录结构

```text
stm32-industrial-protocol-gateway-v2/
├── gateway-f407/   F407 网关
├── node-f103/      F103 下位机
├── host-computer/  PC 上位机
├── Test/           Modbus TCP 测试工具
└── README.md       本文件
```

各部分的详细说明：

- [F407 网关说明](gateway-f407/README.md)
- [F103 下位机说明](node-f103/README.md)
- [Modbus 测试工具说明](Test/README.md)

`build/`、`host-computer/.venv/`、`__pycache__/` 是本地产物，不会提交到仓库。

---

## 4. 快速开始

下面命令默认从仓库根目录开始执行。

### 4.1 硬件和接线

- STM32F407 网关板、STM32F103 下位机板；
- 两个 CAN 收发器；
- 网线，PC 和 F407 在同一个网段；
- DAPLink 或 ST-LINK 下载器；
- 热敏电阻、光敏电阻、风扇、LED（可选）。

CAN 接线注意：

- CANH 接 CANH，CANL 接 CANL；
- **两块板的 CAN 收发器必须共地**；
- 总线两端按需要接 `120Ω` 终端电阻；
- 两边波特率都是 **500 kbps**。

### 4.2 烧录固件

F103 下位机：

```bash
cd node-f103
python3 .scripts/build.py
python3 .scripts/flash.py
```

F407 网关：

```bash
cd gateway-f407
python3 .scripts/build.py
python3 .scripts/flash.py
```

默认使用 DAPLink。ST-LINK 加 `--probe stlink`：

```bash
python3 .scripts/flash.py --probe stlink
```

如果提示找不到 `arm-none-eabi-gcc`，先把工具链的 `bin` 目录加到 `PATH`。

### 4.3 配置网络

F407 默认地址：

```text
IP:      192.168.0.200
端口:    502
子网掩码: 255.255.255.0
```

PC 网卡配成同一网段，例如：

```text
PC: 192.168.0.100/24
```

先确认能 ping 通：

```bash
ping 192.168.0.200
```

网关没有做 DHCP，也没有做网线热插拔检查，建议**先插好网线再上电**。

### 4.4 运行上位机

第一次运行需要装依赖：

```bash
cd host-computer
python3 -m venv .venv
.venv/bin/pip install PySide6==6.12.0 pymodbus==3.16.2
.venv/bin/python main.py
```

如果 `.venv` 已经存在，直接：

```bash
.venv/bin/python main.py
```

默认连接 `192.168.0.200:502`。网关 IP 变了的话，改 [modbustcp.py](host-computer/modbustcp.py) 里的默认参数。

界面说明和打包流程见 [host-computer/README.md](host-computer/README.md)。

### 4.5 用命令行测试

在仓库根目录执行：

```bash
python3 Test/modbus_test.py
```

常用命令：

```text
read                读 7 个寄存器
fan 1500            写风扇 1500
led 1               写 LED 开
led 0               写 LED 关
watch 10 0.5        监视 10 秒，只打印变化
exit                退出
```

更完整的命令和原始报文见 [Test/README.md](Test/README.md)。

---

## 5. 数据怎么走

### 读寄存器

```text
PC 读 0x0000~0x0006
        │
        ▼
F407 直接读自己的设备状态缓存
        │
        ▼
返回 Modbus 响应
```

读寄存器**不会发 CAN 请求**。缓存由 F103 主动发来的 `0x101` 状态帧更新。

### 写风扇/LED

```text
PC 写 0x0002 或 0x0003
        │
        ▼
F407 生成 CAN 0x301
        │
        ▼
F103 执行，回 CAN 0x401 ACK
        │
        ▼
F407 更新 cmd_result
```

写寄存器后，Modbus 的回显只表示“网关收到了”，**不代表下位机已经执行**。真正结果看 `0x0004 cmd_result`：

| 值 | 含义 |
|---:|---|
| 0 | 成功：F103 回了成功 ACK |
| 1 | 在途：命令已发出，正在等 ACK |
| 2 | 失败：ACK 失败或 CAN 发送失败 |
| 3 | 超时：1 秒内没收到匹配的 ACK |

### CAN 报文

| ID | 方向 | 作用 | 周期 |
|---|---|---|---|
| `0x101` | F103 → F407 | 温度/光照/风扇/LED 状态 | 默认 3 秒 |
| `0x201` | F103 → F407 | 心跳 | 500 ms |
| `0x301` | F407 → F103 | 风扇/LED 命令 | 命令触发 |
| `0x401` | F103 → F407 | 命令 ACK | 收到命令后立即 |

F407 连续 1.5 秒没收到 `0x201`，会把 `online` 置为 1（离线）。

> 注意：`gateway-f407/docs/protocol.md` 里 `0x101` 写的是 500 ms，那是早期约定；当前 F103 默认 **3 秒**。需要修改时见 [node-f103/README.md](node-f103/README.md)。

更细的 CAN 字节格式见 [protocol.md](gateway-f407/docs/protocol.md)。

---

## 6. 各部分说明

| 目录 | 是什么 | 详细说明 |
|---|---|---|
| `gateway-f407/` | F407 网关，FreeRTOS + lwIP，Modbus TCP 服务器 + CAN 主站 | [README](gateway-f407/README.md) |
| `node-f103/` | F103 下位机，裸机 CAN 从机，控制风扇/LED、采集 ADC | [README](node-f103/README.md) |
| `host-computer/` | PC 上位机，PySide6 界面 | [README](host-computer/README.md) |
| `Test/` | 命令行 Modbus TCP 测试工具 | [README](Test/README.md) |

---

## 7. 文档

- [F407 网关说明](gateway-f407/README.md)
- [F103 下位机说明](node-f103/README.md)
- [CAN 协议和 Modbus 寄存器](gateway-f407/docs/protocol.md)
- [PC 上位机说明](host-computer/README.md)
- [Modbus TCP 测试工具](Test/README.md)

---

## 8. 当前状态

已经完成：

- F407 网关的 Modbus TCP 服务、设备缓存、CAN 收发；
- F103 的状态帧、心跳、ACK、风扇 PWM 和 LED 控制；
- PC 上位机源码；
- 命令行 Modbus 测试工具；
- F103 和 F407 的编译、烧录脚本。

还需要继续做：

- 上位机跨平台打包（当前 Linux 包不能在 Windows 上直接运行）；
- 温度/光照传感器标定；
- 风扇测速和闭环控制；
- 看门狗、掉电保存、异常恢复。

---

## 9. 常见问题

### ping 不通网关

- PC 网卡是不是 `192.168.0.x`；
- 网线是不是插在 F407 网口；
- 网关是不是上电前就插好网线；
- 网关板子有没有正常供电。

### `cmd_result` 一直是 1 或 3

说明 F407 发了命令，但没收到 F103 的 ACK：

- F103 有没有上电、程序有没有跑起来；
- CAN 收发器有没有共地；
- 两边波特率是不是都是 500 kbps；
- CANH/CANL 有没有接反。

### `online` 一直是 1（离线）

说明 F407 没收到 F103 的心跳：

- F103 有没有在发 `0x201`；
- CAN 线有没有接好；
- F103 是不是停在 `Error_Handler()`。

### 上位机连接失败

- 先确认 `ping 192.168.0.200` 能通；
- 确认 F407 已经启动；
- 确认 [modbustcp.py](host-computer/modbustcp.py) 里的 IP 和端口；
- 先启动网关，再打开上位机。

更多问题见各子项目的 README。
