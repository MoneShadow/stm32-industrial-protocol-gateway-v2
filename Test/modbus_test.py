#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
F407 网关 Modbus TCP 交互测试工具

用法:
    python3 Test/modbus_test.py
    python3 Test/modbus_test.py --host 192.168.0.200 --port 502 --unit 1

输入 exit 或 quit 退出。
"""

from __future__ import annotations

import argparse
import re
import socket
import struct
import time
from datetime import datetime
from typing import List, Optional, Tuple

# ---------------------------------------------------------------------------
# 寄存器定义（以 gateway-f407/docs/protocol.md 为准）
# ---------------------------------------------------------------------------
REG_TEMPERATURE = 0x0000
REG_LIGHT = 0x0001
REG_FAN_RPM = 0x0002
REG_LED_STATE = 0x0003
REG_CMD_RESULT = 0x0004
REG_ONLINE = 0x0005
REG_PROTOCOL_VERSION = 0x0006

REG_COUNT = 7

REG_NAMES = {
    REG_TEMPERATURE: "温度",
    REG_LIGHT: "光照",
    REG_FAN_RPM: "风扇",
    REG_LED_STATE: "LED",
    REG_CMD_RESULT: "命令结果",
    REG_ONLINE: "在线",
    REG_PROTOCOL_VERSION: "协议版本",
}

CMD_RESULT_TEXT = {
    0: "成功",
    1: "在途",
    2: "失败",
    3: "超时",
}

ONLINE_TEXT = {
    0: "在线",
    1: "离线",
}

LED_TEXT = {
    0: "灭",
    1: "亮",
}

EXCEPTION_TEXT = {
    0x01: "非法功能码",
    0x02: "非法地址",
    0x03: "非法数据值",
    0x04: "服务器设备故障",
    0x06: "服务器设备忙",
}

# ---------------------------------------------------------------------------
# Modbus 异常
# ---------------------------------------------------------------------------
class ModbusError(Exception):
    """Modbus 通信/协议错误基类。"""


class ModbusProtocolError(ModbusError):
    """报文格式、事务号、长度等不符合预期。"""


class ModbusExceptionResponse(ModbusError):
    """从站返回的 Modbus 异常响应。"""

    def __init__(self, function: int, code: int) -> None:
        self.function = function
        self.code = code
        name = EXCEPTION_TEXT.get(code, "未知异常")
        super().__init__(
            f"Modbus 异常: 功能码 0x{function:02X}, "
            f"异常码 0x{code:02X} ({name})"
        )


# ---------------------------------------------------------------------------
# Modbus TCP 客户端（标准库实现，无第三方依赖）
# ---------------------------------------------------------------------------
class ModbusTcpClient:
    def __init__(self, host: str, port: int, unit: int, timeout: float = 2.0) -> None:
        self.host = host
        self.port = port
        self.unit = unit
        self.timeout = timeout
        self._sock: Optional[socket.socket] = None
        self._tid = 0

    def connect(self) -> None:
        self.close()
        self._sock = socket.create_connection(
            (self.host, self.port), timeout=self.timeout
        )
        self._sock.settimeout(self.timeout)

    def close(self) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    def ensure_connected(self) -> None:
        if self._sock is None:
            self.connect()

    def _next_tid(self) -> int:
        self._tid = (self._tid + 1) & 0xFFFF
        return self._tid

    def _recv_exact(self, size: int) -> bytes:
        if self._sock is None:
            raise ModbusProtocolError("未连接")
        data = bytearray()
        while len(data) < size:
            chunk = self._sock.recv(size - len(data))
            if not chunk:
                raise ModbusProtocolError("连接被对端关闭")
            data.extend(chunk)
        return bytes(data)

    def _recv_response(self) -> Tuple[int, int, bytes, bytes]:
        """读取一条完整响应，返回 (TID, UnitID, PDU, 完整 ADU)。"""
        mbap = self._recv_exact(6)
        tid, protocol, length = struct.unpack(">HHH", mbap)
        if protocol != 0:
            raise ModbusProtocolError(f"Protocol ID 非 0: {protocol}")
        if length < 2 or length > 254:
            raise ModbusProtocolError(f"MBAP Length 非法: {length}")

        payload = self._recv_exact(length)
        unit = payload[0]
        pdu = payload[1:]
        return tid, unit, pdu, mbap + payload

    def transact(self, pdu: bytes) -> Tuple[int, int, bytes]:
        """发送一个 PDU，返回 (TID, UnitID, 响应 PDU)。"""
        self.ensure_connected()
        tid = self._next_tid()
        adu = struct.pack(">HHHB", tid, 0, len(pdu) + 1, self.unit) + pdu

        assert self._sock is not None
        self._sock.sendall(adu)

        r_tid, r_unit, r_pdu, _ = self._recv_response()
        if r_tid != tid:
            raise ModbusProtocolError(
                f"事务号不匹配: 发送 {tid}, 收到 {r_tid}"
            )
        return r_tid, r_unit, r_pdu

    def raw_transact(self, adu: bytes) -> bytes:
        """发送完整 ADU，返回完整响应 ADU。"""
        self.ensure_connected()
        if len(adu) < 8:
            raise ValueError("报文至少需要 8 字节")

        req_tid = struct.unpack(">H", adu[:2])[0]
        assert self._sock is not None
        self._sock.sendall(adu)

        r_tid, _, _, raw = self._recv_response()
        if r_tid != req_tid:
            raise ModbusProtocolError(
                f"事务号不匹配: 发送 {req_tid}, 收到 {r_tid}"
            )
        return raw

    @staticmethod
    def _check_exception(pdu: bytes) -> None:
        if len(pdu) >= 2 and (pdu[0] & 0x80) != 0:
            raise ModbusExceptionResponse(pdu[0] & 0x7F, pdu[1])

    def read_holding(self, address: int, quantity: int) -> List[int]:
        if not 0 <= address <= 0xFFFF:
            raise ValueError("起始地址必须在 0~65535")
        if not 1 <= quantity <= REG_COUNT:
            raise ValueError(f"读取数量必须在 1~{REG_COUNT}")

        pdu = struct.pack(">BHH", 0x03, address, quantity)
        _, _, response = self.transact(pdu)
        self._check_exception(response)

        if len(response) < 2 or response[0] != 0x03:
            raise ModbusProtocolError(
                f"读响应格式错误: {response.hex(' ').upper()}"
            )

        byte_count = response[1]
        if byte_count != quantity * 2 or len(response) != 2 + byte_count:
            raise ModbusProtocolError(
                f"读响应长度错误: 期望 {quantity * 2} 字节, "
                f"实际 {byte_count} 字节"
            )

        return list(struct.unpack(">" + "H" * quantity, response[2:]))

    def write_single(self, address: int, value: int) -> Tuple[int, int]:
        if not 0 <= address <= 0xFFFF:
            raise ValueError("地址必须在 0~65535")
        if not 0 <= value <= 0xFFFF:
            raise ValueError("值必须在 0~65535")

        pdu = struct.pack(">BHH", 0x06, address, value)
        _, _, response = self.transact(pdu)
        self._check_exception(response)

        if len(response) != 5 or response[0] != 0x06:
            raise ModbusProtocolError(
                f"写响应格式错误: {response.hex(' ').upper()}"
            )

        echo_address, echo_value = struct.unpack(">HH", response[1:5])
        if echo_address != address or echo_value != value:
            raise ModbusProtocolError("写响应回显与请求不一致")

        return echo_address, echo_value


# ---------------------------------------------------------------------------
# 显示
# ---------------------------------------------------------------------------
def describe_register(address: int, value: int) -> str:
    if address == REG_TEMPERATURE:
        return f"按协议 ×0.1℃ = {value / 10.0:.1f} ℃"
    if address == REG_LIGHT:
        return "光照原始值"
    if address == REG_FAN_RPM:
        return f"{value} rpm"
    if address == REG_LED_STATE:
        return LED_TEXT.get(value, "未知")
    if address == REG_CMD_RESULT:
        return CMD_RESULT_TEXT.get(value, "未知")
    if address == REG_ONLINE:
        return ONLINE_TEXT.get(value, "未知")
    if address == REG_PROTOCOL_VERSION:
        return "协议版本"
    return "-"


def print_registers(values: List[int], start: int = 0, title: str = "寄存器") -> None:
    print(f"\n--- {title} ---")
    for offset, value in enumerate(values):
        address = start + offset
        name = REG_NAMES.get(address, "未知")
        print(
            f"  0x{address:04X}  {name:<4} = {value:<6}  "
            f"{describe_register(address, value)}"
        )


def summary_line(values: List[int]) -> str:
    if len(values) < REG_COUNT:
        return " ".join(
            f"0x{i:04X}={value}" for i, value in enumerate(values)
        )
    return (
        f"温度={values[0]}({values[0] / 10.0:.1f}℃) "
        f"光照={values[1]} 风扇={values[2]} "
        f"LED={values[3]}({LED_TEXT.get(values[3], '?')}) "
        f"cmd={values[4]}({CMD_RESULT_TEXT.get(values[4], '?')}) "
        f"在线={values[5]}({ONLINE_TEXT.get(values[5], '?')}) "
        f"版本={values[6]}"
    )


# ---------------------------------------------------------------------------
# 命令
# ---------------------------------------------------------------------------
def parse_int(text: str) -> int:
    return int(text, 0)


def wait_cmd_result(client: ModbusTcpClient, timeout: float = 1.5) -> List[int]:
    """写命令后轮询 cmd_result，直到不再是“在途”或超时。"""
    deadline = time.monotonic() + timeout
    values = client.read_holding(0, REG_COUNT)

    while values[REG_CMD_RESULT] == 1 and time.monotonic() < deadline:
        time.sleep(0.1)
        values = client.read_holding(0, REG_COUNT)

    return values


def cmd_read(client: ModbusTcpClient, args: List[str]) -> None:
    if not args:
        print_registers(client.read_holding(0, REG_COUNT))
        return

    if len(args) != 2:
        print("用法: read [起始地址] [数量]")
        return

    address = parse_int(args[0])
    quantity = parse_int(args[1])

    if (
        address < 0
        or quantity < 1
        or quantity > REG_COUNT
        or address + quantity > REG_COUNT
    ):
        print(f"地址范围 0x0000~0x{REG_COUNT - 1:04X}，且 起始地址+数量 <= {REG_COUNT}")
        return

    values = client.read_holding(address, quantity)
    print_registers(
        values,
        start=address,
        title=f"寄存器 0x{address:04X} 起 {quantity} 个",
    )


def cmd_fan(client: ModbusTcpClient, args: List[str]) -> None:
    if len(args) != 1:
        print("用法: fan <0~3000>")
        return

    rpm = parse_int(args[0])
    if not 0 <= rpm <= 3000:
        print("风扇转速范围 0~3000")
        return

    address, value = client.write_single(REG_FAN_RPM, rpm)
    print(f"写风扇: 0x{address:04X} = {value} (回显成功)")
    print("回显只代表网关已接受，最终执行结果看 cmd_result。")

    time.sleep(0.2)
    print_registers(wait_cmd_result(client), title="写入后状态")


def cmd_led(client: ModbusTcpClient, args: List[str]) -> None:
    if len(args) != 1:
        print("用法: led <0|1>")
        return

    state = parse_int(args[0])
    if state not in (0, 1):
        print("LED 状态只能是 0 或 1")
        return

    address, value = client.write_single(REG_LED_STATE, state)
    print(f"写 LED: 0x{address:04X} = {value} (回显成功)")

    time.sleep(0.2)
    print_registers(wait_cmd_result(client), title="写入后状态")


def cmd_write(client: ModbusTcpClient, args: List[str]) -> None:
    if len(args) != 2:
        print("用法: write <地址> <值>")
        return

    address = parse_int(args[0])
    value = parse_int(args[1])
    echo_address, echo_value = client.write_single(address, value)
    print(f"写寄存器: 0x{echo_address:04X} = {echo_value} (回显)")


def cmd_watch(client: ModbusTcpClient, args: List[str]) -> None:
    duration = 10.0
    interval = 0.5

    if len(args) >= 1:
        duration = float(args[0])
    if len(args) >= 2:
        interval = float(args[1])
    if len(args) > 2:
        print("用法: watch [秒] [间隔秒]")
        return
    if duration <= 0 or interval <= 0:
        print("时间和间隔必须大于 0")
        return

    print(
        f"监视 {duration:g}s，每 {interval:g}s 采样一次，"
        f"只打印变化（Ctrl-C 停止）"
    )

    deadline = time.monotonic() + duration
    last: Optional[List[int]] = None

    while time.monotonic() < deadline:
        values = client.read_holding(0, REG_COUNT)
        if values != last:
            print(f"[{datetime.now():%H:%M:%S}] {summary_line(values)}")
            last = values
        time.sleep(interval)


def looks_like_hex_adu(parts: List[str]) -> bool:
    """判断一行输入是不是完整的十六进制 ADU，例如 00 01 00 00 ...。"""
    if len(parts) < 4:
        return False
    return all(
        re.fullmatch(r"(0[xX])?[0-9A-Fa-f]{2}", part) is not None
        for part in parts
    )


def cmd_raw(client: ModbusTcpClient, args: List[str]) -> None:
    if not args:
        print("用法: raw <完整 Modbus TCP ADU 十六进制>")
        print("示例: raw 000100000006010300000007")
        print("      raw 00 01 00 00 00 06 01 03 00 00 00 07")
        return

    # 允许 "000100..."、"00 01 00 ..."、"00,01,..."、"0x00 0x01..." 等写法
    text = "".join(args)
    text = text.replace("0x", "").replace("0X", "")
    text = text.replace(",", "").replace(":", "")
    try:
        adu = bytes.fromhex(text)
    except ValueError as exc:
        print(f"十六进制格式错误: {exc}")
        return

    if len(adu) < 8:
        print("报文至少需要 8 字节")
        return

    response = client.raw_transact(adu)
    print(f"TX: {adu.hex(' ').upper()}")
    print(f"RX: {response.hex(' ').upper()}")

    # 简单解析响应，方便看异常/读值
    if len(response) >= 9:
        function = response[7]
        if (function & 0x80) != 0:
            code = response[8]
            print(
                f"    -> 异常响应: 功能码 0x{function & 0x7F:02X}, "
                f"异常码 0x{code:02X} ({EXCEPTION_TEXT.get(code, '未知')})"
            )
        elif function == 0x03 and len(response) >= 9:
            byte_count = response[8]
            if byte_count % 2 == 0 and len(response) >= 9 + byte_count:
                values = list(
                    struct.unpack(
                        ">" + "H" * (byte_count // 2),
                        response[9 : 9 + byte_count],
                    )
                )
                print(f"    -> 读响应: {values}")


HELP_TEXT = """
命令:
  read [起始地址] [数量]   读寄存器，默认读 0x0000~0x0006
  fan <0~3000>             写风扇目标转速
  led <0|1>                写 LED 状态
  write <地址> <值>        直接写单个保持寄存器
  watch [秒] [间隔]        周期读并只打印变化，默认 10s / 0.5s
  raw <hex>                发送完整 ADU；可带空格，也可直接粘贴整行
  help / ?                 显示帮助
  exit / quit              退出

示例:
  read
  fan 1500
  led 1
  read 4 3
  watch 10 0.5
  raw 000100000006010300000007
  raw 00 01 00 00 00 06 01 03 00 00 00 07
  00 01 00 00 00 06 01 03 00 00 00 07

说明:
  - 写回显只代表网关接受了请求，真正结果看 0x0004 cmd_result；
   - cmd_result: 0=成功, 1=在途, 2=失败, 3=超时；
   - 同一时间只允许一条写命令在途。
"""



def handle_command(client: ModbusTcpClient, line: str) -> bool:
    parts = line.split()
    command = parts[0].lower()
    args = parts[1:]

    # 直接粘贴一整行十六进制报文时，自动按 raw 处理
    if looks_like_hex_adu(parts):
        cmd_raw(client, parts)
        return True

    if command in ("exit", "quit"):
        return False
    if command in ("help", "?"):
        print(HELP_TEXT)
    elif command in ("read", "r", "status", "s"):
        cmd_read(client, args)
    elif command == "fan":
        cmd_fan(client, args)
    elif command == "led":
        cmd_led(client, args)
    elif command in ("write", "w"):
        cmd_write(client, args)
    elif command in ("watch", "monitor"):
        cmd_watch(client, args)
    elif command in ("raw", "send"):
        cmd_raw(client, args)
    else:
        print(f"未知命令: {command}，输入 help 查看命令")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="F407 网关 Modbus TCP 交互测试工具"
    )
    parser.add_argument("--host", default="192.168.0.200", help="网关 IP")
    parser.add_argument("--port", type=int, default=502, help="Modbus TCP 端口")
    parser.add_argument("--unit", type=int, default=1, help="Unit ID")
    parser.add_argument(
        "--timeout", type=float, default=2.0, help="单次通信超时（秒）"
    )
    args = parser.parse_args()

    if not 0 <= args.unit <= 255:
        parser.error("Unit ID 必须在 0~255")
    if not 1 <= args.port <= 65535:
        parser.error("端口必须在 1~65535")
    if args.timeout <= 0:
        parser.error("超时必须大于 0")

    client = ModbusTcpClient(args.host, args.port, args.unit, args.timeout)

    print(f"目标: {args.host}:{args.port}, Unit ID={args.unit}")
    try:
        client.connect()
        print("已连接")
    except OSError as exc:
        print(f"连接失败: {exc}")
        print("仍可继续输入命令；下一次通信时会自动重试连接。")

    print("输入 help 查看命令，输入 exit 退出。")

    while True:
        try:
            line = input("modbus> ").strip()
        except EOFError:
            print()
            break
        except KeyboardInterrupt:
            print("\n输入 exit 退出。")
            continue

        if not line:
            continue

        try:
            if not handle_command(client, line):
                break
        except ModbusExceptionResponse as exc:
            print(str(exc))
        except ModbusProtocolError as exc:
            print(f"协议错误: {exc}")
            client.close()
        except OSError as exc:
            print(f"通信错误: {exc}")
            client.close()
        except ValueError as exc:
            print(f"参数错误: {exc}")
        except KeyboardInterrupt:
            print("\n已停止当前操作。")

    client.close()
    print("已退出。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
