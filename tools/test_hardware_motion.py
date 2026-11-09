import unittest

from can_protocol import Condition, Heartbeat, MotionPower, Session
from capture_can import Analyzer, csv_record, parse_telemetry


class HardwareMotionTests(unittest.TestCase):
    def test_combined_source_and_missing_motion_wire_bytes(self):
        self.assertEqual(Heartbeat.decode(bytes.fromhex("0101050f01000000")).flags, 15)
        self.assertEqual(Session.decode(bytes.fromhex("0101785634120f00")).flags, 15)
        packet = bytes.fromhex("0500ffff040d2a00")
        value = MotionPower(5, 0, 65535, 3332, 42)
        self.assertEqual(MotionPower.decode(packet), value)
        self.assertEqual(value.encode(), packet)
        self.assertEqual(Condition.decode(bytes.fromhex("050210002823f000")).warnings, 16)

    def test_unavailable_motion_preserves_power_and_sequence(self):
        line = "motion_power,100,0,nan,3.332,4.2,5,1234"
        row = csv_record(parse_telemetry(line), 0.1)
        self.assertEqual(row[4:7], ["nan", "3.332", "4.2"])
        self.assertEqual(row[-2:], ["5", "1234"])
        with self.assertRaises(ValueError):
            parse_telemetry(line.replace(",0,nan", ",100,nan"))
        with self.assertRaises(ValueError):
            parse_telemetry("condition,100,RUN,16,0,90.00,24.0")

    def test_required_hardware_detects_legacy_and_missing_then_recovery(self):
        for flags in (7, 15):
            analyzer = Analyzer(require_hardware_motion=True)
            analyzer.add_line(f"heartbeat,100,1,1,{flags},1,0,1234", 0)
            analyzer.add_line("condition,100,DEGRADED,16,0,90.00,24.0,0,1234", 0)
            analyzer.add_line("motion_power,100,0,nan,3.332,4.2,0,1234", 0)
            analyzer.add_line("motion_power,200,100,0,3.332,4.2,1,1234", 0.1)
            report = analyzer.summary(0.2, 0.2, True)
            reasons = report["review_reasons"]
            self.assertEqual(report["motion_missing_records"], 1)
            self.assertEqual(report["motion_missing_condition_records"], 1)
            self.assertEqual(report["motion_min_mg"], 0)
            self.assertEqual(report["power_missing_records"], 0)
            self.assertIn("Motion readings were unavailable during capture.", reasons)
            self.assertEqual(any("MPU6050 hardware mode" in reason for reason in reasons),
                             flags == 7)
