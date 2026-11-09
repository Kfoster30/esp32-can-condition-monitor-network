import unittest

from can_protocol import (Condition, Heartbeat, MotionPower,
                          TEMPERATURE_UNAVAILABLE, WARN_TEMPERATURE_MISSING,
                          WARN_TEMPERATURE_STALE)


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

    def test_missing_temperature_vector(self):
        value = Condition(7, 2, WARN_TEMPERATURE_MISSING, 0, 8750, TEMPERATURE_UNAVAILABLE)
        self.assertEqual(value.encode().hex(), "070240002e220080")
        self.assertEqual(Condition.decode(value.encode()), value)

    def test_stale_temperature_retains_labeled_value(self):
        value = Condition(8, 2, WARN_TEMPERATURE_STALE, 0, 8750, 240)
        self.assertEqual(value.encode().hex(), "080280002e22f000")
        self.assertEqual(Condition.decode(value.encode()), value)

    def test_temperature_sentinel_is_never_unlabeled(self):
        for warnings, temperature in ((0, TEMPERATURE_UNAVAILABLE), (64, 240),
                                      (192, TEMPERATURE_UNAVAILABLE),
                                      (128, TEMPERATURE_UNAVAILABLE)):
            with self.subTest(warnings=warnings, temperature=temperature):
                with self.assertRaises(ValueError):
                    Condition(0, 2, warnings, 0, 9000, temperature).encode()
                payload = bytes([0, 2, warnings, 0, 0, 0]) + temperature.to_bytes(2, "little", signed=True)
                with self.assertRaises(ValueError):
                    Condition.decode(payload)

    def test_mixed_heartbeat_has_explicit_hardware_flag(self):
        value = Heartbeat(1, 1, 0, 3, 10)
        self.assertEqual(value.encode().hex(), "010100030a000000")
        self.assertEqual(Heartbeat.decode(value.encode()).flags, 3)


if __name__ == "__main__":
    unittest.main()

