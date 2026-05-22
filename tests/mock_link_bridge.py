import argparse
import json
import math
import socket
import struct
import threading
import time


MAGIC = b"\x61\x6d"
FRAME_OVERHEAD = 10

MSG_UAVSTATE = 1
MSG_TEXTINFO = 3
MSG_HEARTBEAT = 6
MSG_UAVCONTROLSTATE = 9


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
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    frame = MAGIC + struct.pack("<I", len(body)) + struct.pack("BB", msg_id & 0xFF, robot_id & 0xFF) + body
    return frame + struct.pack("<H", crc16_arc(frame))


def unpack_stream(buffer: bytearray):
    frames = []
    while len(buffer) >= FRAME_OVERHEAD:
        magic = buffer.find(MAGIC)
        if magic < 0:
            buffer.clear()
            break
        if magic > 0:
            del buffer[:magic]
        if len(buffer) < FRAME_OVERHEAD:
            break
        payload_size = struct.unpack_from("<I", buffer, 2)[0]
        total = payload_size + FRAME_OVERHEAD
        if len(buffer) < total:
            break
        frame = bytes(buffer[:total])
        expected_crc = struct.unpack_from("<H", frame, total - 2)[0]
        if expected_crc != crc16_arc(frame[:-2]):
            del buffer[0]
            continue
        payload = json.loads(frame[8:-2] or b"{}")
        frames.append((frame[6], frame[7], payload))
        del buffer[:total]
    return frames


class MockLinkBridge:
    def __init__(self, args):
        self.args = args
        self._stop = threading.Event()
        self._tcp_server = None
        self._udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._lock = threading.Lock()
        self._active_conn = None
        self._active_addr = None
        self._frame_count = 0
        self._last_ground_heartbeat = 0.0
        self._start_time = time.time()

    def run(self):
        threads = [
            threading.Thread(target=self._run_tcp_server, daemon=True),
            threading.Thread(target=self._run_udp_telemetry, daemon=True),
            threading.Thread(target=self._run_heartbeat_sender, daemon=True),
        ]
        for thread in threads:
            thread.start()
        try:
            while not self._stop.is_set():
                time.sleep(0.1)
        except KeyboardInterrupt:
            pass
        finally:
            self._stop.set()
            with self._lock:
                if self._active_conn:
                    try:
                        self._active_conn.close()
                    except OSError:
                        pass
                    self._active_conn = None
            if self._tcp_server:
                try:
                    self._tcp_server.close()
                except OSError:
                    pass
            self._udp_sock.close()

    def _run_tcp_server(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((self.args.host, self.args.tcp_port))
        server.listen(5)
        server.settimeout(0.5)
        self._tcp_server = server
        while not self._stop.is_set():
            try:
                conn, addr = server.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            with self._lock:
                takeover_allowed = (
                    self._active_conn is None
                    or self._active_addr == addr[0]
                    or (self._last_ground_heartbeat and time.time() - self._last_ground_heartbeat > self.args.heartbeat_timeout)
                )
                if not takeover_allowed:
                    conn.close()
                    continue
                if self._active_conn is not None:
                    try:
                        self._active_conn.close()
                    except OSError:
                        pass
                self._active_conn = conn
                self._active_addr = addr[0]

            threading.Thread(target=self._handle_client, args=(conn, addr), daemon=True).start()

    def _handle_client(self, conn: socket.socket, addr):
        conn.settimeout(0.5)
        buffer = bytearray()
        try:
            while not self._stop.is_set():
                try:
                    chunk = conn.recv(4096)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                buffer.extend(chunk)
                for msg_id, robot_id, payload in unpack_stream(buffer):
                    self._frame_count += 1
                    if msg_id == MSG_HEARTBEAT:
                        self._last_ground_heartbeat = time.time()
                    self._send_udp_ack(robot_id, msg_id, payload)
                    if self.args.drop_after and self._frame_count >= self.args.drop_after:
                        conn.close()
                        return
        finally:
            with self._lock:
                if self._active_conn is conn:
                    self._active_conn = None

    def _send_udp_ack(self, robot_id: int, msg_id: int, payload: dict):
        text = {
            "MessageType": 0,
            "Message": f"ack msg_id={msg_id} payload_keys={sorted(payload.keys())}",
        }
        self._udp_sock.sendto(encode_frame(MSG_TEXTINFO, robot_id, text), (self.args.target_host, self.args.udp_port))

    def _run_udp_telemetry(self):
        while not self._stop.is_set():
            elapsed = time.time() - self._start_time
            paused = self.args.udp_pause_after > 0 and self.args.udp_pause_after <= elapsed < (self.args.udp_pause_after + self.args.udp_pause_duration)
            if not paused:
                x = round(math.cos(elapsed) * 2.0, 3)
                y = round(math.sin(elapsed) * 2.0, 3)
                uav_state = {
                    "uav_id": 1,
                    "connected": True,
                    "armed": True,
                    "mode": "OFFBOARD",
                    "location_source": 0,
                    "gps_status": 3,
                    "battery_state": 23.8,
                    "battery_percetage": 0.76,
                    "altitude": 1.5,
                    "rel_alt": 1.5,
                    "range": 1.4,
                    "position": [x, y, 1.5],
                    "velocity": [0.0, 0.0, 0.0],
                    "attitude": [0.0, 0.0, 0.0],
                }
                control_state = {
                    "uav_id": 1,
                    "control_state": 2,
                    "pos_controller": 0,
                    "failsafe": False,
                    "exec_state": 3,
                    "mission_mode": 1,
                    "active_command_source": 5,
                    "pending_request": 0,
                    "request_active": False,
                }
                self._udp_sock.sendto(encode_frame(MSG_UAVSTATE, 1, uav_state), (self.args.target_host, self.args.udp_port))
                self._udp_sock.sendto(encode_frame(MSG_UAVCONTROLSTATE, 1, control_state), (self.args.target_host, self.args.udp_port))
            time.sleep(self.args.telemetry_interval)

    def _run_heartbeat_sender(self):
        count = 0
        while not self._stop.is_set():
            elapsed = time.time() - self._start_time
            paused = self.args.heartbeat_pause_after > 0 and self.args.heartbeat_pause_after <= elapsed < (self.args.heartbeat_pause_after + self.args.heartbeat_pause_duration)
            if not paused:
                heartbeat = {"count": count, "message": "Mock heartbeat"}
                frame = encode_frame(MSG_HEARTBEAT, 1, heartbeat)
                try:
                    with socket.create_connection((self.args.target_host, self.args.heartbeat_port), timeout=0.5) as sock:
                        sock.sendall(frame)
                except OSError:
                    pass
                count += 1
            time.sleep(self.args.heartbeat_interval)


def main():
    parser = argparse.ArgumentParser(description="Link-level mock Jetson bridge")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--target-host", default="127.0.0.1")
    parser.add_argument("--udp-port", type=int, default=8889)
    parser.add_argument("--tcp-port", type=int, default=55555)
    parser.add_argument("--heartbeat-port", type=int, default=55556)
    parser.add_argument("--telemetry-interval", type=float, default=0.1)
    parser.add_argument("--heartbeat-interval", type=float, default=1.0)
    parser.add_argument("--heartbeat-timeout", type=float, default=3.5)
    parser.add_argument("--drop-after", type=int, default=0)
    parser.add_argument("--udp-pause-after", type=float, default=0.0)
    parser.add_argument("--udp-pause-duration", type=float, default=0.0)
    parser.add_argument("--heartbeat-pause-after", type=float, default=0.0)
    parser.add_argument("--heartbeat-pause-duration", type=float, default=0.0)
    args = parser.parse_args()
    MockLinkBridge(args).run()


if __name__ == "__main__":
    main()
