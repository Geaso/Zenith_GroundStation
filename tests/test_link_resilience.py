import json
import socket
import struct
import subprocess
import sys
import time
import unittest
from pathlib import Path


MAGIC = b"\x61\x6d"
FRAME_OVERHEAD = 10

MSG_TEXTINFO = 3
MSG_HEARTBEAT = 6
MSG_UAVCOMMAND = 108
MSG_MODESELECTION = 202

SCRIPT = Path(__file__).with_name("mock_link_bridge.py")


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
    encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    frame = MAGIC + struct.pack("<I", len(encoded)) + struct.pack("BB", msg_id, robot_id) + encoded
    return frame + struct.pack("<H", crc16_arc(frame))


def decode_frame(data: bytes):
    if len(data) < FRAME_OVERHEAD or data[:2] != MAGIC:
        return None
    payload_size = struct.unpack_from("<I", data, 2)[0]
    total = payload_size + FRAME_OVERHEAD
    if len(data) < total:
        return None
    frame = data[:total]
    if struct.unpack_from("<H", frame, total - 2)[0] != crc16_arc(frame[:-2]):
        return None
    payload = json.loads(frame[8:-2] or b"{}")
    return frame[6], frame[7], payload


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class UdpCollector:
    def __init__(self, port: int):
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", port))
        self.sock.settimeout(0.2)
        self.frames = []
        self.timestamps = []

    def collect(self, duration: float):
        deadline = time.time() + duration
        while time.time() < deadline:
            try:
                data, _ = self.sock.recvfrom(65535)
            except socket.timeout:
                continue
            frame = decode_frame(data)
            if frame:
                self.frames.append(frame)
                self.timestamps.append(time.time())

    def close(self):
        self.sock.close()


class HeartbeatCollector:
    def __init__(self, port: int):
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", port))
        self.sock.listen(5)
        self.sock.settimeout(0.2)
        self.last_rx = 0.0
        self.count = 0
        self.timestamps = []

    def collect(self, duration: float):
        deadline = time.time() + duration
        while time.time() < deadline:
            try:
                conn, _ = self.sock.accept()
            except socket.timeout:
                continue
            with conn:
                data = conn.recv(65535)
                frame = decode_frame(data)
                if frame and frame[0] == MSG_HEARTBEAT:
                    self.last_rx = time.time()
                    self.count += 1
                    self.timestamps.append(self.last_rx)

    def close(self):
        self.sock.close()


class MockBridgeProcess:
    def __init__(self, *, udp_port: int, tcp_port: int, heartbeat_port: int, extra_args=None):
        self.args = [
            sys.executable,
            str(SCRIPT),
            "--udp-port",
            str(udp_port),
            "--tcp-port",
            str(tcp_port),
            "--heartbeat-port",
            str(heartbeat_port),
        ]
        if extra_args:
            self.args.extend(extra_args)
        self.proc = None

    def __enter__(self):
        self.proc = subprocess.Popen(self.args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.8)
        return self

    def __exit__(self, exc_type, exc, tb):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()


class TestPersistentTcpAndReconnect(unittest.TestCase):
    def test_persistent_tcp_multiple_frames_single_session(self):
        udp_port = find_free_port()
        tcp_port = find_free_port()
        heartbeat_port = find_free_port()
        collector = UdpCollector(udp_port)
        with MockBridgeProcess(udp_port=udp_port, tcp_port=tcp_port, heartbeat_port=heartbeat_port):
            with socket.create_connection(("127.0.0.1", tcp_port), timeout=2) as sock:
                sock.sendall(pack_frame(MSG_MODESELECTION, 1, {"mode": 1, "use_mode": 0, "selectId": [1]}))
                sock.sendall(pack_frame(MSG_UAVCOMMAND, 1, {"Command_ID": 1, "Agent_CMD": 2}))
                sock.sendall(pack_frame(MSG_UAVCOMMAND, 1, {"Command_ID": 2, "Agent_CMD": 3}))
                time.sleep(0.4)
            collector.collect(1.2)
        collector.close()

        textinfo = [frame for frame in collector.frames if frame[0] == MSG_TEXTINFO]
        self.assertGreaterEqual(len(textinfo), 3)
        self.assertTrue(any("msg_id=202" in frame[2]["Message"] for frame in textinfo))
        self.assertTrue(any("msg_id=108" in frame[2]["Message"] for frame in textinfo))

    def test_jetson_disconnect_then_ground_station_reconnect(self):
        udp_port = find_free_port()
        tcp_port = find_free_port()
        heartbeat_port = find_free_port()
        collector = UdpCollector(udp_port)
        with MockBridgeProcess(
            udp_port=udp_port,
            tcp_port=tcp_port,
            heartbeat_port=heartbeat_port,
            extra_args=["--drop-after", "2"],
        ):
            sock = socket.create_connection(("127.0.0.1", tcp_port), timeout=2)
            sock.settimeout(2)
            sock.sendall(pack_frame(MSG_MODESELECTION, 1, {"mode": 1, "use_mode": 0, "selectId": [1]}))
            sock.sendall(pack_frame(MSG_UAVCOMMAND, 1, {"Command_ID": 1, "Agent_CMD": 2}))
            time.sleep(0.6)
            self.assertEqual(sock.recv(1), b"")
            sock.close()

            with socket.create_connection(("127.0.0.1", tcp_port), timeout=2) as retry:
                retry.sendall(pack_frame(MSG_MODESELECTION, 1, {"mode": 1, "use_mode": 0, "selectId": [1]}))
                time.sleep(0.4)
            collector.collect(1.2)
        collector.close()

        textinfo = [frame for frame in collector.frames if frame[0] == MSG_TEXTINFO]
        self.assertGreaterEqual(len(textinfo), 3)


class TestFreshnessTimeouts(unittest.TestCase):
    def test_udp_stale_detection_window(self):
        udp_port = find_free_port()
        tcp_port = find_free_port()
        heartbeat_port = find_free_port()
        collector = UdpCollector(udp_port)
        with MockBridgeProcess(
            udp_port=udp_port,
            tcp_port=tcp_port,
            heartbeat_port=heartbeat_port,
            extra_args=["--udp-pause-after", "1.0", "--udp-pause-duration", "4.0"],
        ):
            collector.collect(6.0)
        collector.close()

        self.assertGreaterEqual(len(collector.timestamps), 2)
        gaps = [
            collector.timestamps[idx] - collector.timestamps[idx - 1]
            for idx in range(1, len(collector.timestamps))
        ]
        self.assertTrue(any(gap > 3.0 for gap in gaps), f"Expected UDP stale gap, got {gaps}")

    def test_heartbeat_stale_detection_window(self):
        udp_port = find_free_port()
        tcp_port = find_free_port()
        heartbeat_port = find_free_port()
        collector = HeartbeatCollector(heartbeat_port)
        with MockBridgeProcess(
            udp_port=udp_port,
            tcp_port=tcp_port,
            heartbeat_port=heartbeat_port,
            extra_args=["--heartbeat-pause-after", "1.0", "--heartbeat-pause-duration", "4.0"],
        ):
            collector.collect(6.0)
        collector.close()

        self.assertGreaterEqual(collector.count, 2)
        gaps = [
            collector.timestamps[idx] - collector.timestamps[idx - 1]
            for idx in range(1, len(collector.timestamps))
        ]
        self.assertTrue(any(gap > 3.5 for gap in gaps), f"Expected heartbeat stale gap, got {gaps}")


if __name__ == "__main__":
    unittest.main()
