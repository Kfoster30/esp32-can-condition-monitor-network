from contextlib import redirect_stdout
import csv
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from capture_can import Analyzer, capture, counter_delta, parse_telemetry


def healthy_run(seconds=10):
    analyzer = Analyzer()
    for tick in range(seconds * 10):
        timestamp = 10000 + tick * 100
        elapsed = tick / 10
        if tick % 10 == 0:
            analyzer.add_line(f"heartbeat,{timestamp},1,1,1,{100 + tick // 10}", elapsed)
        if tick % 2 == 0:
            analyzer.add_line(f"condition,{timestamp},RUN,0,0,90.00,25.0", elapsed)
        analyzer.add_line(f"motion_power,{timestamp},100,125,3.300,4.2", elapsed)
    return analyzer


class CaptureTests(unittest.TestCase):
    def test_healthy_run(self):
        report = healthy_run().summary(10, 10, True)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["streams"]["heartbeat"]["count"], 10)
        self.assertEqual(report["streams"]["condition"]["mean_period_ms"], 200)
        self.assertEqual(report["streams"]["motion_power"]["observed_rate_hz"], 10)
        self.assertTrue(any("sequence" in text for text in report["limitations"]))

    def test_disconnect_recovery_keeps_timeout_evidence(self):
        analyzer = healthy_run()
        analyzer.add_line("#event,data_timeout,1", 11)
        analyzer.add_line("#event,heartbeat_timeout,1", 12.5)
        analyzer.add_line("#event,data_timeout,0", 14)
        analyzer.add_line("#event,heartbeat_timeout,0", 14.5)
        report = analyzer.summary(15, 15, True)
        self.assertEqual(len(report["timeout_events"]), 4)
        self.assertEqual(report["status"], "REVIEW")

    def test_restart_detection(self):
        analyzer = Analyzer()
        analyzer.add_line("heartbeat,50000,1,1,1,100", 0)
        analyzer.add_line("heartbeat,51000,1,1,1,1", 1)
        self.assertEqual(analyzer.sensor_restarts, 1)
        analyzer.add_line("heartbeat,500,1,1,1,2", 2)
        self.assertEqual(analyzer.dashboard_restarts, 1)

    def test_millis_wrap_is_not_restart(self):
        analyzer = Analyzer()
        analyzer.add_line("motion_power,4294967200,100,125,3.300,4.2", 0)
        analyzer.add_line("motion_power,4,100,125,3.300,4.2", 0.1)
        self.assertEqual(analyzer.dashboard_restarts, 0)
        self.assertEqual(analyzer.streams["motion_power"]["max_interval_ms"], 100)
        self.assertEqual(counter_delta(1, 10), None)

    def test_bad_records(self):
        analyzer = Analyzer()
        for line in ("heartbeat,10,1,1,1", "condition,10,BOGUS,0,0,90,25",
                     "motion_power,10,100,125,nan,4.2", "#event,data_timeout,2"):
            analyzer.add_line(line, 0)
        self.assertEqual(analyzer.malformed_lines, 4)
        self.assertIsNone(parse_telemetry("#can,ready,bitrate=500000,tx=17,rx=16"))

    def test_missing_and_incomplete_capture(self):
        self.assertEqual(Analyzer().summary(10, 10, True)["status"], "REVIEW")
        self.assertEqual(healthy_run().summary(10, 600, False)["status"], "REVIEW")

    def test_late_gap(self):
        analyzer = healthy_run()
        analyzer.add_line("motion_power,20200,100,125,3.300,4.2", 10.2)
        self.assertEqual(analyzer.streams["motion_power"]["late_intervals"], 1)
        self.assertEqual(analyzer.summary(10.3, 10.3, True)["status"], "REVIEW")

    def test_serial_failure_and_wrong_role(self):
        analyzer = healthy_run()
        analyzer.add_line("#role=SENSOR,simulation=1", 0)
        report = analyzer.summary(10, 10, True, "port disconnected")
        self.assertEqual(report["status"], "REVIEW")
        self.assertTrue(any("dashboard serial port" in text for text in report["review_reasons"]))

    def test_end_silence(self):
        report = healthy_run().summary(14, 14, True)
        self.assertEqual(report["status"], "REVIEW")
        self.assertTrue(any("boundary" in text for text in report["review_reasons"]))

    def test_boot_and_error_reported(self):
        analyzer = healthy_run()
        analyzer.add_line("#project=ESP32 CAN Condition Monitor Network,firmware=0.1.0", 1)
        analyzer.add_line("E (154) esp_core_dump_flash: Core dump check failed", 1)
        self.assertEqual(analyzer.startup_banners, 1)
        self.assertEqual(analyzer.warning_error_lines, 1)
        self.assertEqual(analyzer.summary(10, 10, True)["status"], "REVIEW")

    def test_missing_temperature_is_recorded_not_discarded(self):
        analyzer = healthy_run()
        self.assertIsNotNone(analyzer.add_line("condition,20000,DEGRADED,64,0,90.00,nan", 10))
        self.assertEqual(analyzer.malformed_lines, 0)
        self.assertEqual(analyzer.temperature_missing_records, 1)
        self.assertEqual(analyzer.summary(10.1, 10.1, True)["status"], "REVIEW")

    def test_stale_and_unlabeled_nan_rules(self):
        analyzer = Analyzer()
        self.assertIsNotNone(analyzer.add_line("condition,100,DEGRADED,128,0,90.00,24.0", 0))
        self.assertEqual(analyzer.temperature_stale_records, 1)
        for line in ("condition,200,RUN,0,0,90.00,nan",
                     "condition,300,DEGRADED,192,0,90.00,nan",
                     "condition,400,RUN,128,0,90.00,24.0",
                     "condition,500,DEGRADED,64,0,90.00,24.0"):
            analyzer.add_line(line, 1)
        self.assertEqual(analyzer.malformed_lines, 4)

    def test_require_hardware_rejects_simulated_capture(self):
        analyzer = healthy_run()
        analyzer.require_hardware_temperature = True
        self.assertEqual(analyzer.summary(10, 10, True)["status"], "REVIEW")

    def test_require_hardware_passes_mixed_source(self):
        analyzer = Analyzer(require_hardware_temperature=True)
        for tick in range(100):
            timestamp = 10000 + tick * 100
            if tick % 10 == 0:
                analyzer.add_line(f"heartbeat,{timestamp},1,1,3,{100 + tick // 10}", tick / 10)
            if tick % 2 == 0:
                analyzer.add_line(f"condition,{timestamp},RUN,0,0,90.00,24.0", tick / 10)
            analyzer.add_line(f"motion_power,{timestamp},100,125,3.300,4.2", tick / 10)
        self.assertEqual(analyzer.summary(10, 10, True)["status"], "PASS")
        self.assertEqual(analyzer.heartbeat_flags_seen, {3})

    def test_capture_writes_artifacts_without_reset(self):
        class FakeSerial:
            def __init__(self, **kwargs):
                self.tick = 0
                self.in_waiting = 4096

            def open(self):
                if self.dtr or self.rts:
                    raise AssertionError("Control lines must be inactive before open")

            def read(self, size):
                timestamp = 10000 + self.tick * 100
                lines = []
                if self.tick % 10 == 0:
                    lines.append(f"heartbeat,{timestamp},1,1,1,{100 + self.tick // 10}")
                if self.tick % 2 == 0:
                    lines.append(f"condition,{timestamp},RUN,0,0,90.00,25.0")
                lines.append(f"motion_power,{timestamp},100,125,3.300,4.2")
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
            args = SimpleNamespace(port="FAKE", seconds=5, output_root=folder)
            with (patch.dict("sys.modules", {"serial": fake_serial}),
                  patch("capture_can.time.monotonic", side_effect=clock),
                  patch("capture_can.source_commit", return_value="test-only"),
                  redirect_stdout(io.StringIO())):
                result = capture(args)
            self.assertEqual(result, 0)
            output = next(Path(folder).iterdir())
            report = json.loads((output / "summary.json").read_text())
            self.assertEqual(report["status"], "PASS")
            self.assertEqual(report["port"], "FAKE")
            self.assertTrue((output / "raw.log").read_bytes().startswith(b"heartbeat,"))
            with (output / "telemetry.csv").open(newline="") as csv_file:
                rows = list(csv.reader(csv_file))
            self.assertTrue(all(len(row) == 10 for row in rows))
            self.assertEqual(len(rows) - 1, sum(s["count"] for s in report["streams"].values()))


if __name__ == "__main__":
    unittest.main()
