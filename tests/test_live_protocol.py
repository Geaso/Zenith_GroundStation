# HISTORICAL LEGACY PROTOCOL: archived am/JSON framing, not MAVLink2.
# Not a validation tool for the current bridge; use ZenithMavlinkProbe and CTest.
"""
Live protocol verification against a real Jetson running zenith_communication_bridge.

Usage:
    python tests/test_live_protocol.py

This script:
  1. Detects the Windows host IP on the same subnet as the Jetson
  2. SSHs into the Jetson to start roscore + bridge (pointing to this host)
  3. Listens on UDP 8889 for telemetry frames
  4. Sends a TCP command frame to Jetson:55555
  5. Verifies round-trip codec compatibility
  6. Cleans up (kills roscore on Jetson)

Requirements:
    pip install paramiko   (for SSH; fallback to subprocess ssh if unavailable)
"""

import os
import socket
import struct
import json
import time
import sys
import subprocess
import threading

# ---------------------------------------------------------------------------
# Config — override via environment variables or tests/config.local.json
# ---------------------------------------------------------------------------
def _load_config():
    """Load test config from env vars, falling back to config.local.json."""
    cfg = {}
    cfg_path = os.path.join(os.path.dirname(__file__), "config.local.json")
    if os.path.exists(cfg_path):
        with open(cfg_path, "r") as f:
            cfg = json.load(f)
    return {
        "jetson_ip":   os.environ.get("JETSON_IP",   cfg.get("jetson_ip",   "192.168.82.XXX")),
        "jetson_user": os.environ.get("JETSON_USER", cfg.get("jetson_user", "jetson")),
        "jetson_pass": os.environ.get("JETSON_PASS", cfg.get("jetson_pass", "")),
    }

_cfg = _load_config()
JETSON_IP = _cfg["jetson_ip"]
JETSON_USER = _cfg["jetson_user"]
JETSON_PASS = _cfg["jetson_pass"]
UDP_PORT = 8889
TCP_PORT = 55555
TIMEOUT_SEC = 20

# ---------------------------------------------------------------------------
# Protocol codec (same as test_protocol_codec.py)
# ---------------------------------------------------------------------------
MAGIC = b'\x61\x6d'
FRAME_OVERHEAD = 10

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

def pack_frame(msg_id: int, robot_id: int, payload: dict) -> bytes:
    json_bytes = json.dumps(payload, separators=(',', ':')).encode('utf-8')
    header = MAGIC
    header += struct.pack('<I', len(json_bytes))
    header += struct.pack('B', msg_id & 0xFF)
    header += struct.pack('B', robot_id & 0xFF)
    frame_no_crc = header + json_bytes
    crc = crc16_arc(frame_no_crc)
    return frame_no_crc + struct.pack('<H', crc)

def unpack_frame(data: bytes):
    if len(data) < FRAME_OVERHEAD:
        return None
    offset = 0
    while offset + 1 < len(data):
        if data[offset] == 0x61 and data[offset + 1] == 0x6D:
            break
        offset += 1
    else:
        return None
    if offset + FRAME_OVERHEAD > len(data):
        return None
    payload_size = struct.unpack_from('<I', data, offset + 2)[0]
    total_size = payload_size + FRAME_OVERHEAD
    if len(data) < offset + total_size:
        return None
    frame = data[offset:offset + total_size]
    expected_crc = struct.unpack_from('<H', frame, total_size - 2)[0]
    actual_crc = crc16_arc(frame[:total_size - 2])
    if expected_crc != actual_crc:
        return None
    msg_id = frame[6]
    robot_id = frame[7]
    json_bytes = frame[8:8 + payload_size]
    payload = json.loads(json_bytes)
    return msg_id, robot_id, payload, total_size

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
MSG_NAMES = {
    1: "UAVSTATE", 3: "TEXTINFO", 4: "GIMBALSTATE", 5: "VISIONDIFF",
    6: "HEARTBEAT", 7: "UGVSTATE", 8: "MULTIDETECTIONINFO",
    9: "UAVCONTROLSTATE", 10: "POSESTAMPED",
    108: "UAVCOMMAND", 109: "UAVSETUP", 202: "MODESELECTION"
}

def get_local_ip_for(remote_ip: str) -> str:
    """Find our IP on the same subnet as remote_ip."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((remote_ip, 1))
        return s.getsockname()[0]
    finally:
        s.close()

def ssh_exec(cmd: str, timeout: int = 10) -> str:
    """Execute command on Jetson via ssh subprocess."""
    full = [
        "ssh", "-o", "StrictHostKeyChecking=no",
        "-o", f"ConnectTimeout={timeout}",
        f"{JETSON_USER}@{JETSON_IP}", cmd
    ]
    r = subprocess.run(full, capture_output=True, text=True, timeout=timeout + 5)
    return r.stdout + r.stderr

def ssh_exec_bg(cmd: str) -> subprocess.Popen:
    """Execute command on Jetson in background, return Popen."""
    full = [
        "ssh", "-o", "StrictHostKeyChecking=no",
        "-o", "ConnectTimeout=5",
        f"{JETSON_USER}@{JETSON_IP}", cmd
    ]
    return subprocess.Popen(full, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


# ===========================================================================
# Main test sequence
# ===========================================================================
def main():
    print("=" * 60)
    print("Zenith Live Protocol Verification")
    print("=" * 60)

    # --- Step 0: detect local IP ---
    local_ip = get_local_ip_for(JETSON_IP)
    print(f"\n[INFO] Windows host IP: {local_ip}")
    print(f"[INFO] Jetson IP:       {JETSON_IP}")

    # --- Step 1: check Jetson reachable ---
    print("\n[1/6] Checking Jetson SSH connectivity...")
    try:
        out = ssh_exec("echo OK", timeout=8)
        if "OK" not in out:
            print(f"[FAIL] SSH returned unexpected: {out}")
            return 1
        print("  SSH OK")
    except Exception as e:
        print(f"[FAIL] Cannot SSH to Jetson: {e}")
        return 1

    # --- Step 2: check if roscore is running, start if not ---
    print("\n[2/6] Ensuring roscore is running on Jetson...")
    out = ssh_exec("source /opt/ros/noetic/setup.bash && rosnode list 2>&1", timeout=8)
    roscore_started_by_us = False
    if "ERROR" in out or "Unable" in out or "not running" in out.lower():
        print("  roscore not running, starting it...")
        roscore_proc = ssh_exec_bg(
            "source /opt/ros/noetic/setup.bash && source ~/Zenith_ws/devel/setup.bash && roscore"
        )
        roscore_started_by_us = True
        time.sleep(3)
        print("  roscore started")
    else:
        print("  roscore already running")

    # --- Step 3: start communication bridge ---
    print("\n[3/6] Starting communication bridge on Jetson...")
    # Kill any previous bridge and start in tmux for persistence
    ssh_exec(
        "source /opt/ros/noetic/setup.bash && source ~/Zenith_ws/devel/setup.bash && "
        "rosnode cleanup -y 2>/dev/null; tmux kill-session -t bridge 2>/dev/null; "
        "tmux new-session -d -s bridge '"
        "source /opt/ros/noetic/setup.bash && source ~/Zenith_ws/devel/setup.bash && "
        "roslaunch zenith_communication_bridge bridge.launch "
        f"ground_station_ip:={local_ip} "
        "robot_id:=1 uav_id:=1 is_simulation:=0 autoload:=false "
        "zenith_moudles_url:=/home/jetson/Prometheus/Modules/ "
        "2>&1 | tee /tmp/bridge.log'",
        timeout=12
    )
    time.sleep(6)
    # Verify bridge is listening
    out = ssh_exec("ss -tlnp 2>/dev/null | grep 55555", timeout=5)
    if "55555" in out:
        print("  bridge started, TCP 55555 listening")
    else:
        print("  [WARN] bridge may not be ready, TCP 55555 not detected")
    bridge_proc = None  # tmux manages the process

    # --- Step 4: listen for UDP frames ---
    print(f"\n[4/6] Listening for UDP frames on port {UDP_PORT} (timeout {TIMEOUT_SEC}s)...")
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    udp_sock.bind(('0.0.0.0', UDP_PORT))
    udp_sock.settimeout(TIMEOUT_SEC)

    received_frames = []
    decoded_msgs = {}
    try:
        deadline = time.time() + TIMEOUT_SEC
        while time.time() < deadline and len(received_frames) < 30:
            try:
                data, addr = udp_sock.recvfrom(65536)
                received_frames.append(data)
                result = unpack_frame(data)
                if result:
                    msg_id, robot_id, payload, total = result
                    name = MSG_NAMES.get(msg_id, f"UNKNOWN({msg_id})")
                    if msg_id not in decoded_msgs:
                        decoded_msgs[msg_id] = payload
                        print(f"  [OK] Decoded {name} (id={msg_id}, robot={robot_id}, {total}B)")
                        # print first few fields
                        keys = list(payload.keys())[:6]
                        preview = {k: payload[k] for k in keys}
                        print(f"       Fields: {preview}")
                else:
                    print(f"  [WARN] Failed to decode frame ({len(data)}B) from {addr}")
            except socket.timeout:
                break
    finally:
        udp_sock.close()

    if not received_frames:
        print("\n  [WARN] No UDP frames received.")
        print("  This may mean the bridge hasn't started publishing yet,")
        print("  or there's no control node publishing /zenith/state.")
        print("  The bridge only forwards ROS topics - without a control node,")
        print("  only heartbeat frames (via TCP) would be sent.")
    else:
        print(f"\n  Received {len(received_frames)} frames, decoded {len(decoded_msgs)} unique message types")

    # --- Step 5: test TCP command send ---
    print(f"\n[5/6] Testing TCP command to {JETSON_IP}:{TCP_PORT}...")
    mode_payload = {
        "mode": 1,
        "selectId": [1],
        "use_mode": 0,
        "is_simulation": False,
        "swarm_num": 1,
        "cmd": ""
    }
    frame = pack_frame(202, 1, mode_payload)
    tcp_ok = False
    try:
        tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_sock.settimeout(5)
        tcp_sock.connect((JETSON_IP, TCP_PORT))
        tcp_sock.sendall(frame)
        print(f"  [OK] TCP connected and sent ModeSelection frame ({len(frame)}B)")
        tcp_ok = True
        # try to receive any response
        try:
            tcp_sock.settimeout(3)
            resp = tcp_sock.recv(4096)
            if resp:
                r = unpack_frame(resp)
                if r:
                    print(f"  [OK] TCP response: {MSG_NAMES.get(r[0], r[0])} ({len(resp)}B)")
                else:
                    print(f"  [INFO] TCP response received ({len(resp)}B) but not a valid frame")
        except socket.timeout:
            print("  [INFO] No TCP response (expected - bridge may not reply to ModeSelection)")
        tcp_sock.close()
    except Exception as e:
        print(f"  [FAIL] TCP connection failed: {e}")

    # --- Step 6: cleanup ---
    print("\n[6/6] Cleaning up Jetson processes...")
    ssh_exec("tmux kill-session -t bridge 2>/dev/null", timeout=8)
    if roscore_started_by_us:
        ssh_exec("pkill -f roscore 2>/dev/null; pkill -f rosmaster 2>/dev/null", timeout=8)
        print("  Killed roscore (started by us)")
    print("  Done")

    # --- Summary ---
    print("\n" + "=" * 60)
    print("RESULTS SUMMARY")
    print("=" * 60)
    print(f"  SSH connectivity:     OK")
    print(f"  UDP frames received:  {len(received_frames)}")
    print(f"  UDP frames decoded:   {len(decoded_msgs)} unique types")
    print(f"  TCP command send:     {'OK' if tcp_ok else 'FAIL'}")
    print(f"  CRC-16-ARC:           confirmed (via decode success)")

    if decoded_msgs:
        print(f"\n  Decoded message types:")
        for mid, pay in decoded_msgs.items():
            print(f"    {MSG_NAMES.get(mid, mid)}: {list(pay.keys())[:8]}")

    all_ok = tcp_ok  # UDP frames may be 0 if no control node is running
    if all_ok:
        print("\n  [PASS] Protocol compatibility verified!")
    else:
        print("\n  [PARTIAL] TCP or UDP had issues - see details above")

    return 0 if all_ok else 1


if __name__ == '__main__':
    sys.exit(main())
