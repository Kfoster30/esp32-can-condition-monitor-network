import unittest

from can_protocol import Condition, Heartbeat, MotionPower


class ProtocolTests(unittest.TestCase):
    def test_heartbeat_golden_vector(self):
        message = Heartbeat(1, 1, 42, 1, 0x12345678)
        self.assertEqual(message.encode().hex(), "01012a0178563412")
        self.assertEqual(Heartbeat.decode(message.encode()), message)

    def test_condition_golden_vector(self):
        message = Condition(7, 1, 2, 0, 8750, 263)
        self.assertEqual(message.encode().hex(), "070102002e220701")
        self.assertEqual(Condition.decode(message.encode()), message)

    def test_motion_power_golden_vector(self):
        message = MotionPower(9, 100, 125, 3300, -42)
        self.assertEqual(message.encode().hex(), "09647d00e40c d6ff".replace(" ", ""))
        self.assertEqual(MotionPower.decode(message.encode()), message)

    def test_decode_rejects_wrong_length(self):
        with self.assertRaises(ValueError):
            Heartbeat.decode(b"1234567")

    def test_condition_rejects_invalid_state(self):
        with self.assertRaises(ValueError):
            Condition.decode(bytes([0, 4, 0, 0, 0, 0, 0, 0]))


if __name__ == "__main__":
    unittest.main()

