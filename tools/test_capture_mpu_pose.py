"""Host-only diagnostic parser/averaging/capture tests, not MCU driver tests."""

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from capture_mpu_pose import Collector, analyze, capture, parse_sample


def sample(stamp=1000, xyz="0,0,1", norm="1", status="HW_SAMPLE"):
    return parse_sample(f"mpu6050_measure,{stamp},{status},{xyz},{norm}")


def window():
    return [{"host_elapsed_s": index * 0.22, **sample(index * 220)}
            for index in range(68)]


class MpuPoseTests(unittest.TestCase):
    def test_signed_axes_and_real_zero(self):
        self.assertEqual(sample(xyz="0,-1,0")["ay_g"], -1)
        self.assertEqual(sample(xyz="0,0,0", norm="0")["norm_g"], 0)

    def test_bad_numeric_and_wire_formats(self):
        for line in ("noise", "mpu6050_measure,1,HW_SAMPLE,0,0,1",
                     "mpu6050_measure,-1,HW_SAMPLE,0,0,1,1",
                     "mpu6050_measure,4294967296,HW_SAMPLE,0,0,1,1",
                     "mpu6050_measure,1,HW_SAMPLE,0,0,nan,nan",
                     "mpu6050_measure,1,HW_SAMPLE,0,0,inf,inf",
                     "mpu6050_measure,1,HW_SAMPLE,0,0,4,4",
                     "mpu6050_measure,1,HW_SAMPLE,0,0,1,0.9"):
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_sample(line)

    def test_failure_evidence_requires_all_nans(self):
        row = sample(xyz="nan,nan,nan", norm="nan", status="READ_FAILED")
        self.assertIsNone(row["ax_g"])
        with self.assertRaises(ValueError):
            sample(status="READ_FAILED")

    def test_averaging_never_normalizes_or_passes_accuracy(self):
        rows = window()
        for row in rows:
            row.update(az_g=0.87, norm_g=0.87)
        result = analyze(rows, 15)
        self.assertEqual(result["status"], "RECORDED")
        self.assertEqual(result["accuracy_status"], "NOT_VALIDATED")
        self.assertAlmostEqual(result["statistics"]["norm_g"]["mean"], 0.87)
        self.assertEqual(result["statistics"]["norm_g"]["stddev"], 0)

    def test_failures_remain_review_after_numeric_recovery(self):
        rows = window()
        rows[10] = {"host_elapsed_s": 2.2, **sample(2200, "nan,nan,nan", "nan", "READ_FAILED")}
        result = analyze(rows, 15)
        self.assertEqual(result["status"], "REVIEW")
        self.assertEqual(result["unavailable_samples"], 1)

    def test_reset_duplicate_and_rollover(self):
        rows = window()
        rows[20]["device_t_ms"] = rows[19]["device_t_ms"]
        result = analyze(rows, 15)
        self.assertEqual(result["duplicate_timestamps"], 1)
        rows[20]["device_t_ms"] = 1
        self.assertEqual(analyze(rows, 15)["observed_timestamp_restarts"], 1)
        pair = [{"host_elapsed_s": 0, **sample(0xFFFFFFF0)},
                {"host_elapsed_s": 0.22, **sample(204)}]
        self.assertEqual(analyze(pair, 15)["observed_timestamp_restarts"], 0)

    def test_fragmented_bytes_and_trailing_boundary(self):
        collector = Collector()
        collector.feed(b"mpu6050_meas", 0)
        collector.feed(b"ure,1,HW_SAMPLE,0,0,1,1\r\n#note\nmpu", 0.1)
        self.assertEqual(len(collector.records), 1)
        self.assertEqual(collector.comments, 1)
        self.assertEqual(collector.pending, b"mpu")
        self.assertEqual(collector.malformed, 0)

    def test_invalid_utf8_and_unexpected_lines_are_not_hidden(self):
        collector = Collector()
        collector.feed(b"\xff\nrandom\n", 0)
        self.assertEqual(collector.malformed, 2)

    def test_opening_fragment_burst_remains_review_before_clean_samples(self):
        collector = Collector()
        fragment = b",-0.1987,-0.1912,0.8109,0.8565\n"
        fused = b"mpu6050_measure,901069,HW_SAMPLE" * 35 + b"\n"
        collector.feed(fragment * 1012 + fused, 0.1)
        for index in range(68):
            line = f"mpu6050_measure,{index * 220},HW_SAMPLE,0,0,1,1\n"
            collector.feed(line.encode(), index * 0.22)
        self.assertEqual(collector.malformed, 1013)
        self.assertEqual(len(collector.records), 68)
        result = analyze(collector.records, 15, malformed=collector.malformed)
        self.assertEqual(result["status"], "REVIEW")
        self.assertEqual(result["malformed_lines"], 1013)

    def test_incomplete_sparse_and_malformed_are_review(self):
        self.assertEqual(analyze([], 15)["status"], "REVIEW")
        self.assertEqual(analyze(window(), 15, complete=False)["status"], "REVIEW")
        self.assertEqual(analyze(window(), 15, malformed=1)["status"], "REVIEW")
        rows = window()[10:-10]
        self.assertEqual(analyze(rows, 15)["status"], "REVIEW")

    def test_open_failure_saved_without_reset_or_flash(self):
        class FailedPort:
            def __init__(self, **kwargs):
                self.dtr = self.rts = True

            def open(self):
                if self.dtr or self.rts:
                    raise AssertionError("Controls not inactive before opening")
                raise OSError("test port unavailable")

            def close(self):
                pass

        with tempfile.TemporaryDirectory() as directory:
            args = SimpleNamespace(port="TEST", pose="z_up", seconds=15, output_root=directory)
            with patch.dict("sys.modules", {"serial": SimpleNamespace(Serial=FailedPort)}), redirect_stdout(io.StringIO()):
                self.assertEqual(capture(args), 2)
            folder = next(Path(directory).iterdir())
            result = json.loads((folder / "summary.json").read_text())
            self.assertEqual(result["status"], "REVIEW")
            self.assertFalse(result["capture_complete"])
            self.assertIn("test port unavailable", " ".join(result["review_reasons"]))
            self.assertTrue((folder / "raw.log").exists())

    def test_complete_fake_capture_retains_raw_values_and_unique_folders(self):
        class FakePort:
            def __init__(self, **kwargs):
                self.stamp = 0

            def open(self):
                if self.dtr or self.rts:
                    raise AssertionError("Controls not inactive before opening")

            def read(self, size):
                self.stamp += 220
                return f"mpu6050_measure,{self.stamp},HW_SAMPLE,0,0,0.87,0.87\n".encode()

            def close(self):
                pass

        clock = [-0.11]

        def monotonic():
            clock[0] += 0.11
            return clock[0]

        with tempfile.TemporaryDirectory() as directory:
            args = SimpleNamespace(port="TEST", pose="z_up", seconds=15, output_root=directory)
            with patch.dict("sys.modules", {"serial": SimpleNamespace(Serial=FakePort)}), \
                    patch("capture_mpu_pose.time.monotonic", side_effect=monotonic), \
                    redirect_stdout(io.StringIO()):
                self.assertEqual(capture(args), 0)
                self.assertEqual(capture(args), 0)
            folders = list(Path(directory).iterdir())
            self.assertEqual(len(folders), 2)
            for folder in folders:
                result = json.loads((folder / "summary.json").read_text())
                records = json.loads((folder / "samples.json").read_text())
                self.assertEqual(result["status"], "RECORDED")
                self.assertEqual(result["accuracy_status"], "NOT_VALIDATED")
                self.assertTrue(result["capture_complete"])
                self.assertEqual(result["hardware_samples"], len(records))
                self.assertAlmostEqual(result["statistics"]["norm_g"]["mean"], 0.87)
                self.assertEqual((folder / "raw.log").read_bytes().count(b"HW_SAMPLE"), len(records))


if __name__ == "__main__":
    unittest.main()
