# STM32F407 工业协议网关

> 这个目录只放 **F407 网关本体**。  
> F103 下位机不在本次封包范围内，后面单独整理。

一句话说明：

> PC 通过网线用 Modbus TCP 读写网关里的寄存器；网关把风扇、LED 命令转成 CAN 发给下位机，同时把下位机回传的状态缓存起来，供 PC 读取。

---

## 1. 这个网关现在能做什么

已经实现的部分：

- PC 通过网线连接网关，使用 **Modbus TCP** 读写寄存器；
- 支持功能码 `0x03`（读保持寄存器）和 `0x06`（写单个寄存器）；
- 网关缓存 7 个寄存器：温度、光照、风扇转速、LED 状态、命令结果、在线状态、协议版本；
- PC 写风扇或 LED 后，网关会转成 CAN 命令 `0x301` 发出去；
- 网关能接收下位机的 CAN 状态帧 `0x101`、心跳 `0x201`、命令确认 `0x401`；
- 多条 TCP 连接、断开后重连都能正常工作；
- 读寄存器不会经过 CAN，直接读网关里的缓存，所以响应很快。

已经实测过的：

- 固件能正常启动，FreeRTOS 任务能跑起来；
- 目标板 IP 为 `192.168.0.200`，`502` 端口可连接；
- 两个客户端同时读寄存器正常；
- 断开重连后再读正常；
- 读 7 个寄存器返回值正确。

### 它现在不做什么

- 不支持 Modbus RTU、Modbus ASCII；
- 不支持 `0x01/0x02/0x04/0x05/0x0F/0x10` 等功能码；
- 不做用户登录、权限、加密；
- 不做掉电保存，重启后寄存器恢复默认值；
- 不做 DHCP，IP 是写死的；
- 网线热插拔没有专门处理，最好上电前先把网线插好。

> 这是一个个人项目，不是量产网关。先把它用起来，再考虑工业现场需要的看门狗、掉电保存、权限、异常恢复这些东西。

---

## 2. 接线和准备工作

### 需要的东西

- STM32F407 网关板；
- 网线，PC 和网关在同一个网段；
- CAN 收发器，以及 F103 下位机；
- DAPLink 或 ST-LINK 下载器；
- PC 上装好 `cmake`、`ninja`、`arm-none-eabi-gcc`、`openocd`、`python3`。

### 网络

网关默认地址：

```text
IP:      192.168.0.200
子网掩码: 255.255.255.0
网关:    192.168.0.1
Modbus TCP 端口: 502
```

PC 的网卡需要配成同一网段，例如：

```text
PC: 192.168.0.100/24
```

然后把网线插到网关的网口，先确认能 ping 通：

```bash
ping 192.168.0.200
```

如果 PC 的网段不是 `192.168.0.x`，要么改 PC 网卡，要么改网关 IP。

改网关 IP 的位置：

```text
gateway-f407/LWIP/App/lwip.c
```

找到这四行改就行：

```c
IP_ADDRESS[0] = 192;
IP_ADDRESS[1] = 168;
IP_ADDRESS[2] = 0;
IP_ADDRESS[3] = 200;
```

Modbus 端口在：

```text
gateway-f407/app/app_tasks.c
```

```c
Net_Port_Init(502);
```

### CAN

- 两边 CAN 波特率都是 **500 kbps**；
- F407 的 CAN 引脚是 `PD0/PD1`；
- F103 的 CAN 引脚是 `PA11/PA12`；
- **两块板的 CAN 收发器必须共地**。只接 CANH/CANL、不共地，很可能完全收不到；
- CAN 总线两端按需要接 `120Ω` 终端电阻。

---

## 3. 编译和烧录

### 3.1 编译

```bash
cd gateway-f407
python3 .scripts/build.py
```

默认使用 `Debug`，生成：

```text
build/Debug/gateway-f407.elf
```

也可以指定 Release：

```bash
python3 .scripts/build.py -p Release
```

如果提示找不到 `arm-none-eabi-gcc`，先把工具链的 `bin` 目录加到 `PATH`。

### 3.2 烧录

默认使用 DAPLink：

```bash
python3 .scripts/flash.py
```

脚本会自动在 `build/Debug` 下找固件，然后烧录、校验、复位。

如果用的是 ST-LINK：

```bash
python3 .scripts/flash.py --probe stlink
```

烧录失败时先检查：

- 板子有没有供电；
- DAPLink 的 `SWDIO` 是否接 `PA13`；
- `SWCLK` 是否接 `PA14`；
- `GND` 是否共地。

---

## 4. 怎么用 Modbus 主站读写

可以用 **Modbus Poll**、**QModMaster**，或者自己写 Python/组态软件。

### 读寄存器

配置：

```text
模式: Modbus TCP
IP:   192.168.0.200
端口: 502
Unit ID: 1
功能码: 03 Read Holding Registers
起始地址: 0
数量: 7
```

正常启动、下位机还没发心跳时，读出来的值大概是：

```text
0x0000 = 0      温度
0x0001 = 0      光照
0x0002 = 0      风扇转速
0x0003 = 0      LED 状态
0x0004 = 0      命令结果
0x0005 = 1      离线
0x0006 = 1      协议版本
```

`Unit ID` 当前没有做严格校验，原样回显；建议统一填 `1`。

### 写风扇

```text
功能码: 06
地址:   0x0002
值:     0 ~ 3000
```

例如写 `1500`，表示目标转速 1500 转。

### 写 LED

```text
功能码: 06
地址:   0x0003
值:     0 或 1
```

`0` 关，`1` 开。

### 写命令的结果看哪里

写寄存器后，网关会立刻返回 Modbus 回显。  
**回显只代表“网关收到了”，不代表下位机已经执行。**

真正执行结果看 `0x0004 cmd_result`：

| 值 | 含义 |
|---:|---|
| 0 | 成功：下位机回了 ACK，且结果为成功 |
| 1 | 在途：CAN 命令已发出，正在等 ACK |
| 2 | 失败：下位机回了失败 ACK，或者 CAN 发送失败 |
| 3 | 超时：1 秒内没收到匹配的 ACK |

所以典型流程是：

```text
PC 写 0x0003 = 1
  -> 网关回写回显
  -> cmd_result = 1
  -> 等 0x401 ACK
  -> 成功则 cmd_result = 0
  -> 失败/超时则 cmd_result = 2/3
```

如果对端没有回 `0x401`，最终就会是 `3`。  
这是“对方没有确认”，不是 Modbus TCP 没通。

---

## 5. 寄存器表

每个寄存器就是一个 16 位的数据格。

| 地址 | 读写 | 名称 | 说明 |
|---|---|---|---|
| `0x0000` | R | `temperature` | 温度，按 `×0.1℃` 解释 |
| `0x0001` | R | `light` | 光照值 |
| `0x0002` | R/W | `fan_rpm` | 读：当前风扇转速；写：目标转速 0~3000 |
| `0x0003` | R/W | `led_state` | 读：当前 LED 状态；写：0 关、1 开 |
| `0x0004` | R | `cmd_result` | 0 成功、1 在途、2 失败、3 超时 |
| `0x0005` | R | `online` | 0 在线、1 离线 |
| `0x0006` | R | `protocol_version` | 当前固定为 1 |

限制：

- 一次最多读 **7 个**寄存器；
- 只支持写 `0x0002` 和 `0x0003`；
- 同一时间只允许一条写命令在途，第二条写会返回“设备忙”；
- 读请求不受在途写命令影响，仍然可以读 `cmd_result`。

---

## 6. 数据是怎么走的

### 读寄存器

```text
PC 读 0x0000~0x0006
        │
        ▼
F407 直接读自己的 device_model 缓存
        │
        ▼
返回 Modbus 响应
```

注意：**读寄存器不会发 CAN 读请求**。  
网关缓存由下位机主动发来的 `0x101` 状态帧更新。

### 写风扇/LED

```text
PC 写 0x0002 或 0x0003
        │
        ▼
F407 生成 CAN 0x301
        │
        ▼
下位机执行，并回 CAN 0x401 ACK
        │
        ▼
F407 更新 cmd_result
```

### CAN 报文

| ID | 方向 | 作用 |
|---|---|---|
| `0x101` | 下位机 → 网关 | 温度/光照/风扇/LED 状态 |
| `0x201` | 下位机 → 网关 | 心跳，500ms 一帧 |
| `0x301` | 网关 → 下位机 | 风扇/LED 命令 |
| `0x401` | 下位机 → 网关 | 命令 ACK |

网关判定离线：

- 1.5 秒没有收到 `0x201` 心跳，`online` 置 1（离线）；
- 收到心跳后 `online` 置 0（在线）。

更细的 CAN 字节格式见：

- [docs/protocol.md](docs/protocol.md)

> 这份协议文档已经按当前实现同步，寄存器和 CAN 帧细节以 `docs/protocol.md` 为准。

---

## 7. 代码是怎么分层的

```text
gateway-f407/
├── app/        启动和任务创建
├── bsp/        网络、CAN 这些板级驱动
├── gateway/    会话、设备模型、命令状态机、任务调度
├── protocol/   Modbus 和 CAN 的编解码
├── Core/       STM32CubeMX 生成的芯片初始化
├── LWIP/       lwIP 协议栈和网卡驱动
├── Third_Party/FreeRTOS
├── docs/       协议、代码结构、开发日志
└── .scripts/   编译和烧录脚本
```

每一层只干自己的事：

### `app/`

负责“把系统启动起来”：

- 初始化 lwIP；
- 初始化 CAN；
- 初始化网络端口；
- 初始化 gateway；
- 创建各个 FreeRTOS 任务。

### `bsp/`

负责“和硬件打交道”：

- `net_port.c`：TCP socket、连接事件、发送队列；
- `can_port.c`：CAN 原始帧收发。

这一层不解析 Modbus，也不管命令状态。

### `gateway/`

负责“网关业务”：

- `gateway_modbus.c`：接收 TCP 数据，按连接拼 Modbus 帧，转成请求；
- `gateway_can.c`：接收 CAN 帧，解码成状态/心跳/ACK；
- `gateway.c`：把 Modbus 请求和 CAN 事件分发给对应处理；
- `device_model.c`：保存 7 个寄存器的缓存；
- `command_manager.c`：管理写命令、序号、ACK 和超时。

### `protocol/`

负责“把字节翻译成含义”：

- `modbus_tcp.c`：MBAP 头、拆帧、组帧；
- `modbus_pdu.c`：功能码、地址、数量、值、异常响应；
- `can_app.c`：0x101/0x201/0x301/0x401 的编解码。

这一层不碰 socket，也不碰 GPIO。

### 任务关系

```text
Gateway_ModbusRxTask  ─┐
                       ├─> GatewayTask ─> CommandTask ─> CAN
Gateway_CanRxTask     ─┘
                              │
                              ▼
                         device_model
                              │
                              ▼
                         Modbus 读响应
```

简单理解：

- `Gateway_ModbusRxTask`：收网络请求；
- `Gateway_CanRxTask`：收 CAN 帧；
- `GatewayTask`：决定这件事该谁处理；
- `CommandTask`：真正发 CAN、等 ACK、回 Modbus；
- `device_model`：所有寄存器值的“小仓库”。

---

## 8. 常见问题

### ping 不通

先查：

1. PC 网卡是不是 `192.168.0.x`；
2. 网线是不是插在网关网口；
3. 网关是不是上电前就插好网线；
4. 目标板 link 灯是否亮。

当前没有做周期 link 检查，**先上电后插网线可能起不来**。测试时建议先插网线再上电。

### TCP 能连上，但读不到数据

- 确认 Unit ID 填的是 `1`；
- 确认功能码是 `03`；
- 确认起始地址 `0`、数量 `7`；
- 确认没有把地址填成 `1` 起始。

### 写命令后 `cmd_result` 一直是 3

说明网关发了 CAN，但没收到匹配的 ACK。检查：

- 下位机有没有收到 `0x301`；
- 下位机有没有回 `0x401`；
- ACK 里的命令码、序号和原命令是否一致；
- CAN 收发器是否共地；
- 波特率是否都是 500 kbps。

### CAN 完全没反应

重点查三件事：

1. CANH/CANL 有没有接反；
2. 两块板有没有共地；
3. 总线终端电阻和波特率。

### 烧录失败

```bash
python3 .scripts/flash.py --probe stlink
```

或者检查 DAPLink 的 SWD 接线和板子供电。

---

## 9. 文档

- [协议和寄存器说明](docs/protocol.md)
- [代码结构与文件职责](docs/code_structure.md)

---

## 10. 当前状态

已经实测通过：

- F407 网关启动；
- TCP 502 监听；
- Modbus TCP 多客户端读；
- 断开重连；
- 7 个寄存器读回。

已经实现、但还需要和对端联调：

- CAN 命令 `0x301` 的发送；
- 状态帧 `0x101`、心跳 `0x201`、ACK `0x401` 的接收和状态更新；
- 写命令的 ACK / 超时状态机。

还需要继续做的：

- F103 业务侧的状态帧、心跳、ACK 自动回复；
- 网线热插拔的 link 周期检查；
- 更完整的异常码和错误恢复；
- 掉电保存、看门狗、权限等量产功能。

如果只是验证网关本体，现在读寄存器已经够用了。  
如果要跑完整的“写命令 -> 下位机执行 -> ACK”闭环，还需要 F103 侧配合。
