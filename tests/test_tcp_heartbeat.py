# HISTORICAL LEGACY PROTOCOL: archived am/JSON framing, not MAVLink2.
# Not a validation tool for the current bridge; use ZenithMavlinkProbe and CTest.
"""
Tests for persistent TCP connection and heartbeat behavior.

This module provides:
  1. MockJetsonBridge - simulates the Jetson communication_bridge TCP server
  2. Tests that verify the expected behavior of the ground station protocol client

Expected behaviors being tested:
  - Ground station maintains ONE persistent TCP connection (no connect-per-message)
  - Ground station sends heartbeat frames periodically (1/sec)
  - Ground station reconnects automatically after TCP disconnect
  - Multiple commands sent over the same persistent connection
  - Heartbeat frames are well-formed with incrementing count

Usage:
    # Unit tests (no ground station needed, tests mock internals):
    python tests/test_tcp_heartbeat.py

    # Interactive mock server (for manual testing with actual ground station):
    python tests/test_tcp_heartbeat.py --serve
"""

import socket
import struct
import json
import time
import threading
import sys
import unittest
from typing import List, Optional, Tuple

# ---------------------------------------------------------------------------
# Protocol codec (shared)
# ---------------------------------------------------------------------------
MAGIC = b'\x61\x6d'
FRAME_OVERHEAD = 10
MSG_HEARTBEAT = 6
MSG_UAVSTATE = 1
MSG_UAVCONTROLSTATE = 9
MSG_TEXTINFO = 3
MSG_MODESELECTION = 202
MSG_UAVCOMMAND = 108

MSG_NAMES = {
    1: "UAVSTATE", 3: "TEXTINFO", 6: "HEARTBEAT",
    9: "UAVCONTROLSTATE", 108: "UAVCOMMAND", 202: "MODESELECTION"
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


def unpack_frames_from_stream(buffer: bytes) -> Tuple[list, bytes]:
    """Parse as many frames as possible from a byte buffer.
    Returns (list_of_(msgId, robotId, payload), remaining_buffer)."""
    frames = []
    pos = 0
    while pos + FRAME_OVERHEAD <= len(buffer):
        # find magic
        while pos + 1 < len(buffer):
            if buffer[pos] == 0x61 and buffer[pos + 1] == 0x6D:
                break
            pos += 1
        else:
            break

        if pos + FRAME_OVERHEAD > len(buffer):
            break

        payload_size = struct.unpack_from('<I', buffer, pos + 2)[0]
        total_size = payload_size + FRAME_OVERHEAD
        if pos + total_size > len(buffer):
            break  # incomplete frame

        frame = buffer[pos:pos + total_size]
        expected_crc = struct.unpack_from('<H', frame, total_size - 2)[0]
        actual_crc = crc16_arc(frame[:total_size - 2])
        if expected_crc != actual_crc:
            pos += 1  # skip bad byte
            continue

        msg_id = frame[6]
        robot_id = frame[7]
        payload = json.loads(frame[8:8 + payload_size])
        frames.append((msg_id, robot_id, payload))
        pos += total_size

    return frames, buffer[pos:]


# ===========================================================================
# MockJetsonBridge - simulates Jetson's communication_bridge behavior
# ===========================================================================
class MockJetsonBridge:
    """
    Simulates Jetson's communication bridge for testing the ground station.

    TCP server on port 55555: accepts commands, records received frames.
    UDP sender on port 8889: periodically sends fake UAVState.
    TCP heartbeat client: connects to ground station's 55556 and sends heartbeat.
    """

    def __init__(self, tcp_port=55555, udp_port=8889, hb_port=55556, robot_id=1):
        self.tcp_port = tcp_port
        self.udp_port = udp_port
        self.hb_port = hb_port
        self.robot_id = robot_id
        self._running = False
        self._tcp_server: Optional[socket.socket] = None
        self._received_frames: List[Tuple[int, int, dict]] = []
        self._tcp_connections: List[socket.socket] = []
        self._connection_count = 0
        self._threads: List[threading.Thread] = []
        self._lock = threading.Lock()

    @property
    def received_frames(self):
        with self._lock:
            return list(self._received_frames)

    @property
    def connection_count(self):
        """Number of TCP connections accepted (should be 1 for persistent)."""
        with self._lock:
            return self._connection_count

    def heartbeat_frames_received(self) -> List[dict]:
        """Return only heartbeat frames received."""
        with self._lock:
            return [pay for mid, rid, pay in self._received_frames if mid == MSG_HEARTBEAT]

    def start(self, ground_station_ip: str = "127.0.0.1"):
        self._running = True
        self._ground_station_ip = ground_station_ip

        # TCP command server
        t1 = threading.Thread(target=self._tcp_server_loop, daemon=True)
        t1.start()
        self._threads.append(t1)

        # UDP telemetry broadcaster
        t2 = threading.Thread(target=self._udp_broadcast_loop, daemon=True)
        t2.start()
        self._threads.append(t2)

        # TCP heartbeat client (connects to ground station's 55556)
        t3 = threading.Thread(target=self._heartbeat_client_loop, daemon=True)
        t3.start()
        self._threads.append(t3)

        time.sleep(0.2)

    def stop(self):
        self._running = False
        if self._tcp_server:
            try:
                self._tcp_server.close()
            except:
                pass
        for conn in self._tcp_connections:
            try:
                conn.close()
            except:
                pass
        for t in self._threads:
            t.join(timeout=2)

    def force_disconnect_tcp(self):
        """Forcibly close all TCP connections to test reconnect behavior."""
        with self._lock:
            for conn in self._tcp_connections:
                try:
                    conn.close()
                except:
                    pass
            self._tcp_connections.clear()

    def _tcp_server_loop(self):
        self._tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._tcp_server.bind(('0.0.0.0', self.tcp_port))
        self._tcp_server.listen(5)
        self._tcp_server.settimeout(1.0)

        while self._running:
            try:
                conn, addr = self._tcp_server.accept()
                with self._lock:
                    self._connection_count += 1
                    self._tcp_connections.append(conn)
                t = threading.Thread(target=self._handle_tcp_client, args=(conn, addr), daemon=True)
                t.start()
            except socket.timeout:
                continue
            except OSError:
                break

    def _handle_tcp_client(self, conn: socket.socket, addr):
        buffer = b''
        conn.settimeout(1.0)
        while self._running:
            try:
                data = conn.recv(4096)
                if not data:
                    break
                buffer += data
                frames, buffer = unpack_frames_from_stream(buffer)
                with self._lock:
                    for f in frames:
                        self._received_frames.append(f)
                        name = MSG_NAMES.get(f[0], f"MSG_{f[0]}")
            except socket.timeout:
                continue
            except (ConnectionResetError, OSError):
                break
        try:
            conn.close()
        except:
            pass

    def _udp_broadcast_loop(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        hb_count = 0
        while self._running:
            # Send fake UAVState
            state = {
                "uav_id": self.robot_id,
                "connected": True,
                "armed": False,
                "odom_valid": True,
                "location_source": 0,
                "mode": "POSCTL",
                "gps_status": 0,
                "gps_num": 0,
                "position": [0.0, 0.0, 0.0],
                "velocity": [0.0, 0.0, 0.0],
                "attitude": [0.0, 0.0, 0.0],
                "attitude_q": {"x": 0, "y": 0, "z": 0, "w": 1},
                "attitude_rate": [0.0, 0.0, 0.0],
                "battery_state": 16.2,
                "battery_percetage": 0.85,
                "latitude": 0.0,
                "longitude": 0.0,
                "altitude": 0.0
            }
            frame = pack_frame(MSG_UAVSTATE, self.robot_id, state)
            try:
                sock.sendto(frame, (self._ground_station_ip, self.udp_port))
            except:
                pass
            time.sleep(0.1)  # 10 Hz
        sock.close()

    def _heartbeat_client_loop(self):
        """Simulates the Jetson bridge connecting to ground station heartbeat port."""
        count = 0
        while self._running:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(2)
                sock.connect((self._ground_station_ip, self.hb_port))
                while self._running:
                    hb = pack_frame(MSG_HEARTBEAT, self.robot_id, {"count": count, "message": ""})
                    sock.sendall(hb)
                    count += 1
                    time.sleep(1)
                sock.close()
            except:
                time.sleep(1)


# ===========================================================================
# Unit tests: verify mock itself works correctly
# ===========================================================================
class TestMockJetsonBridge(unittest.TestCase):
    """Test the mock server infrastructure itself."""

    def test_mock_starts_and_accepts_tcp(self):
        mock = MockJetsonBridge(tcp_port=55755, udp_port=18889, hb_port=55756)
        mock.start("127.0.0.1")
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2)
            s.connect(('127.0.0.1', 55755))
            # Send a ModeSelection
            frame = pack_frame(MSG_MODESELECTION, 1, {"mode": 1, "selectId": [1], "use_mode": 0, "is_simulation": False, "swarm_num": 1, "cmd": ""})
            s.sendall(frame)
            time.sleep(0.3)
            s.close()

            self.assertGreaterEqual(mock.connection_count, 1)
            received = mock.received_frames
            self.assertGreaterEqual(len(received), 1)
            self.assertEqual(received[0][0], MSG_MODESELECTION)
        finally:
            mock.stop()

    def test_mock_sends_udp_telemetry(self):
        mock = MockJetsonBridge(tcp_port=55757, udp_port=18890, hb_port=55758)
        mock.start("127.0.0.1")
        try:
            us = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            us.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            us.bind(('0.0.0.0', 18890))
            us.settimeout(3)
            data, _ = us.recvfrom(65536)
            us.close()
            frames, _ = unpack_frames_from_stream(data)
            self.assertEqual(len(frames), 1)
            self.assertEqual(frames[0][0], MSG_UAVSTATE)
            self.assertTrue(frames[0][2]["connected"])
        finally:
            mock.stop()

    def test_mock_counts_connections(self):
        """Verify connection counter distinguishes persistent vs per-message."""
        mock = MockJetsonBridge(tcp_port=55759, udp_port=18891, hb_port=55760)
        mock.start("127.0.0.1")
        try:
            # Simulate per-message pattern (BAD): 3 connections for 3 messages
            for i in range(3):
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(2)
                s.connect(('127.0.0.1', 55759))
                s.sendall(pack_frame(MSG_UAVCOMMAND, 1, {"Agent_CMD": 2, "Command_ID": i}))
                time.sleep(0.1)
                s.close()
                time.sleep(0.1)
            time.sleep(0.3)
            self.assertEqual(mock.connection_count, 3, "Per-message pattern should create 3 connections")

            # Reset for persistent pattern test
            mock.stop()
            mock2 = MockJetsonBridge(tcp_port=55761, udp_port=18892, hb_port=55762)
            mock2.start("127.0.0.1")

            # Simulate persistent pattern (GOOD): 1 connection for 3 messages
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2)
            s.connect(('127.0.0.1', 55761))
            for i in range(3):
                s.sendall(pack_frame(MSG_UAVCOMMAND, 1, {"Agent_CMD": 2, "Command_ID": i}))
                time.sleep(0.1)
            time.sleep(0.3)
            s.close()
            self.assertEqual(mock2.connection_count, 1, "Persistent pattern should create 1 connection")
            self.assertEqual(len(mock2.received_frames), 3, "All 3 commands should be received")
            mock2.stop()
        finally:
            mock.stop()

    def test_multiple_frames_on_persistent_connection(self):
        mock = MockJetsonBridge(tcp_port=55763, udp_port=18893, hb_port=55764)
        mock.start("127.0.0.1")
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2)
            s.connect(('127.0.0.1', 55763))

            # Send 5 different commands over same connection
            for i in range(5):
                cmd = {
                    "Agent_CMD": 4,
                    "Control_Level": 0,
                    "Move_mode": 0,
                    "position_ref": [float(i), 0.0, 1.0],
                    "velocity_ref": [0.0, 0.0, 0.0],
                    "acceleration_ref": [0.0, 0.0, 0.0],
                    "yaw_ref": 0.0,
                    "Yaw_Rate_Mode": False,
                    "yaw_rate_ref": 0.0,
                    "att_ref": [0.0, 0.0, 0.0, 0.0],
                    "Command_ID": i + 1
                }
                s.sendall(pack_frame(MSG_UAVCOMMAND, 1, cmd))
                time.sleep(0.05)

            time.sleep(0.5)
            s.close()

            self.assertEqual(mock.connection_count, 1)
            self.assertEqual(len(mock.received_frames), 5)
            # Verify command IDs are sequential
            for i, (mid, rid, pay) in enumerate(mock.received_frames):
                self.assertEqual(mid, MSG_UAVCOMMAND)
                self.assertEqual(pay["Command_ID"], i + 1)
        finally:
            mock.stop()

    def test_heartbeat_frame_structure(self):
        """Verify heartbeat frame contains required fields."""
        hb_payload = {"count": 42, "message": ""}
        frame = pack_frame(MSG_HEARTBEAT, 1, hb_payload)
        frames, _ = unpack_frames_from_stream(frame)
        self.assertEqual(len(frames), 1)
        mid, rid, pay = frames[0]
        self.assertEqual(mid, MSG_HEARTBEAT)
        self.assertIn("count", pay)
        self.assertIn("message", pay)
        self.assertEqual(pay["count"], 42)


# ===========================================================================
# Interactive mock server mode
# ===========================================================================
def serve_mode():
    """Run as interactive mock Jetson for manual ground station testing."""
    print("=" * 60)
    print("Mock Jetson Bridge Server")
    print("=" * 60)
    print("TCP command server on :55555")
    print("UDP telemetry on :8889 -> 127.0.0.1")
    print("Heartbeat client -> 127.0.0.1:55556")
    print("Press Ctrl+C to stop\n")

    mock = MockJetsonBridge()
    mock.start("127.0.0.1")
    try:
        while True:
            time.sleep(2)
            frames = mock.received_frames
            if frames:
                latest = frames[-1]
                name = MSG_NAMES.get(latest[0], f"MSG_{latest[0]}")
                print(f"[{len(frames)} frames] Latest: {name} | TCP connections: {mock.connection_count}")
    except KeyboardInterrupt:
        print("\nStopping...")
        mock.stop()


if __name__ == '__main__':
    if '--serve' in sys.argv:
        serve_mode()
    else:
        unittest.main(verbosity=2)
