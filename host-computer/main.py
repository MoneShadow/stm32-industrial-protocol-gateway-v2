from PySide6.QtWidgets import QApplication
from modbustcp import ModbusTCP
from widget import Widget
import sys

def main() -> None:
    c: ModbusTCP = ModbusTCP()
    if not c.connect():
        print("服务端连接失败")
        sys.exit(1)
    print("服务端连接成功")

    app = QApplication(sys.argv)
    window: Widget = Widget(c)
    window.show()

    app.exec()

if __name__ == "__main__":
    raise SystemExit(main())