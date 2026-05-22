"""
Verify that UAVControlState (msgId=9) from Jetson bridge now includes
the new state machine fields: exec_state, mission_mode, active_command_source,
pending_request, request_active.

Usage:
  1. Start bridge on Jetson (any of the Zenith_*.sh scripts, or standalone bridge)
  2. Run this script on Windows: python test_control_state_fields.py
  3. Script listens on UDP 8889, decodes UAVControlState frames,
     checks for new fields, reports PASS/FAIL.
"""

import socket
import struct
import json
import sys
import time


MAGIC = b'\x61\x6d'
FRAME_OVERHEAD = 10
MSG_UAVCONTROLSTATE = 9

REQUIRED_NEW_FIELDS = [
    "exec_state",
    "mission_mode",
    "active_command_source",
    "pending_request",
    "request_active",
]

REQUIRED_OLD_FIELDS = [
    "uav_id",
    "control_state",
    "pos_controller",
    "failsafe",
]


def crc16_arc(data: bytes) -> int:
    crc = 0x0000
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def try_decode_frame(data: bytes):
    if len(data) < FRAME_OVERHEAD:
        return None
    if data[0:2] != MAGIC:
        return None

    payload_size = struct.unpack_from('<I', data, 2)[0]
    total_size = payload_size + FRAME_OVERHEAD
    if len(data) < total_size:
        return None

    msg_id = data[6]
    robot_id = data[7]
    json_bytes = data[8:8 + payload_size]

    # Verify CRC
    frame_no_crc = data[:8 + payload_size]
    expected_crc = crc16_arc(frame_no_crc)
    actual_crc = struct.unpack_from('<H', data, 8 + payload_size)[0]
    if expected_crc != actual_crc:
        return None

    try:
        payload = json.loads(json_bytes.decode('utf-8'))
    except (json.JSONDecodeError, UnicodeDecodeError):
        return None

    return msg_id, robot_id, payload


def main():
    timeout_sec = 30
    print(f"Listening on UDP 0.0.0.0:8889 for UAVControlState (msgId={MSG_UAVCONTROLSTATE})...")
    print(f"Timeout: {timeout_sec}s\n")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', 8889))
    sock.settimeout(timeout_sec)

    control_state_count = 0
    other_msg_count = 0
    start = time.time()

    try:
        while True:
            try:
                data, addr = sock.recvfrom(65536)
            except socket.timeout:
                print(f"\nTIMEOUT: No UAVControlState received in {timeout_sec}s.")
                print(f"  Other messages received: {other_msg_count}")
                print("FAIL - UAVControlState not received")
                return 1

            result = try_decode_frame(data)
            if result is None:
                continue

            msg_id, robot_id, payload = result

            if msg_id != MSG_UAVCONTROLSTATE:
                other_msg_count += 1
                elapsed = time.time() - start
                if other_msg_count <= 3 or other_msg_count % 20 == 0:
                    print(f"  [{elapsed:.1f}s] Received msgId={msg_id} from {addr} (#{other_msg_count})")
                continue

            control_state_count += 1
            elapsed = time.time() - start
            print(f"\n[{elapsed:.1f}s] === UAVControlState #{control_state_count} from {addr} ===")
            print(f"  Full payload: {json.dumps(payload, indent=2)}")

            # Check old fields
            missing_old = [f for f in REQUIRED_OLD_FIELDS if f not in payload]
            if missing_old:
                print(f"\n  FAIL - Missing old fields: {missing_old}")
                return 1
            print(f"\n  Old fields present: {REQUIRED_OLD_FIELDS}")

            # Check new fields
            missing_new = [f for f in REQUIRED_NEW_FIELDS if f not in payload]
            if missing_new:
                print(f"  FAIL - Missing new fields: {missing_new}")
                return 1

            print(f"  New fields present: {REQUIRED_NEW_FIELDS}")
            print(f"\n  exec_state={payload['exec_state']}")
            print(f"  mission_mode={payload['mission_mode']}")
            print(f"  active_command_source={payload['active_command_source']}")
            print(f"  pending_request={payload['pending_request']}")
            print(f"  request_active={payload['request_active']}")

            print(f"\nPASS - UAVControlState contains all {len(REQUIRED_OLD_FIELDS) + len(REQUIRED_NEW_FIELDS)} fields")
            return 0

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 1
    finally:
        sock.close()


if __name__ == '__main__':
    sys.exit(main())
