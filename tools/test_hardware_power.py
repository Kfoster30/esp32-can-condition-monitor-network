"""0.4 codec/capture contracts; MCU state/driver checks are separate evidence."""

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from can_protocol import (BUS_UNAVAILABLE, CURRENT_UNAVAILABLE, WARN_POWER_MISSING,
                          Condition, Heartbeat, MotionPower, Session)
from capture_can import Analyzer, capture, csv_record, parse_telemetry


def combined_run(flags=7, seconds=10):
    analyzer = Analyzer(require_hardware_temperature=True, require_sequences=True,
                        require_hardware_power=True)
    for tick in range(seconds * 10):
        timestamp = 10000 + tick * 100
        elapsed = tick / 10
        if tick % 10 == 0:
            analyzer.add_line(f"heartbeat,{timestamp},1,1,{flags},{100 + tick // 10},{tick // 10 % 256},1234", elapsed)
        if tick % 2 == 0:
            analyzer.add_line(f"condition,{timestamp},RUN,0,0,90.00,24.0,{tick // 2 % 256},1234", elapsed)
        analyzer.add_line(f"motion_power,{timestamp},100,125,3.332,4.2,{tick % 256},1234", elapsed)
    return analyzer


class HardwarePowerTests(unittest.TestCase):
    def test_combined_source_golden_vectors(self):
        heartbeat = Heartbeat(1, 1, 42, 7, 0x12345678)
        self.assertEqual(heartbeat.encode().hex(), "01012a0778563412")
        self.assertEqual(Heartbeat.decode(heartbeat.encode()), heartbeat)
        session = Session(1, 1, 0x12345678, 7)
        self.assertEqual(session.encode().hex(), "0101785634120700")
        self.assertEqual(Session.decode(session.encode()), session)

    def test_all_supported_source_modes(self):
        for flags in (1, 3, 5, 7):
            self.assertEqual(Heartbeat.decode(Heartbeat(1, 1, 0, flags, 1).encode()).flags, flags)
            self.assertEqual(Session.decode(Session(1, 1, 1, flags).encode()).flags, flags)

    def test_unknown_source_modes_rejected(self):
        for flags in (0, 2, 4, 6, 9, 255):
            with self.subTest(flags=flags):
                with self.assertRaises(ValueError):
                    Heartbeat(1, 1, 0, flags, 1).encode()
                with self.assertRaises(ValueError):
                    Heartbeat.decode(struct.pack("<BBBBI", 1, 1, 0, flags, 1))
                with self.assertRaises(ValueError):
                    Session(1, 1, 1, flags).encode()
                with self.assertRaises(ValueError):
                    parse_telemetry(f"heartbeat,100,1,1,{flags},1")

    def test_hardware_power_golden_vector(self):
        message = MotionPower(9, 100, 125, 3332, 42)
        self.assertEqual(message.encode().hex(), "09647d00040d2a00")
        self.assertEqual(MotionPower.decode(message.encode()), message)

    def test_missing_power_golden_vector(self):
        message = MotionPower(9, 100, 125, BUS_UNAVAILABLE, CURRENT_UNAVAILABLE)
        self.assertEqual(message.encode().hex(), "09647d00ffff0080")
        self.assertEqual(MotionPower.decode(message.encode()), message)

    def test_half_sentinels_rejected_in_both_directions(self):
        for bus, current in ((3332, CURRENT_UNAVAILABLE), (BUS_UNAVAILABLE, 0)):
            with self.assertRaises(ValueError):
                MotionPower(0, 100, 125, bus, current).encode()
            with self.assertRaises(ValueError):
                MotionPower.decode(struct.pack("<BBHHh", 0, 100, 125, bus, current))

    def test_measured_zero_and_signed_current_preserved(self):
        for current in (-1, 0, 42):
            value = MotionPower(0, 100, 125, 3332, current)
            self.assertEqual(MotionPower.decode(value.encode()), value)
        self.assertEqual(MotionPower.decode(MotionPower(0, 100, 0, 0, 0).encode()).bus_millivolts, 0)

    def test_power_warning_independent_of_temperature(self):
        value = Condition(0, 2, WARN_POWER_MISSING, 0, 9000, 240)
        self.assertEqual(value.encode().hex(), "000220002823f000")
        self.assertEqual(Condition.decode(value.encode()), value)
        missing_both = Condition(0, 2, 96, 0, 9000, -32768)
        self.assertEqual(Condition.decode(missing_both.encode()), missing_both)

    def test_power_warning_requires_degraded_state(self):
        with self.assertRaises(ValueError):
            Condition(0, 1, 32, 0, 9000, 240).encode()
        with self.assertRaises(ValueError):
            Condition.decode(struct.pack("<BBBBHh", 0, 1, 32, 0, 9000, 240))
        with self.assertRaises(ValueError):
            parse_telemetry("condition,100,RUN,32,0,90.00,24.0")

    def test_paired_nan_preserved_in_csv(self):
        fields = parse_telemetry("motion_power,100,100,125,nan,nan,1,1234")
        row = csv_record(fields, 0.5)
        self.assertEqual(len(row), 10)
        self.assertEqual(row[5:7], ["nan", "nan"])
        self.assertEqual(row[-2:], ["1", "1234"])

    def test_nonfinite_or_half_missing_power_is_malformed(self):
        for bus, current in (("nan", "4.2"), ("3.332", "nan"), ("inf", "inf"),
                             ("-1", "0"), ("65.535", "0"), ("3.332", "-3276.8")):
            with self.assertRaises(ValueError):
                parse_telemetry(f"motion_power,100,100,125,{bus},{current}")

    def test_combined_healthy_capture_passes_all_requirements(self):
        report = combined_run().summary(10, 10, True)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["sequence_check"]["status"], "PASS")
        self.assertEqual(report["heartbeat_flags_seen"], [7])
        self.assertEqual(report["power_missing_records"], 0)

    def test_old_dht_mode_cannot_pass_hardware_power_requirement(self):
        report = combined_run(flags=3).summary(10, 10, True)
        self.assertEqual(report["status"], "REVIEW")
        self.assertTrue(any("INA219 hardware mode" in reason for reason in report["review_reasons"]))

    def test_ina_only_mode_cannot_pass_temperature_requirement(self):
        report = combined_run(flags=5).summary(10, 10, True)
        self.assertEqual(report["status"], "REVIEW")
        self.assertTrue(any("DHT11 hardware mode" in reason for reason in report["review_reasons"]))

    def test_mode_change_is_not_hidden(self):
        analyzer = combined_run()
        analyzer.add_line("heartbeat,20000,1,1,3,110,10,1234", 10)
        self.assertEqual(analyzer.summary(10.1, 10.1, True)["status"], "REVIEW")

    def test_sensor_loss_keeps_temperature_numeric_and_records_power_nan(self):
        analyzer = combined_run()
        analyzer.add_line("condition,20000,DEGRADED,32,0,90.00,24.0,50,1234", 10)
        analyzer.add_line("motion_power,20000,100,125,nan,nan,100,1234", 10)
        report = analyzer.summary(10.1, 10.1, True)
        self.assertEqual(report["status"], "REVIEW")
        self.assertEqual(report["power_missing_records"], 1)
        self.assertEqual(report["power_missing_condition_records"], 1)
        self.assertEqual(report["temperature_missing_records"], 0)
        self.assertEqual(report["malformed_lines"], 0)
        self.assertEqual(report["sequence_check"]["status"], "PASS")

    def test_one_missing_power_indicator_is_sufficient_for_review(self):
        for line in ("condition,20000,DEGRADED,32,0,90.00,24.0,50,1234",
                     "motion_power,20000,100,125,nan,nan,100,1234"):
            analyzer = combined_run()
            analyzer.add_line(line, 10)
            report = analyzer.summary(10.1, 10.1, True)
            self.assertTrue(any("Power readings" in text for text in report["review_reasons"]))

    def test_numeric_recovery_does_not_erase_fault_evidence(self):
        analyzer = combined_run()
        analyzer.add_line("motion_power,20000,100,125,nan,nan,100,1234", 10)
        analyzer.add_line("motion_power,20100,100,125,3.332,4.2,101,1234", 10.1)
        report = analyzer.summary(10.2, 10.2, True)
        self.assertEqual(report["power_missing_records"], 1)
        self.assertEqual(report["status"], "REVIEW")

    def test_unexpected_ascii_and_duplicate_rendered_text_require_review(self):
        analyzer = combined_run()
        analyzer.add_line("garbledgarbled", 1)
        analyzer.add_line("motion_power,11000,motion_power,11000,100,125,3.332,4.2", 1)
        report = analyzer.summary(10, 10, True)
        self.assertEqual(report["unexpected_lines"], 1)
        self.assertEqual(report["malformed_lines"], 1)
        self.assertEqual(report["status"], "REVIEW")

    def test_source_and_quality_comments_do_not_fail_capture(self):
        analyzer = combined_run()
        for line in ("#sources,condition=SIM,temperature=DHT11,motion=SIM,power=INA219",
                     "#power_quality,HW_VALID", "#temperature_quality,HW_VALID"):
            analyzer.add_line(line, 0)
        self.assertEqual(analyzer.summary(10, 10, True)["status"], "PASS")

    def test_captured_combined_artifacts_include_power_requirement(self):
        class FakeSerial:
            def __init__(self, **kwargs):
                self.tick = 0
                self.in_waiting = 4096

            def open(self):
                if self.dtr or self.rts:
                    raise AssertionError("Reset lines must remain inactive")

            def read(self, size):
                tick = self.tick
                timestamp = 10000 + tick * 100
                lines = []
                if tick % 10 == 0:
                    lines.append(f"heartbeat,{timestamp},1,1,7,{100 + tick // 10},{tick // 10 % 256},1234")
                if tick % 2 == 0:
                    lines.append(f"condition,{timestamp},RUN,0,0,90.00,24.0,{tick // 2 % 256},1234")
                lines.append(f"motion_power,{timestamp},100,125,3.332,4.2,{tick % 256},1234")
                self.tick += 1
                return ("\n".join(lines) + "\n").encode()

            def close(self):
                pass

        clock_value = 0

        def clock():
            nonlocal clock_value
            clock_value += 0.05
            return clock_value

        fake_serial = SimpleNamespace(Serial=FakeSerial, SerialException=OSError)
        with tempfile.TemporaryDirectory() as folder:
            args = SimpleNamespace(port="FAKE", seconds=5, output_root=folder,
                                   require_hardware_temperature=True, require_sequences=True,
                                   require_hardware_power=True)
            with (patch.dict("sys.modules", {"serial": fake_serial}),
                  patch("capture_can.time.monotonic", side_effect=clock),
                  patch("capture_can.source_commit", return_value="test-only"),
                  redirect_stdout(io.StringIO())):
                result = capture(args)
            report = json.loads((next(Path(folder).iterdir()) / "summary.json").read_text())
            self.assertEqual(result, 0)
            self.assertTrue(report["require_hardware_power"])
            self.assertEqual(report["heartbeat_flags_seen"], [7])


if __name__ == "__main__":
    unittest.main()
