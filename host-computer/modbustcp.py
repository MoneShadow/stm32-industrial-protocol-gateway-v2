"""
读寄存器有7个            写寄存器有两个
0x0000  温度
0x0001  光照
0x0002  转速            0x0002  转速       (0~3000 对应占空比 0~50%)
0x0003  led状态         0x0003  led状态    (0/1 关/开)
0x0004  命令结果
0x0005  在线状态
0x0006  协议版本

"""
from pymodbus.client import ModbusTcpClient

class ModbusTCP():
    def __init__(self, host='192.168.0.200', port=502, device_id=1, timeout=0.3) -> None:
        self.client = ModbusTcpClient(host=host, port=port, timeout=timeout)
        self.device_id = device_id

    def connect(self) -> bool:
        return self.client.connect()

    def close(self) -> None:
        self.client.close()

    # ================= 读寄存器业务 =================
    def read_temperature(self) -> int | None:
        # 温度 0x0000
        res = self.client.read_holding_registers(0x0000, count=1, device_id=self.device_id)
        if res.isError():
            print(f"读取温度失败，异常信息: {res}")
            return None
        temp_val = res.registers[0]
        print(f"温度: {temp_val}")
        return temp_val

    def read_light(self) -> int | None:
        # 光照 0x0001
        res = self.client.read_holding_registers(0x0001, count=1, device_id=self.device_id)
        if res.isError():
            print(f"读取光照失败，异常信息: {res}")
            return None
        light_val = res.registers[0]
        print(f"光照: {light_val}")
        return light_val

    def read_speed(self) -> int | None:
        # 转速 0x0002
        res = self.client.read_holding_registers(0x0002, count=1, device_id=self.device_id)
        if res.isError():
            print(f"读取转速失败，异常信息: {res}")
            return None
        rpm_val = res.registers[0]
        print(f"转速: {rpm_val}")
        return rpm_val
    
    def read_led_status(self) -> int | None:
        # led状态 0x0003
        res = self.client.read_holding_registers(0x0003, count=1, device_id=self.device_id)
        if res.isError():
            print(f"读取状态失败，异常信息: {res}")
            return None
        state_val = res.registers[0]
        print(f"led状态: {state_val}")
        return state_val

    def read_device_model(self):
        res = self.client.read_holding_registers(0x0000, count=7, device_id=self.device_id)
        if res.isError():
            print(f"读取失败，异常信息: {res}")
            return None
        device_model = res.registers
        return device_model

    # ================= 写寄存器业务 =================
    def set_speed(self, fan_rpm: int) -> bool:
        if not (0 <= fan_rpm <= 3000):
            raise ValueError("风扇转速在0 ~ 3000")
        res = self.client.write_register(0x0002, fan_rpm, device_id=self.device_id)
        if res.isError():
            print(f"写入失败，异常信息: {res}")
            return False
        print(f"写入成功 {res.registers[0]}")
        return True
        
    def set_led(self, turn_on: bool) -> bool:
        value = 1 if turn_on else 0
        res = self.client.write_register(0x0003, value, device_id=self.device_id)
        if res.isError():
            print(f"写入失败，异常信息: {res}")
            return False
        print(f"写入成功 {res.registers[0]}")
        return True
