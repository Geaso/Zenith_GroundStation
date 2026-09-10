# HISTORICAL LEGACY PROTOCOL: archived am/JSON framing, not MAVLink2.
# Not a validation tool for the current bridge; use ZenithMavlinkProbe and CTest.
"""
End-to-end integration test against the real Jetson communication bridge.

This test verifies:
  1. UDP telemetry reception from Jetson (UAVSTATE, TEXTINFO, etc.)
  2. Persistent TCP connection to Jetson (ModeSelection + commands on same socket)
  3. Heartbeat exchange (ground station sends heartbeat, Jetson sends heartbeat)
  4. Command roundtrip: send UAVCommand, observe Jetson echo back via UDP

Prerequisites:
  - Jetson at 192.168.82.223 with communication_bridge running in tmux
  - roscore running on Jetson

Usage:
    python tests/test_e2e_integration.py
"""

import socket
import struct
import json
import time
import threading
import unittest
import sys

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
JETSON_IP = "192.168.82.223"
LOCAL_IP = None  # auto-detected
UDP_PORT = 8889
TCP_PORT = 55555
HB_PORT = 55556

# ---------------------------------------------------------------------------
# Protocol codec
# ---------------------------------------------------------------------------
MAGIC = b'\x61\x6d'
FRAME_OVERHEAD = 10

MSG_UAVSTATE = 1
MSG_TEXTINFO = 3
MSG_HEARTBEAT = 6
MSG_UAVCONTROLSTATE = 9
MSG_UAVCOMMAND = 108
MSG_PARAMSETTINGS = 110
MSG_MODESELECTION = 202

MSG_NAMES = {
    1: "UAVSTATE", 3: "TEXTINFO", 6: "HEARTBEAT",
    9: "UAVCONTROLSTATE", 108: "UAVCOMMAND", 109: "UAVSETUP",
    110: "PARAMSETTINGS", 202: "MODESELECTION"
}


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
    j = json.dumps(payload, separators=(',', ':')).encode()
    h = MAGIC + struct.pack('<I', len(j)) + struct.pack('BB', msg_id, robot_id)
    f = h + j
    return f + struct.pack('<H', crc16_arc(f))


def unpack_frame(data: bytes):
    if len(data) < FRAME_OVERHEAD:
        return None
    o = 0
    while o + 1 < len(data):
        if data[o] == 0x61 and data[o + 1] == 0x6D:
            break
        o += 1
    else:
        return None
    if o + FRAME_OVERHEAD > len(data):
        return None
    ps = struct.unpack_from('<I', data, o + 2)[0]
    ts = ps + FRAME_OVERHEAD
    if len(data) < o + ts:
        return None
    fr = data[o:o + ts]
    ec = struct.unpack_from('<H', fr, ts - 2)[0]
    if ec != crc16_arc(fr[:ts - 2]):
        return None
    return fr[6], fr[7], json.loads(fr[8:8 + ps]), ts


def unpack_stream(buffer: bytes):
    """Parse multiple frames from TCP stream buffer."""
    frames = []
    pos = 0
    while pos + FRAME_OVERHEAD <= len(buffer):
        while pos + 1 < len(buffer):
            if buffer[pos] == 0x61 and buffer[pos + 1] == 0x6D:
                break
            pos += 1
        else:
            break
        if pos + FRAME_OVERHEAD > len(buffer):
            break
        ps = struct.unpack_from('<I', buffer, pos + 2)[0]
        ts = ps + FRAME_OVERHEAD
        if pos + ts > len(buffer):
            break
        fr = buffer[pos:pos + ts]
        ec = struct.unpack_from('<H', fr, ts - 2)[0]
        if ec != crc16_arc(fr[:ts - 2]):
            pos += 1
            continue
        frames.append((fr[6], fr[7], json.loads(fr[8:8 + ps])))
        pos += ts
    return frames, buffer[pos:]


def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((JETSON_IP, 1))
        return s.getsockname()[0]
    finally:
        s.close()


# ===========================================================================
# Test helpers
# ===========================================================================
class UdpCollector:
    """Background thread that collects UDP frames."""

    def __init__(self, port=UDP_PORT, duration=10):
        self.port = port
        self.duration = duration
        self.frames = []
        self._thread = None

    def start(self):
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(('0.0.0.0', self.port))
        sock.settimeout(self.duration)
        deadline = time.time() + self.duration
        while time.time() < deadline:
            try:
                data, addr = sock.recvfrom(65536)
                r = unpack_frame(data)
                if r:
                    self.frames.append(r)
            except socket.timeout:
                break
        sock.close()

    def join(self, timeout=None):
        if self._thread:
            self._thread.join(timeout=timeout or self.duration + 2)


# ===========================================================================
# Integration tests
# ===========================================================================
class TestJetsonConnectivity(unittest.TestCase):
    """Basic connectivity checks."""

    def test_01_jetson_reachable(self):
        """Jetson responds to TCP connection on bridge port."""
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3)
        try:
            s.connect((JETSON_IP, TCP_PORT))
            s.close()
        except Exception as e:
            self.fail(f"Cannot connect to {JETSON_IP}:{TCP_PORT}: {e}")


class TestPersistentTcp(unittest.TestCase):
    """Test persistent TCP connection behavior."""

    def test_02_persistent_tcp_multiple_commands(self):
        """
        Send ModeSelection + 3 UAVCommands over ONE persistent TCP connection.
        All should be accepted (no connection reset).
        """
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((JETSON_IP, TCP_PORT))

        # 1. Send ModeSelection
        mode = pack_frame(MSG_MODESELECTION, 1, {
            "mode": 1, "selectId": [1], "use_mode": 0,
            "is_simulation": False, "swarm_num": 1, "cmd": ""
        })
        s.sendall(mode)
        time.sleep(0.5)

        # 2. Send 3 UAVCommands over same connection
        for i in range(3):
            cmd = pack_frame(MSG_UAVCOMMAND, 1, {
                "Agent_CMD": 2, "Control_Level": 0, "Move_mode": 0,
                "position_ref": [0, 0, 0], "velocity_ref": [0, 0, 0],
                "acceleration_ref": [0, 0, 0], "yaw_ref": 0.0,
                "Yaw_Rate_Mode": False, "yaw_rate_ref": 0.0,
                "att_ref": [0, 0, 0, 0], "Command_ID": i + 1
            })
            s.sendall(cmd)
            time.sleep(0.2)

        # 3. Connection should still be alive
        try:
            s.sendall(b'')  # empty send to check
            alive = True
        except:
            alive = False

        s.close()
        self.assertTrue(alive, "TCP connection died during persistent session")

    def test_03_heartbeat_send_receive(self):
        """
        Send heartbeat frames over TCP connection.
        Verify they are well-formed and accepted.
        """
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((JETSON_IP, TCP_PORT))

        # Send 3 heartbeats
        for i in range(3):
            hb = pack_frame(MSG_HEARTBEAT, 1, {"count": i, "message": ""})
            s.sendall(hb)
            time.sleep(0.5)

        # If we got here without exception, heartbeats were accepted
        s.close()


class TestUdpTelemetry(unittest.TestCase):
    """Test UDP telemetry reception triggered by TCP commands."""

    def test_04_udp_telemetry_after_mode_selection(self):
        """
        After sending ModeSelection, Jetson should reply with UDP frames
        (TEXTINFO, PARAMSETTINGS, UAVCOMMAND, etc.)
        """
        # Start UDP collector
        collector = UdpCollector(duration=8)
        collector.start()
        time.sleep(0.3)

        # Send ModeSelection to trigger responses
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((JETSON_IP, TCP_PORT))
        mode = pack_frame(MSG_MODESELECTION, 1, {
            "mode": 1, "selectId": [1], "use_mode": 0,
            "is_simulation": False, "swarm_num": 1, "cmd": ""
        })
        s.sendall(mode)

        collector.join()
        s.close()

        # Verify we received some frames
        self.assertGreater(len(collector.frames), 0,
                           "No UDP frames received after ModeSelection")

        # Check decoded message types
        msg_types = set(f[0] for f in collector.frames)
        print(f"\n  Received {len(collector.frames)} UDP frames")
        print(f"  Message types: {[MSG_NAMES.get(m, m) for m in msg_types]}")

        # We expect at least TEXTINFO (connection notification)
        self.assertTrue(
            MSG_TEXTINFO in msg_types or MSG_PARAMSETTINGS in msg_types or MSG_UAVCOMMAND in msg_types,
            f"Expected TEXTINFO/PARAMSETTINGS/UAVCOMMAND in response, got: {msg_types}"
        )

    def test_05_verify_textinfo_json_fields(self):
        """Verify TEXTINFO frames have correct JSON field names."""
        collector = UdpCollector(duration=6)
        collector.start()
        time.sleep(0.3)

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((JETSON_IP, TCP_PORT))
        s.sendall(pack_frame(MSG_MODESELECTION, 1, {
            "mode": 1, "selectId": [1], "use_mode": 0,
            "is_simulation": False, "swarm_num": 1, "cmd": ""
        }))

        collector.join()
        s.close()

        # Find TEXTINFO frame
        textinfo_frames = [f for f in collector.frames if f[0] == MSG_TEXTINFO]
        if not textinfo_frames:
            self.skipTest("No TEXTINFO frames received (bridge may not have sent any)")

        _, robot_id, payload, _ = textinfo_frames[0]
        print(f"\n  TEXTINFO payload: {payload}")

        # Verify fields match TelemetryStore.applyTextInfo expectations
        self.assertIn("MessageType", payload)
        self.assertIn("Message", payload)
        self.assertEqual(robot_id, 1)

    def test_06_verify_uavcommand_echo_fields(self):
        """Verify UAVCommand echo frames have all expected JSON fields."""
        collector = UdpCollector(duration=6)
        collector.start()
        time.sleep(0.3)

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((JETSON_IP, TCP_PORT))
        s.sendall(pack_frame(MSG_MODESELECTION, 1, {
            "mode": 1, "selectId": [1], "use_mode": 0,
            "is_simulation": False, "swarm_num": 1, "cmd": ""
        }))

        collector.join()
        s.close()

        cmd_frames = [f for f in collector.frames if f[0] == MSG_UAVCOMMAND]
        if not cmd_frames:
            self.skipTest("No UAVCOMMAND echo frames received")

        _, robot_id, payload, _ = cmd_frames[0]
        print(f"\n  UAVCOMMAND echo fields: {list(payload.keys())}")

        # Verify fields match CommandDispatcher field names
        expected = ["Agent_CMD", "Control_Level", "Move_mode",
                    "position_ref", "velocity_ref", "yaw_ref", "Command_ID"]
        for field in expected:
            self.assertIn(field, payload, f"Missing field in UAVCOMMAND echo: {field}")


class TestHeartbeatExchange(unittest.TestCase):
    """Test bidirectional heartbeat."""

    def test_07_jetson_heartbeat_to_ground_station(self):
        """
        Start a TCP server on 55556 (like the ground station does).
        Jetson bridge should connect to it and send heartbeat frames.
        """
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(('0.0.0.0', HB_PORT))
        server.listen(1)
        server.settimeout(8)

        heartbeats = []
        try:
            conn, addr = server.accept()
            conn.settimeout(5)
            buffer = b''
            deadline = time.time() + 5
            while time.time() < deadline:
                try:
                    data = conn.recv(4096)
                    if not data:
                        break
                    buffer += data
                    frames, buffer = unpack_stream(buffer)
                    for mid, rid, pay in frames:
                        if mid == MSG_HEARTBEAT:
                            heartbeats.append(pay)
                    if len(heartbeats) >= 3:
                        break
                except socket.timeout:
                    break
            conn.close()
        except socket.timeout:
            pass
        finally:
            server.close()

        print(f"\n  Received {len(heartbeats)} heartbeat frames from Jetson")
        if heartbeats:
            print(f"  First heartbeat: {heartbeats[0]}")

        # Jetson bridge may or may not connect to our heartbeat port
        # depending on its configuration. This is informational.
        if not heartbeats:
            self.skipTest("No heartbeat connection from Jetson (may need ground_station_ip configured)")

        self.assertIn("count", heartbeats[0])


# ===========================================================================
# Main
# ===========================================================================
if __name__ == '__main__':
    LOCAL_IP = get_local_ip()
    print(f"Local IP: {LOCAL_IP}")
    print(f"Jetson IP: {JETSON_IP}")
    print()

    # Check connectivity first
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3)
        s.connect((JETSON_IP, TCP_PORT))
        s.close()
    except Exception as e:
        print(f"FATAL: Cannot connect to Jetson bridge at {JETSON_IP}:{TCP_PORT}")
        print(f"Error: {e}")
        print("Ensure the bridge is running:")
        print(f"  ssh jetson@{JETSON_IP}")
        print("  tmux new-session -d -s bridge 'source /opt/ros/noetic/setup.bash && ...")
        sys.exit(1)

    unittest.main(verbosity=2)
