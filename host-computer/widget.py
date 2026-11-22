from PySide6.QtWidgets import QWidget, QMessageBox
from PySide6.QtCore import QTimer
from PySide6.QtGui import QCloseEvent
from ui_widget import Ui_Widget
from modbustcp import ModbusTCP
import datetime

class Widget(QWidget, Ui_Widget):
    def __init__(self, c: ModbusTCP) -> None:
        super().__init__()
        self.setupUi(self)
        self.setWindowTitle("网关上位机")
        self.gateway = c

        self.led_on_button.clicked.connect(self.set_led_on)
        self.led_off_button.clicked.connect(self.set_led_off)
        self.set_rpm_button.clicked.connect(self.set_fan_rpm)
        self.update_button.clicked.connect(self.update_device_model)

        self.timer = QTimer(self)
        self.timer.setInterval(1000)
        self.timer.timeout.connect(self.update_device_model_silence)
        self.timer.start()

    def set_led_on(self) -> None:
        if not self.gateway.set_led(True):
            self.append_log("设置LED状态(1)失败")
            return None
        self.append_log("设置LED状态(1)成功")
        self.update_device_model_silence()

    def set_led_off(self) -> None:
        if not self.gateway.set_led(False):
            self.append_log("设置LED状态(0)失败")
            return None
        self.append_log("设置LED状态(0)成功")
        self.update_device_model_silence()

    def set_fan_rpm(self) -> None:
        if not self.gateway.set_speed(self.rpm_set.value()):
            self.append_log("设置转速失败")
            return None
        self.append_log(f"设置转速成功: {self.rpm_set.text()}")
        self.rpm_set.setValue(0)
        self.update_device_model_silence()

    def update_device_model(self) -> None:
        recv = self.gateway.read_device_model()
        if recv == None:
            print("设备模型更新失败")
            self.append_log("设备模型更新失败")
            return None
        print(f"设备模型更新成功 {recv}")
        self.append_log(f"设备模型更新成功 {recv}")
        self.temperature_value_label.setText(f"{recv[0]}")
        self.light_value_label.setText(f"{recv[1]}")
        self.fan_rpm_value_label.setText(f"{recv[2]}")
        self.led_state_value_label.setText(f"{recv[3]}")
        self.cmd_result_value_label.setText(f"{recv[4]}")
        self.online_value_label.setText(f"{recv[5]}")
        self.protocol_version_value_label.setText(f"{recv[6]}")

    def update_device_model_silence(self) -> None:
        recv = self.gateway.read_device_model()
        if recv == None:
            return None
        self.temperature_value_label.setText(f"{recv[0]}")
        self.light_value_label.setText(f"{recv[1]}")
        self.fan_rpm_value_label.setText(f"{recv[2]}")
        self.led_state_value_label.setText(f"{recv[3]}")
        self.cmd_result_value_label.setText(f"{recv[4]}")
        self.online_value_label.setText(f"{recv[5]}")
        self.protocol_version_value_label.setText(f"{recv[6]}")

    def append_log(self, message: str):
        time_str = datetime.datetime.now().strftime("%H:%M:%S")
        log_msg = f"[{time_str}] {message}"
        
        # 追加到文本框
        self.log_textedit.append(log_msg)
        
        # 自动滚动到最底部，保证最新日志可见
        scrollbar = self.log_textedit.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def closeEvent(self, event: QCloseEvent) -> None:
        # 弹出确认框
        reply = QMessageBox.question(
            self, 
            '确认退出', 
            '确定要退出上位机吗？退出后将断开设备连接。',
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No, 
            QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            # 用户点了“是”，执行清理
            if hasattr(self, 'timer') and self.timer.isActive():
                self.timer.stop()
            if hasattr(self, 'gateway') and self.gateway:
                self.gateway.close()
                
            event.accept()  # 允许关闭
        else:
            # 用户点了“否”，忽略关闭事件（窗口保持打开）
            event.ignore()