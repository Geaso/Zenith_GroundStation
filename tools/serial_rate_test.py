#!/usr/bin/env python3
"""
Zenith 串口帧率 & 带宽测试工具
从地面站端(Windows)监听数传接收到的 Zenith 帧，统计各 msg_id 的帧率和字节吞吐量。
用法: python serial_rate_test.py [COM端口] [波特率] [测试时长秒]
"""

import sys
import time
import struct
from collections import defaultdict

try:
    import serial
except ImportError:
    print("需要 pyserial: pip install pyserial")
    sys.exit(1)

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM5"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 921600
DURATION = int(sys.argv[3]) if len(sys.argv) > 3 else 15

MAGIC = b"\x61\x6d"
HEADER_SIZE = 8   # magic(2) + size(4) + msg_id(1) + robot_id(1)
CRC_SIZE = 2

MSG_NAMES = {
    1: "UAVSTATE",
    2: "MODESELECTION",
    3: "TEXTINFO",
    4: "UAVCOMMAND",
    5: "MULTIMODESELECTION",
    6: "HEARTBEAT",
    7: "PARAMCONFIG",
    8: "PARAMSETTINGS",
    9: "UAVCONTROLSTATE",
}

def crc16_arc(data: bytes) -> int:
    crc = 0x0000
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

def main():
    print(f"=== Zenith 串口帧率测试 ===")
    print(f"端口: {PORT}  波特率: {BAUD}  测试时长: {DURATION}s")
    print()

    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    ser.reset_input_buffer()

    buf = bytearray()
    frame_counts = defaultdict(int)
    frame_bytes = defaultdict(int)
    total_bytes = 0
    crc_errors = 0
    start = time.time()
    last_print = start

    print(f"{'时间':>6s}  {'UAVSTATE':>10s}  {'CTRLSTATE':>10s}  {'HEARTBEAT':>10s}  {'TEXTINFO':>10s}  {'其他':>6s}  {'总帧':>6s}  {'带宽KB/s':>9s}  {'CRC错':>6s}")
    print("-" * 90)

    try:
        while True:
            elapsed = time.time() - start
            if elapsed >= DURATION:
                break

            chunk = ser.read(4096)
            if chunk:
                buf.extend(chunk)
                total_bytes += len(chunk)

            # Parse frames from buffer
            while len(buf) >= HEADER_SIZE + CRC_SIZE:
                # Find magic
                idx = buf.find(MAGIC)
                if idx < 0:
                    buf.clear()
                    break
                if idx > 0:
                    buf = buf[idx:]

                if len(buf) < HEADER_SIZE:
                    break

                payload_size = struct.unpack_from("<I", buf, 2)[0]
                total_size = HEADER_SIZE + payload_size + CRC_SIZE

                if len(buf) < total_size:
                    break  # Wait for more data

                frame = bytes(buf[:total_size])
                buf = buf[total_size:]

                # CRC check
                expected_crc = struct.unpack_from("<H", frame, total_size - 2)[0]
                actual_crc = crc16_arc(frame[:total_size - 2])
                if expected_crc != actual_crc:
                    crc_errors += 1
                    continue

                msg_id = frame[6]
                frame_counts[msg_id] += 1
                frame_bytes[msg_id] += total_size

            # Print stats every 2 seconds
            now = time.time()
            if now - last_print >= 2.0:
                dt = now - start
                total_frames = sum(frame_counts.values())
                bw = total_bytes / dt / 1024
                print(f"{dt:6.1f}  "
                      f"{frame_counts.get(1,0):10d}  "
                      f"{frame_counts.get(9,0):10d}  "
                      f"{frame_counts.get(6,0):10d}  "
                      f"{frame_counts.get(3,0):10d}  "
                      f"{sum(v for k,v in frame_counts.items() if k not in (1,9,6,3)):6d}  "
                      f"{total_frames:6d}  "
                      f"{bw:9.2f}  "
                      f"{crc_errors:6d}")
                last_print = now

    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

    # Final summary
    dt = time.time() - start
    total_frames = sum(frame_counts.values())
    print()
    print("=" * 90)
    print(f"测试时长: {dt:.1f}s  总接收字节: {total_bytes}  总有效帧: {total_frames}  CRC错误: {crc_errors}")
    print()
    print(f"{'MsgID':>6s}  {'名称':>20s}  {'帧数':>8s}  {'帧率Hz':>8s}  {'平均帧大小B':>12s}  {'带宽B/s':>10s}")
    print("-" * 70)
    for msg_id in sorted(frame_counts.keys()):
        name = MSG_NAMES.get(msg_id, f"UNKNOWN_{msg_id}")
        count = frame_counts[msg_id]
        rate = count / dt if dt > 0 else 0
        avg_size = frame_bytes[msg_id] / count if count > 0 else 0
        bw = frame_bytes[msg_id] / dt if dt > 0 else 0
        print(f"{msg_id:6d}  {name:>20s}  {count:8d}  {rate:8.2f}  {avg_size:12.1f}  {bw:10.1f}")

    total_bw = sum(frame_bytes.values()) / dt if dt > 0 else 0
    print(f"{'':>6s}  {'合计':>20s}  {total_frames:8d}  {total_frames/dt if dt > 0 else 0:8.2f}  {'':>12s}  {total_bw:10.1f}")
    print()
    print(f"原始串口接收带宽: {total_bytes/dt/1024:.2f} KB/s")

if __name__ == "__main__":
    main()
