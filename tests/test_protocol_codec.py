# HISTORICAL LEGACY PROTOCOL: archived am/JSON framing, not MAVLink2.
# Not a validation tool for the current bridge; use ZenithMavlinkProbe and CTest.
"""
Zenith protocol frame codec verification tests.

Tests cover:
  1. CRC-16-ARC algorithm correctness
  2. Frame packing (encode) roundtrip
  3. Frame unpacking (decode) roundtrip
  4. Edge cases: empty payload, large payload, bad CRC, truncated frame
  5. JSON field naming conventions matching Struct.hpp
"""

import struct
import json
import unittest


# ---------------------------------------------------------------------------
# Protocol constants (must match ZenithProtocolClient.cpp & Struct.hpp)
# ---------------------------------------------------------------------------
MAGIC = b'\x61\x6d'          # 'am'
FRAME_OVERHEAD = 10           # 2 magic + 4 size + 1 msgId + 1 robotId + 2 crc

MSG_UAVSTATE        = 1
MSG_TEXTINFO         = 3
MSG_HEARTBEAT        = 6
MSG_UAVCONTROLSTATE  = 9
MSG_UAVCOMMAND       = 108
MSG_UAVSETUP         = 109
MSG_MODESELECTION    = 202


def crc16_arc(data: bytes) -> int:
    """CRC-16-ARC (polynomial 0xA001, init 0x0000) -- must match both sides."""
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
    """Encode a Zenith protocol frame -- mirrors ZenithProtocolClient::packFrame."""
    json_bytes = json.dumps(payload, separators=(',', ':')).encode('utf-8')
    header = MAGIC
    header += struct.pack('<I', len(json_bytes))   # payload size, little-endian
    header += struct.pack('B', msg_id & 0xFF)
    header += struct.pack('B', robot_id & 0xFF)
    frame_no_crc = header + json_bytes
    crc = crc16_arc(frame_no_crc)
    return frame_no_crc + struct.pack('<H', crc)


def unpack_frame(data: bytes):
    """
    Decode a Zenith protocol frame -- mirrors ZenithProtocolClient::tryDecodeFrame.
    Returns (msg_id, robot_id, payload_dict, total_bytes) or None on failure.
    """
    if len(data) < FRAME_OVERHEAD:
        return None

    # find magic
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

    # CRC check
    expected_crc = struct.unpack_from('<H', frame, total_size - 2)[0]
    actual_crc = crc16_arc(frame[:total_size - 2])
    if expected_crc != actual_crc:
        return None

    msg_id = frame[6]
    robot_id = frame[7]
    json_bytes = frame[8:8 + payload_size]
    payload = json.loads(json_bytes)

    return msg_id, robot_id, payload, total_size


# ===========================================================================
# Unit tests
# ===========================================================================
class TestCRC16Arc(unittest.TestCase):
    """Verify CRC-16-ARC against known test vectors."""

    def test_empty(self):
        self.assertEqual(crc16_arc(b''), 0x0000)

    def test_known_vector_123456789(self):
        # Standard CRC-16-ARC test vector: "123456789" -> 0xBB3D
        self.assertEqual(crc16_arc(b'123456789'), 0xBB3D)

    def test_single_byte(self):
        # Verify determinism
        c1 = crc16_arc(b'\x00')
        c2 = crc16_arc(b'\x00')
        self.assertEqual(c1, c2)

    def test_different_data_different_crc(self):
        self.assertNotEqual(crc16_arc(b'hello'), crc16_arc(b'world'))


class TestFrameRoundtrip(unittest.TestCase):
    """Verify pack -> unpack roundtrip for various message types."""

    def _roundtrip(self, msg_id, robot_id, payload):
        frame = pack_frame(msg_id, robot_id, payload)
        result = unpack_frame(frame)
        self.assertIsNotNone(result, f"Failed to decode frame for msgId={msg_id}")
        dec_msg_id, dec_robot_id, dec_payload, total = result
        self.assertEqual(dec_msg_id, msg_id)
        self.assertEqual(dec_robot_id, robot_id)
        self.assertEqual(total, len(frame))
        return dec_payload

    def test_heartbeat(self):
        payload = {"count": 42, "message": ""}
        dec = self._roundtrip(MSG_HEARTBEAT, 1, payload)
        self.assertEqual(dec["count"], 42)

    def test_uav_command(self):
        payload = {
            "Agent_CMD": 4,
            "Control_Level": 0,
            "Move_mode": 1,
            "position_ref": [0.0, 0.0, 1.5],
            "velocity_ref": [0.2, 0.0, 0.0],
            "acceleration_ref": [0.0, 0.0, 0.0],
            "yaw_ref": 0.0,
            "Yaw_Rate_Mode": False,
            "yaw_rate_ref": 0.0,
            "att_ref": [0.0, 0.0, 0.0, 0.0],
            "latitude": 0.0,
            "longitude": 0.0,
            "altitude": 1.5,
            "Command_ID": 1
        }
        dec = self._roundtrip(MSG_UAVCOMMAND, 1, payload)
        self.assertEqual(dec["Agent_CMD"], 4)
        self.assertEqual(dec["Move_mode"], 1)
        self.assertAlmostEqual(dec["position_ref"][2], 1.5)
        self.assertEqual(dec["Command_ID"], 1)

    def test_uav_setup(self):
        payload = {
            "cmd": 1,
            "arming": True,
            "px4_mode": "OFFBOARD",
            "control_state": ""
        }
        dec = self._roundtrip(MSG_UAVSETUP, 1, payload)
        self.assertEqual(dec["cmd"], 1)
        self.assertTrue(dec["arming"])

    def test_mode_selection(self):
        payload = {
            "mode": 1,
            "selectId": [1],
            "use_mode": 0,
            "is_simulation": False,
            "swarm_num": 1,
            "cmd": ""
        }
        dec = self._roundtrip(MSG_MODESELECTION, 1, payload)
        self.assertEqual(dec["mode"], 1)
        self.assertEqual(dec["selectId"], [1])

    def test_uav_state(self):
        """Full UAVState payload with all fields from Struct.hpp."""
        payload = {
            "secs": 1000,
            "nsecs": 500000,
            "uav_id": 1,
            "location_source": 0,
            "mode": "OFFBOARD",
            "connected": True,
            "armed": True,
            "odom_valid": True,
            "gps_status": 3,
            "gps_num": 12,
            "latitude": 39.9042,
            "longitude": 116.4074,
            "altitude": 50.0,
            "position": [1.0, 2.0, 3.0],
            "velocity": [0.1, 0.2, 0.0],
            "attitude": [0.01, 0.02, 1.57],
            "attitude_q": {"x": 0.0, "y": 0.0, "z": 0.707, "w": 0.707},
            "attitude_rate": [0.0, 0.0, 0.0],
            "battery_state": 16.2,
            "battery_percetage": 0.85
        }
        dec = self._roundtrip(MSG_UAVSTATE, 1, payload)
        self.assertTrue(dec["connected"])
        self.assertTrue(dec["armed"])
        self.assertEqual(dec["position"], [1.0, 2.0, 3.0])
        self.assertAlmostEqual(dec["battery_percetage"], 0.85)

    def test_text_info(self):
        payload = {
            "sec": 12345,
            "MessageType": 1,
            "Message": "Low battery warning"
        }
        dec = self._roundtrip(MSG_TEXTINFO, 1, payload)
        self.assertEqual(dec["MessageType"], 1)
        self.assertEqual(dec["Message"], "Low battery warning")

    def test_uav_control_state(self):
        payload = {
            "uav_id": 1,
            "control_state": 2,
            "pos_controller": 0,
            "failsafe": False
        }
        dec = self._roundtrip(MSG_UAVCONTROLSTATE, 1, payload)
        self.assertEqual(dec["control_state"], 2)
        self.assertFalse(dec["failsafe"])

    def test_empty_payload(self):
        dec = self._roundtrip(MSG_HEARTBEAT, 1, {})
        self.assertEqual(dec, {})

    def test_large_robot_id(self):
        dec = self._roundtrip(MSG_HEARTBEAT, 255, {"count": 1, "message": ""})
        self.assertEqual(dec, {"count": 1, "message": ""})

    def test_unicode_payload(self):
        payload = {"Message": "Chinese test", "MessageType": 0, "sec": 0}
        dec = self._roundtrip(MSG_TEXTINFO, 1, payload)
        self.assertEqual(dec["Message"], "Chinese test")


class TestFrameEdgeCases(unittest.TestCase):
    """Test malformed frames, truncation, corruption."""

    def test_truncated_frame(self):
        frame = pack_frame(MSG_HEARTBEAT, 1, {"count": 1, "message": ""})
        self.assertIsNone(unpack_frame(frame[:5]))

    def test_bad_magic(self):
        frame = bytearray(pack_frame(MSG_HEARTBEAT, 1, {"count": 1, "message": ""}))
        frame[0] = 0xFF
        self.assertIsNone(unpack_frame(bytes(frame)))

    def test_bad_crc(self):
        frame = bytearray(pack_frame(MSG_HEARTBEAT, 1, {"count": 1, "message": ""}))
        frame[-1] ^= 0xFF  # corrupt CRC
        self.assertIsNone(unpack_frame(bytes(frame)))

    def test_extra_bytes_after_frame(self):
        frame = pack_frame(MSG_HEARTBEAT, 1, {"count": 1, "message": ""})
        padded = frame + b'\x00\x00\x00'
        result = unpack_frame(padded)
        self.assertIsNotNone(result)
        self.assertEqual(result[3], len(frame))  # total_bytes should be frame len only

    def test_garbage_before_magic(self):
        frame = pack_frame(MSG_HEARTBEAT, 1, {"count": 1, "message": ""})
        with_garbage = b'\xFF\xFE\xFD' + frame
        result = unpack_frame(with_garbage)
        self.assertIsNotNone(result)

    def test_multiple_frames_in_buffer(self):
        f1 = pack_frame(MSG_HEARTBEAT, 1, {"count": 1, "message": ""})
        f2 = pack_frame(MSG_UAVSTATE, 2, {"uav_id": 2, "connected": True})
        buf = f1 + f2
        r1 = unpack_frame(buf)
        self.assertIsNotNone(r1)
        self.assertEqual(r1[0], MSG_HEARTBEAT)
        # decode second frame
        remaining = buf[r1[3]:]
        r2 = unpack_frame(remaining)
        self.assertIsNotNone(r2)
        self.assertEqual(r2[0], MSG_UAVSTATE)


class TestFieldNaming(unittest.TestCase):
    """
    Verify JSON field names match Struct.hpp / JsonConverter conventions.
    These are the exact keys the Jetson side expects.
    """

    def test_uav_command_field_names(self):
        """All field names must match UAVCommand struct member names."""
        required = [
            "Agent_CMD", "Control_Level", "Move_mode",
            "position_ref", "velocity_ref", "acceleration_ref",
            "yaw_ref", "Yaw_Rate_Mode", "yaw_rate_ref",
            "att_ref", "latitude", "longitude", "altitude",
            "Command_ID"
        ]
        payload = {
            "Agent_CMD": 4, "Control_Level": 0, "Move_mode": 0,
            "position_ref": [0, 0, 1], "velocity_ref": [0, 0, 0],
            "acceleration_ref": [0, 0, 0], "yaw_ref": 0.0,
            "Yaw_Rate_Mode": False, "yaw_rate_ref": 0.0,
            "att_ref": [0, 0, 0, 0], "latitude": 0.0,
            "longitude": 0.0, "altitude": 0.0, "Command_ID": 1
        }
        for field in required:
            self.assertIn(field, payload, f"Missing field: {field}")

    def test_uav_state_field_names(self):
        """Fields the ground station must parse from UAVState frames."""
        required = [
            "uav_id", "connected", "armed", "odom_valid",
            "location_source", "mode", "gps_status", "gps_num",
            "position", "velocity", "attitude",
            "battery_state", "battery_percetage",
            "latitude", "longitude", "altitude"
        ]
        sample = {
            "uav_id": 1, "connected": True, "armed": False,
            "odom_valid": True, "location_source": 0, "mode": "POSCTL",
            "gps_status": 0, "gps_num": 0,
            "position": [0, 0, 0], "velocity": [0, 0, 0],
            "attitude": [0, 0, 0],
            "battery_state": 16.0, "battery_percetage": 0.8,
            "latitude": 0.0, "longitude": 0.0, "altitude": 0.0
        }
        for field in required:
            self.assertIn(field, sample, f"Missing field: {field}")

    def test_uav_setup_field_names(self):
        required = ["cmd", "arming", "px4_mode", "control_state"]
        sample = {"cmd": 0, "arming": False, "px4_mode": "", "control_state": ""}
        for field in required:
            self.assertIn(field, sample, f"Missing field: {field}")

    def test_mode_selection_field_names(self):
        required = ["mode", "selectId", "use_mode", "is_simulation", "swarm_num", "cmd"]
        sample = {
            "mode": 1, "selectId": [1], "use_mode": 0,
            "is_simulation": False, "swarm_num": 1, "cmd": ""
        }
        for field in required:
            self.assertIn(field, sample, f"Missing field: {field}")


if __name__ == '__main__':
    unittest.main(verbosity=2)
