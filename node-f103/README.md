# STM32F103 CAN 下位机节点

> 这个目录只放 **F103 下位机本体**。  
> F407 网关在 `../gateway-f407`，PC 测试工具在 `../Test`。

一句话说明：

> F103 通过 CAN 接收 F407 网关发来的风扇/LED 命令，执行后回 ACK，并周期性上报温度、光照、风扇和 LED 状态。

系统里的位置：

```text
PC ──Modbus TCP──> F407 网关 ──CAN 500kbps──> F103 下位机
                                              │
                                              ├─ PWM  -> 风扇
                                              ├─ GPIO -> LED
                                              ├─ ADC  -> 热敏/光敏
                                              └─ CAN  -> 状态/心跳/ACK
```

---

## 1. 这个下位机现在能做什么

已经实现的部分：

- 500 kbps CAN 通信，标准数据帧；
- 接收 F407 发来的 `0x301` 风扇/LED 命令；
- 命令执行后回 `0x401` ACK；
- 每 500 ms 发一次 `0x201` 心跳；
- 默认每 3 秒采集一次 ADC，更新设备模型，并发 `0x101` 状态帧；
- 收到有效命令后会立即补发一帧 `0x101`，不用等下一个 3 秒周期；
- 风扇目标转速 `0~3000` 映射到 TIM2_CH1 的 `0~50%` 占空比；
- LED 低电平点亮，上电默认熄灭；
- CAN 接收中断只负责入队，主循环再解码，避免在中断里做业务；
- 接收队列容量 128 帧，队列满时丢弃新帧，不覆盖旧数据。

已经验证过的：

- 固件能正常编译、烧录、启动；
- 心跳序号按 500 ms 递增；
- 3 秒状态任务能正常触发；
- 用调试器往接收队列注入 `0x301` 后，模型、PWM、LED 都能正确变化；
- CAN 发送没有错误计数；
- 开发时用主机侧临时测试验证了环形队列、设备模型、0x101/0x201/0x401 编解码和命令执行。

### 它现在不做什么

- **温度/光照暂时直接上报 ADC 原始值**，还没有按热敏电阻曲线换算成物理温度；
- 风扇是**开环控制**：只把目标转速换成 PWM，没有测速反馈，`0x101` 里的 `fan_rpm` 是设定值，不是实测值；
- 方向脚 `PB0/PB1` 仍然保持 CubeMX 初始状态，没有在运行时切换正反转；
- 不支持 `0x301` 之外的 CAN 命令；
- 未知命令码目前直接丢弃，不回 ACK；
- 不做掉电保存、看门狗、OTA；
- 没有用 RTOS，所有任务都在主循环里轮询。

> 这是一个个人项目，先把 CAN 闭环跑通，再考虑看门狗、掉电保存、异常恢复这些功能。

---

## 2. 接线和准备工作

### 需要的东西

- STM32F103C8 下位机板（64 KB Flash / 20 KB RAM）；
- CAN 收发器，以及 F407 网关；
- DAPLink 或 ST-LINK 下载器；
- 热敏电阻、光敏电阻（可选，不接也能跑，ADC 会读到浮动值）；
- 风扇驱动板和小风扇（可选）；
- PC 上装好 `cmake`、`ninja`、`arm-none-eabi-gcc`、`openocd`、`python3`。

### CAN

- 两边 CAN 波特率都是 **500 kbps**；
- F103 的 CAN 引脚：

| 引脚 | 功能 |
|---|---|
| `PA11` | CAN_RX |
| `PA12` | CAN_TX |

- F407 的 CAN 引脚是 `PD0/PD1`；
- **两块板的 CAN 收发器必须共地**。只接 CANH/CANL、不共地，很可能完全收不到；
- CAN 总线两端按需要接 `120Ω` 终端电阻；
- F103 只接收 `0x301`，过滤器只放行这个 ID。

### 传感器

| 引脚 | 功能 | 说明 |
|---|---|---|
| `PA1` | ADC1_IN1 | 热敏电阻 |
| `PA2` | ADC1_IN2 | 光敏电阻 |

ADC 配置为 12 位、连续扫描、DMA 循环搬运，设备任务直接读 `g_adc_raw[0]` / `g_adc_raw[1]`。

### 风扇和 LED

| 引脚 | 功能 | 当前行为 |
|---|---|---|
| `PA0` | TIM2_CH1 PWM | 1 kHz，占空比 `0~50%`，上电默认 0% |
| `PB0` | Direction_control1 | 上电默认高，运行时不改 |
| `PB1` | Direction_control2 | 上电默认低，运行时不改 |
| `PA3` | LED1 | **低电平点亮**，上电默认高（熄灭） |
| `PC13` | TEST_LED | 收到 CAN 帧时翻转，方便看中断有没有进 |

风扇转速映射：

```text
0 rpm    -> CCR1 = 0    -> 0%
1500 rpm -> CCR1 = 250  -> 25%
3000 rpm -> CCR1 = 500  -> 50%
```

### 调试接口

- `PA13`：SWDIO；
- `PA14`：SWCLK；
- `GND`：必须和下载器共地。

---

## 3. 编译和烧录

### 3.1 编译

```bash
cd node-f103
python3 .scripts/build.py
```

默认使用 `Debug`，生成：

```text
build/Debug/node-f103.elf
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

## 4. CAN 协议

### 4.1 基本参数

| 项目 | 当前值 |
|---|---|
| 波特率 | 500 kbps |
| 帧类型 | 标准数据帧，11 位 ID |
| 多字节字节序 | 小端 |
| 接收过滤器 | 只放行 `0x301` |

### 4.2 报文总表

| ID | 方向 | DLC | 作用 |
|---|---|---|---|
| `0x101` | F103 → F407 | 8 | 温度/光照/风扇/LED 状态 |
| `0x201` | F103 → F407 | 2 | 心跳 |
| `0x301` | F407 → F103 | 8 | 风扇/LED 命令 |
| `0x401` | F103 → F407 | 3 | 命令 ACK |

### 4.3 `0x101` 状态帧

DLC = 8。

```text
Byte 0-1  温度，小端（协议约定 ×10，即 25.0℃ = 250）
Byte 2-3  光照，小端
Byte 4-5  风扇转速，小端
Byte 6    LED 状态：0 灭、1 亮
Byte 7    保留，填 0
```

当前实现里：

- 温度/光照直接填 ADC 原始值，还没有做物理量换算；
- 风扇转速填的是**命令设定值**，不是实测转速；
- 有效命令执行后会立即补发一帧，之后仍然按 3 秒周期发送。

> 注意：F407 的 `docs/protocol.md` 里写的是 500 ms 一帧，那是早期约定；当前 F103 默认 **3 秒**一帧。  
> 需要修改时，改 `device/device.c` 里的 `DEVICE_DEFAULT_UPDATE_PERIOD_MS`，或者运行时调用 `Device_SetUpdatePeriodMs()`。

### 4.4 `0x201` 心跳帧

DLC = 2，每 500 ms 一帧。

```text
Byte 0  Node ID，当前为 1
Byte 1  心跳序号，0~255 循环
```

F407 超过 1.5 秒没收到心跳，会把 `online` 置为 1（离线）。

### 4.5 `0x301` 命令帧

DLC = 8。

风扇命令：

```text
Byte 0    01
Byte 1-2  目标转速，小端，0~3000
Byte 3    命令序号 seq
Byte 4-7  保留，填 0
```

例如风扇 1500：

```text
01 DC 05 07 00 00 00 00
```

LED 命令：

```text
Byte 0    02
Byte 1    LED 状态：0 灭、1 亮
Byte 2    0
Byte 3    命令序号 seq
Byte 4-7  保留，填 0
```

例如 LED 打开、seq = 8：

```text
02 01 00 08 00 00 00 00
```

### 4.6 `0x401` ACK 帧

DLC = 3，命令处理完后立即回复。

```text
Byte 0  原命令码：0x01 或 0x02
Byte 1  结果：0 成功、1 失败、2 非法
Byte 2  原命令序号 seq
```

当前实现：

- 命令合法并执行成功：结果 `0`；
- 风扇超过 3000 或 LED 不是 0/1：结果 `2`；
- 失败结果 `1` 目前没有使用；
- 未知命令码直接丢弃，不回 ACK。

### 4.7 时序汇总

| 项目 | 当前值 |
|---|---:|
| `0x101` 状态帧 | 默认 3000 ms |
| `0x201` 心跳 | 500 ms |
| `0x401` ACK | 收到 `0x301` 后立即回复 |
| F407 判定离线 | 1500 ms 无心跳 |

---

## 5. 数据和命令怎么走

### 接收命令

```text
CAN 中断
  → 取帧，填 can_frame_t
  → RingBuffer_Push 入队
  → 主循环 App_Task
  → App_CanTask 出队
  → Can_App_Deco 解码 0x301
  → 校验命令码和参数
  → 更新 device_model
  → 设置 PWM / LED
  → Can_App_Ack_Frame_Code 编 0x401
  → Can_Port_send 发 ACK
  → 立即补发一帧 0x101
```

### 周期状态

```text
App_Task
  → Device_Task 检查是否到 3 秒
  → 读 ADC 原始值
  → 更新 device_model 的温度/光照
  → Can_App_State_Code 编 0x101
  → Can_Port_send 发送
```

### 心跳

```text
App_Task
  → App_HeartbeatTask 检查是否到 500 ms
  → Can_App_Heart_Code 编 0x201
  → Can_Port_send 发送
```

主循环的节奏是：

```c
while (1) {
    App_Task();
    HAL_Delay(10);
}
```

所以 3 秒和 500 ms 的定时精度大约是 10 ms 级别，对状态上报和心跳够用。

---

## 6. 代码是怎么分层的

```text
node-f103/
├── app/        应用编排：命令处理、心跳、状态任务、执行器
├── bsp/        CAN 驱动、环形队列
├── device/     设备模型、状态上报
├── protocol/   CAN 帧结构和应用层编解码
├── Core/       STM32CubeMX 生成的芯片初始化
├── cmake/      工具链和构建配置
└── .scripts/   编译和烧录脚本
```

每一层只干自己的事：

### `app/`

负责“把各个模块串起来”：

- `App_Init()`：初始化设备模型、心跳计时、执行器初始状态；
- `App_Task()`：每轮主循环调用；
- `App_CanTask()`：从接收队列取命令，解码并执行；
- `App_HeartbeatTask()`：每 500 ms 发心跳；
- `App_ApplyFan()` / `App_ApplyLed()`：把命令落到 PWM 和 GPIO。

### `bsp/`

负责“和硬件打交道”：

- `can_port.c`：CAN 过滤器、接收中断、发送帧；
- `ring_buffer.c`：通用环形队列，中断 Push、主循环 Pop。

`can_port` 不解析命令含义，`ring_buffer` 也不知道 CAN 是什么。

### `device/`

负责“设备现在是什么状态”：

- `device_model_t`：温度、光照、风扇、LED；
- `Device_Task()`：每 3 秒刷新 ADC，并发送 0x101；
- `Device_SendStateFrame()`：读模型、编码、发送；
- `Device_SetFanRpm()` / `Device_SetLedState()`：命令执行后更新模型。

### `protocol/`

负责“把字节翻译成含义”：

- `can_frame.h`：统一的 CAN 帧结构；
- `can_app.c`：`0x101/0x201/0x301/0x401` 的编解码。

这一层不碰 GPIO，也不碰 HAL 的发送流程。

### `Core/`

CubeMX 生成的初始化代码：

- 时钟、GPIO、CAN、TIM2、ADC、DMA；
- 中断向量和 HAL 配置。

一般不需要手改，除非改硬件配置。

---

## 7. 怎么测试

### 通过 F407 网关测试

仓库根目录的 `Test/modbus_test.py` 可以直接用：

```bash
cd /home/mone/Project/stm32-industrial-protocol-gateway-v2
python3 Test/modbus_test.py
```

进入交互后：

```text
read
fan 1500
led 1
watch 10 0.5
led 0
fan 0
exit
```

预期：

- `fan 1500` 后 `cmd_result` 变 0，`fan_rpm = 1500`，风扇 25%；
- `led 1` 后 `led_state = 1`，LED 点亮；
- `watch` 能看到 3 秒状态帧带来的温度/光照变化；
- `led 0`、`fan 0` 后 LED 熄灭、风扇停转。

更详细的命令和原始报文见：

- [Modbus 测试工具说明](../Test/README.md)

### 直接用 CAN 分析仪测试

把分析仪设成 500 kbps，发送标准数据帧。

风扇 1500：

```text
ID:   0x301
DLC:  8
Data: 01 DC 05 07 00 00 00 00
```

预期收到：

```text
0x401  DLC=3  Data=01 00 07
0x101  DLC=8  Data=<温度低> <温度高> <光照低> <光照高> DC 05 <LED 0/1> 00
```

LED 打开：

```text
ID:   0x301
DLC:  8
Data: 02 01 00 08 00 00 00 00
```

预期收到：

```text
0x401  DLC=3  Data=02 00 08
```

非法风扇转速 3001：

```text
ID:   0x301
DLC:  8
Data: 01 B9 0B 09 00 00 00 00
```

预期收到：

```text
0x401  DLC=3  Data=01 02 09
```

结果 `02` 表示“非法”。

心跳可以周期性看到：

```text
0x201  DLC=2  Data=01 00
0x201  DLC=2  Data=01 01
0x201  DLC=2  Data=01 02
...
```

---

## 8. 常见问题

### F407 读到的 online 一直是 1（离线）

说明 F407 没收到 `0x201` 心跳。检查：

- F103 有没有正常上电、程序有没有跑起来；
- CAN 收发器有没有共地；
- 两边波特率是否都是 500 kbps；
- CANH/CANL 有没有接反；
- F103 的 `PC13` 在收到 CAN 帧时会不会翻转。

### 写命令后 `cmd_result` 一直是 1（在途）

说明命令发出去了，但 F103 没有回 `0x401`。检查：

- F103 有没有收到 `0x301`；
- F103 有没有进入 `App_CanTask`；
- ACK 帧的 ID、DLC、命令码、序号是否正确；
- F103 是不是停在 `Error_Handler()`。

### `cmd_result` 变成 3（超时）

说明 F407 在 1 秒内没收到匹配的 ACK。检查：

- CAN 线有没有接好；
- 两边波特率是否一致；
- F103 是否正常供电；
- ACK 里的 seq 是否和原命令一致。

### 风扇不转

检查：

- `0x101` 里的 `fan_rpm` 是不是 0；
- 命令值是否在 `0~3000`；
- `PA0` 有没有 PWM 输出；
- 风扇驱动板有没有供电；
- `PB0/PB1` 的方向组合是否符合你的驱动板。

注意：当前 `PB0/PB1` 保持上电默认值，不会随命令改变。

### LED 亮灭反了

LED 是**低电平点亮**：

- 写 `1` -> `PA3` 输出低 -> 点亮；
- 写 `0` -> `PA3` 输出高 -> 熄灭。

如果你的板子是高电平点亮，需要改 `app/app.c` 里的 `App_ApplyLed()`。

### 温度/光照数值很奇怪

当前是 ADC 原始值，不是物理温度。温度字段协议上按 `×0.1℃` 解释，但代码还没做热敏电阻换算。等传感器参数确定后，在 `Device_UpdateModel()` 里补换算。

### 烧录失败

```bash
python3 .scripts/flash.py --probe stlink
```

或者检查：

- DAPLink 的 SWD 接线；
- 板子供电；
- `GND` 是否共地；
- 芯片有没有被读保护。

---

## 9. 文档

- [F407 网关说明](../gateway-f407/README.md)
- [CAN 协议和寄存器详细说明](../gateway-f407/docs/protocol.md)
- [Modbus TCP 测试工具](../Test/README.md)
- [测试脚本](../Test/modbus_test.py)

---

## 10. 当前状态

已经完成并验证：

- 环形队列接收 CAN 帧；
- `0x301` 风扇/LED 命令解码；
- `0x401` ACK 编码和发送；
- `0x201` 500 ms 心跳；
- `0x101` 3 秒状态帧；
- 设备模型维护温度、光照、风扇、LED；
- 风扇 `0~3000` 映射 `0~50%` PWM；
- LED 低电平点亮；
- 开发时主机侧测试和实际板子运行验证。

还需要继续做的：

- 温度/光照传感器标定；
- 风扇测速和闭环控制；
- 方向脚控制（如果需要正反转）；
- 未知命令的 ACK；
- ACK 发送失败重试；
- 看门狗、掉电保存、异常恢复等量产功能。

目前这套代码已经足够跑通：

```text
PC -> Modbus TCP -> F407 -> CAN 0x301 -> F103 -> PWM/LED
F103 -> CAN 0x401/0x101/0x201 -> F407 -> Modbus 缓存
```

完整的 Modbus 闭环，建议用根目录的 `Test/modbus_test.py` 实际写一次风扇和 LED 来确认。
