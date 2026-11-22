# PC 上位机

> 这个目录只放 **PC 端上位机**。  
> F407 网关在 `../gateway-f407`，F103 下位机在 `../node-f103`。

一句话说明：

> 用 Qt 界面通过 Modbus TCP 连接 F407 网关，显示 7 个设备寄存器，控制风扇和 LED。

---

## 1. 现在能做什么

- 显示温度、光照、风扇转速、LED 状态、命令结果、在线状态、协议版本；
- 每 1 秒自动刷新一次状态；
- 设置风扇目标转速 `0~3000`；
- 打开/关闭 LED；
- 日志区域显示操作记录；
- 关闭窗口前弹出确认，退出时停止定时器并断开 Modbus 连接。

---

## 2. 运行前准备

需要先准备好：

- F407 网关已经启动；
- PC 和网关在同一个网段，能 ping 通 `192.168.0.200`；
- F103 下位机已经烧录并运行。

网关默认地址：

```text
IP:      192.168.0.200
端口:    502
Unit ID: 1
```

如果网关 IP 变了，改 [modbustcp.py](modbustcp.py) 里 `ModbusTCP.__init__` 的默认参数。

---

## 3. 安装和运行

第一次运行：

```bash
cd host-computer
python3 -m venv .venv
.venv/bin/pip install PySide6==6.12.0 pymodbus==3.16.2
.venv/bin/python main.py
```

如果 `.venv` 已经存在：

```bash
.venv/bin/python main.py
```

程序启动时会先连接网关。连接失败会直接退出，所以建议先启动网关，再打开上位机。

---

## 4. 界面怎么用

### 状态显示

左侧是 7 个寄存器：

```text
温度、光照、风扇、LED、命令结果、在线、协议版本
```

- 程序每 1 秒自动刷新一次；
- 点击 `Update` 可以立即刷新一次；
- 温度/光照当前是 ADC 原始值，还没有换算成物理量；
- 风扇转速显示的是下位机的设定值，不是实测转速。

### 设置风扇

1. 在 `RPM` 输入框填 `0~3000`；
2. 点击 `Set RPM`；
3. 写成功后，`cmd_result` 先变成 `1`（在途），收到下位机 ACK 后变成 `0`（成功）。

### 控制 LED

- 点击 `LED ON`：写 `0x0003 = 1`；
- 点击 `LED OFF`：写 `0x0003 = 0`。

写寄存器后的 Modbus 回显只表示“网关收到了”，真正执行结果看 `cmd_result`：

| 值 | 含义 |
|---:|---|
| 0 | 成功 |
| 1 | 在途 |
| 2 | 失败 |
| 3 | 超时 |

### 在线状态

- `0`：在线，F407 在 1.5 秒内收到过 F103 的心跳；
- `1`：离线，没收到心跳。

---

## 5. 打包成可执行文件

打包只在需要分发时做，平时开发直接运行 `main.py` 就行。

### 5.1 安装 PyInstaller

```bash
cd host-computer
.venv/bin/pip install pyinstaller==6.22.3
```

### 5.2 打包

```bash
.venv/bin/pyinstaller --noconfirm --windowed --name HostComputer main.py
```

生成：

```text
dist/HostComputer/
├── HostComputer       ← 主程序
└── _internal/         ← Qt 和 Python 运行库
```

**分发时要整个 `HostComputer/` 目录一起发**，不能只复制 `HostComputer` 这一个文件，否则别的机器上跑不起来。

### 5.3 运行打包结果

```bash
./dist/HostComputer/HostComputer
```

### 5.4 单文件版本（可选）

```bash
.venv/bin/pyinstaller --noconfirm --onefile --windowed --name HostComputer main.py
```

输出是单个 `dist/HostComputer`。单文件启动会慢一点，文件也更大。

### 5.5 注意

- PyInstaller 打出来的是**当前平台**的程序：Linux 上打包只能在 Linux 上跑，Windows 上需要在 Windows 上重新打包；
- `build/`、`dist/` 都是打包产物，不要提交到 Git；
- 如果本地有 `main.spec`，也可以直接 `pyinstaller main.spec`，它默认输出到 `dist/main/`；
- 当前 `.gitignore` 已经忽略 `dist/` 和 `main.spec`。

---

## 6. 常见问题

### 启动就提示“服务端连接失败”

- 先 `ping 192.168.0.200`；
- 确认 F407 网关已经启动；
- 确认 PC 网卡和网关在同一网段；
- 确认 [modbustcp.py](modbustcp.py) 里的 IP、端口没写错。

### `cmd_result` 一直是 1 或 3

说明 F407 发了命令，但没收到 F103 的 ACK：

- 检查 CAN 接线和共地；
- 确认两边波特率都是 500 kbps；
- 确认 F103 已经运行、没有停在 `Error_Handler()`；
- 用 [Test/modbus_test.py](../Test/modbus_test.py) 或 CAN 分析仪先确认链路通。

### `online` 一直是 1

说明 F407 没收到 F103 的心跳 `0x201`，先检查 CAN 链路和下位机程序。

### 界面偶尔卡一下

当前 Modbus 读取是同步的，超时 0.3 秒，网关离线时界面可能短暂卡顿。后面可以改成工作线程，先把功能跑通问题不大。

---

## 7. 依赖版本

```text
Python   3.15.2
PySide6  6.12.0
pymodbus 3.16.2
PyInstaller 6.22.3（仅打包时用）
```
