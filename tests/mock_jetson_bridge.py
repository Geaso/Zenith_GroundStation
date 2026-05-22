"""
Mock Jetson Bridge — simulates the Jetson communication bridge for ground station testing.

Sends periodic UDP telemetry (UAVSTATE + UAVCONTROLSTATE) to localhost:8889,
and optionally listens on TCP 55555 for commands.

Usage:
  python mock_jetson_bridge.py [--scenario cycle]

Scenarios:
  default   — static HOVER state, sends every 100ms
  cycle     — cycles through all exec_state/mission_mode combinations every 2s
  arm_seq   — simulates arm → takeoff → hover → land → disarm sequence
"""

import socket
import struct
import json
import time
import argparse
import math
import sys


MAGIC = b'\x61\x6d'
UDP_PORT = 8889
MSG_UAVSTATE = 1
MSG_UAVCONTROLSTATE = 9
MSG_HEARTBEAT = 6

EXEC_STATES = ["DISARMED", "STANDBY", "RC_CONTROL", "AUTO_HOLD", "AUTO_TRACK", "AUTO_LAND", "FAILSAFE"]
MISSION_MODES = ["MANUAL", "HOVER", "MOVE", "TRACK_TRAJ", "AUTO_EXPLORE", "RETURN_HOME", "LAND_PENDING", "EMERGENCY"]
COMMAND_SOURCES = ["NONE", "RC", "MISSION", "PLANNER", "FAILSAFE", "GROUND_STATION"]
PENDING_REQUESTS = ["NONE", "ENTER_INIT", "ENTER_RC", "ENTER_CMD", "HOVER", "LAND", "MANUAL_OVERRIDE"]


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


def encode_frame(msg_id: int, robot_id: int, payload: dict) -> bytes:
    json_bytes = json.dumps(payload, separators=(',', ':')).encode('utf-8')
    payload_size = len(json_bytes)
    header = MAGIC + struct.pack('<I', payload_size) + bytes([msg_id, robot_id])
    frame_no_crc = header + json_bytes
    crc = crc16_arc(frame_no_crc)
    return frame_no_crc + struct.pack('<H', crc)


def make_uav_state(t: float, armed: bool = True) -> dict:
    """Generate a UAVState payload with simulated circular motion."""
    radius = 3.0
    speed = 0.5
    angle = speed * t
    x = radius * math.cos(angle)
    y = radius * math.sin(angle)
    z = 1.5
    vx = -radius * speed * math.sin(angle)
    vy = radius * speed * math.cos(angle)
    yaw = math.atan2(vy, vx)

    return {
        "uav_id": 1,
        "connected": True,
        "armed": armed,
        "mode": "OFFBOARD",
        "location_source": 0,  # MOCAP
        "gps_status": 3,
        "battery_state": 23.4,
        "battery_percetage": 0.78,
        "altitude": z,
        "rel_alt": z,
        "range": z - 0.05,
        "position": [round(x, 4), round(y, 4), round(z, 4)],
        "velocity": [round(vx, 4), round(vy, 4), 0.0],
        "attitude": [0.01, -0.02, round(yaw, 4)],
        "attitude_q": [1, 0, 0, 0],
        "attitude_rate": [0, 0, 0],
        "latitude": 39.9042 + x * 0.00001,
        "longitude": 116.4074 + y * 0.00001,
    }


def make_control_state(exec_state=3, mission_mode=1, source=5, request=0, request_active=False) -> dict:
    return {
        "uav_id": 1,
        "control_state": 2,  # HOVER (legacy)
        "pos_controller": 0,  # PX4_ORIGIN
        "failsafe": (exec_state == 6),
        "exec_state": exec_state,
        "mission_mode": mission_mode,
        "active_command_source": source,
        "pending_request": request,
        "request_active": request_active,
    }


def scenario_default(sock, addr):
    """Static hover state."""
    t0 = time.time()
    hb_count = 0
    print("Scenario: default (static AUTO_HOLD / HOVER / GROUND_STATION)")
    print(f"Sending to {addr[0]}:{addr[1]} every 100ms. Ctrl+C to stop.\n")

    while True:
        t = time.time() - t0

        # UAVSTATE at ~10Hz
        uav = make_uav_state(t)
        sock.sendto(encode_frame(MSG_UAVSTATE, 1, uav), addr)

        # UAVCONTROLSTATE at ~10Hz
        ctrl = make_control_state(exec_state=3, mission_mode=1, source=5)
        sock.sendto(encode_frame(MSG_UAVCONTROLSTATE, 1, ctrl), addr)

        # Heartbeat at ~1Hz
        if int(t) > hb_count:
            hb_count = int(t)
            hb = {"count": hb_count, "message": "MockBridge OK"}
            sock.sendto(encode_frame(MSG_HEARTBEAT, 1, hb), addr)

        time.sleep(0.1)


def scenario_cycle(sock, addr):
    """Cycle through all exec_state and mission_mode combinations."""
    t0 = time.time()
    hb_count = 0
    print("Scenario: cycle (rotates exec_state every 2s, mission_mode every 0.8s)")
    print(f"Sending to {addr[0]}:{addr[1]}. Ctrl+C to stop.\n")

    while True:
        t = time.time() - t0
        exec_idx = int(t / 2.0) % len(EXEC_STATES)
        mode_idx = int(t / 0.8) % len(MISSION_MODES)
        src_idx = int(t / 3.0) % len(COMMAND_SOURCES)

        uav = make_uav_state(t, armed=(exec_idx > 0))
        sock.sendto(encode_frame(MSG_UAVSTATE, 1, uav), addr)

        ctrl = make_control_state(exec_state=exec_idx, mission_mode=mode_idx, source=src_idx)
        sock.sendto(encode_frame(MSG_UAVCONTROLSTATE, 1, ctrl), addr)

        if int(t) > hb_count:
            hb_count = int(t)
            hb = {"count": hb_count, "message": f"exec={EXEC_STATES[exec_idx]} mode={MISSION_MODES[mode_idx]}"}
            sock.sendto(encode_frame(MSG_HEARTBEAT, 1, hb), addr)
            print(f"  [{t:.0f}s] exec={EXEC_STATES[exec_idx]:12s} mode={MISSION_MODES[mode_idx]:14s} src={COMMAND_SOURCES[src_idx]}")

        time.sleep(0.1)


def scenario_arm_seq(sock, addr):
    """Simulates: DISARMED → STANDBY → RC_CONTROL → AUTO_HOLD → AUTO_LAND → DISARMED."""
    sequence = [
        (3.0, 0, 0, 0, "Disarmed, waiting..."),
        (3.0, 1, 0, 1, "Standby, RC connected"),
        (3.0, 2, 0, 1, "RC control active"),
        (2.0, 3, 1, 5, "Auto hold, hovering"),
        (4.0, 3, 2, 5, "Auto hold, moving to waypoint"),
        (3.0, 4, 3, 3, "Auto tracking trajectory"),
        (3.0, 3, 5, 5, "Return home, auto hold"),
        (3.0, 5, 6, 5, "Auto landing"),
        (2.0, 0, 0, 0, "Disarmed, sequence complete"),
    ]
    t0 = time.time()
    hb_count = 0
    step = 0
    step_start = t0
    print("Scenario: arm_seq (arm → fly → land → disarm)")
    print(f"Sending to {addr[0]}:{addr[1]}. Ctrl+C to stop.\n")

    while True:
        t = time.time() - t0
        duration, exec_s, mode_s, src_s, desc = sequence[step]

        if time.time() - step_start > duration:
            step = (step + 1) % len(sequence)
            step_start = time.time()
            _, exec_s, mode_s, src_s, desc = sequence[step]
            print(f"  [{t:.0f}s] Step {step}: {desc}")

        armed = exec_s > 0
        uav = make_uav_state(t, armed=armed)
        sock.sendto(encode_frame(MSG_UAVSTATE, 1, uav), addr)

        ctrl = make_control_state(exec_state=exec_s, mission_mode=mode_s, source=src_s)
        sock.sendto(encode_frame(MSG_UAVCONTROLSTATE, 1, ctrl), addr)

        if int(t) > hb_count:
            hb_count = int(t)
            hb = {"count": hb_count, "message": desc}
            sock.sendto(encode_frame(MSG_HEARTBEAT, 1, hb), addr)

        time.sleep(0.1)


def main():
    parser = argparse.ArgumentParser(description="Mock Jetson Bridge for ground station testing")
    parser.add_argument("--scenario", choices=["default", "cycle", "arm_seq"], default="default")
    parser.add_argument("--host", default="127.0.0.1", help="Target host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=UDP_PORT, help="Target UDP port (default: 8889)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    addr = (args.host, args.port)

    print(f"=== Mock Jetson Bridge ===")
    print(f"Target: {addr[0]}:{addr[1]}\n")

    scenarios = {
        "default": scenario_default,
        "cycle": scenario_cycle,
        "arm_seq": scenario_arm_seq,
    }

    try:
        scenarios[args.scenario](sock, addr)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
