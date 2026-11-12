#!/usr/bin/env python3
"""

设计目标：同一块板子换项目时不需要改脚本。
  · 固件位置自动在 build/<preset>/ 下查找，不写死文件名
  · 探针、芯片型号集中在本文件的常量里，换硬件只改一处
  · 失败时复位探针并重试一次（廉价 DAPLink 偶尔在烧录中卡死）

用法:
  python3 .scripts/flash.py                  # DAPLink + Debug，最常用
  python3 .scripts/flash.py --probe stlink   # 临时换回 ST-LINK
  python3 .scripts/flash.py --speed 500      # 调整 SWD 时钟
"""

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# 目标芯片配置。换芯片（比如以后上 F411/F429）只改这一行。
TARGET = "target/stm32f1x.cfg"

# 构建产物可能的后缀，优先用 .elf（OpenOCD 能直接解析地址）
IMAGE_SUFFIXES = (".elf", ".hex", ".bin")

# 探针名 → (OpenOCD 接口配置, USB VID:PID)
# VID:PID 只用于失败后复位探针，换探针时用 lsusb 查一下即可。
PROBES = {
    "daplink": ("interface/cmsis-dap.cfg", "c251:f001"),
    "stlink": ("interface/stlink.cfg", "0483:3748"),
}


def find_image(build_dir: Path) -> Path:
    """在构建目录里找固件文件，优先 .elf。"""
    if not build_dir.is_dir():
        sys.exit(f"构建目录不存在: {build_dir}（先跑 build.py）")

    images = [p for p in build_dir.rglob("*") if p.suffix.lower() in IMAGE_SUFFIXES]
    elves = [p for p in images if p.suffix.lower() == ".elf"]
    found = sorted(elves or images)

    if not found:
        sys.exit(f"在 {build_dir} 下没找到固件")
    if len(found) > 1:
        names = "\n".join(f"  {p}" for p in found)
        sys.exit(f"找到多个固件，请用 -i 指定一个:\n{names}")
    return found[0]


def main() -> int:
    parser = argparse.ArgumentParser(description="烧录固件到目标板")
    parser.add_argument("-p", "--preset", default="Debug", help="构建目录名")
    parser.add_argument("--probe", choices=PROBES, default="daplink", help="调试探针")
    # 100 kHz 足够稳；这颗 DAPLink 走 HID，时钟调高也快不了
    parser.add_argument("--speed", type=int, default=100, help="SWD 时钟 kHz")
    parser.add_argument("-i", "--image", type=Path, help="手动指定固件文件")
    parser.add_argument("--no-retry", action="store_true", help="失败后不重试")
    args = parser.parse_args()

    # 没手动指定就自动找，这是"换项目不改脚本"的关键
    image = args.image or find_image(ROOT / "build" / args.preset)
    if not image.is_file():
        sys.exit(f"固件不存在: {image}")

    interface, usb_id = PROBES[args.probe]

    # 参数顺序不能乱：接口配置必须最先，后面的 transport/speed 才生效
    cmd = [
        "openocd",
        "-f", interface,                              # 1. 探针驱动
        "-c", "transport select swd",                 # 2. SWD（不是 JTAG）
        "-c", f"adapter speed {args.speed}",          # 3. SWD 时钟
        "-f", TARGET,                                 # 4. 目标芯片
        "-c", f'program "{image}" verify reset exit', # 5. 烧录 + 校验 + 复位
    ]

    print(f"固件: {image}")
    for attempt in (1, 2):
        if attempt > 1:
            print("重试...")
        print(">", " ".join(cmd))
        if subprocess.run(cmd).returncode == 0:
            print("烧录成功")
            return 0
        if attempt == 2 or args.no_retry:
            break
        # 探针烧录中卡死后，USB 端口复位通常能救回来，省得动手拔插
        print("失败，复位探针后重试")
        if shutil.which("usbreset"):
            subprocess.run(["usbreset", usb_id], check=False)
        time.sleep(2)

    sys.exit("烧录失败：检查板子供电 / 探针 / SWD 接线（SWDIO=PA13, SWCLK=PA14, GND）")


if __name__ == "__main__":
    raise SystemExit(main())
