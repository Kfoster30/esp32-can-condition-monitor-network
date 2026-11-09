import unittest

from sequence_tracking import SequenceTracker
from capture_can import Analyzer, csv_record, parse_telemetry
from can_protocol import Session


def healthy_sequence_run(seconds=10):
    analyzer = Analyzer(require_hardware_temperature=True, require_sequences=True)
    for tick in range(seconds * 10):
        timestamp = 10000 + tick * 100
        elapsed = tick / 10
        if tick % 10 == 0:
            analyzer.add_line(f"heartbeat,{timestamp},1,1,3,{100 + tick // 10},{tick // 10 % 256},1234", elapsed)
        if tick % 2 == 0:
            analyzer.add_line(f"condition,{timestamp},RUN,0,0,90.00,24.0,{tick // 2 % 256},1234", elapsed)
        analyzer.add_line(f"motion_power,{timestamp},100,125,3.300,4.2,{tick % 256},1234", elapsed)
    return analyzer


class SequenceTests(unittest.TestCase):
    def test_in_order_and_wrap(self):
        tracker = SequenceTracker(100)
        events = [tracker.observe(value, 1, index * 100)
                  for index, value in enumerate((254, 255, 0, 1))]
        self.assertEqual(events, ["BASELINE", "IN_ORDER", "IN_ORDER", "IN_ORDER"])
        self.assertEqual(tracker.wraps, 1)
        self.assertEqual(tracker.inferred_missing, 0)

    def test_gap_and_duplicate_do_not_double_count(self):
        tracker = SequenceTracker(100)
        tracker.observe(10, 1, 0)
        self.assertEqual(tracker.observe(14, 1, 400), "GAP")
        self.assertEqual(tracker.observe(14, 1, 401), "DUPLICATE")
        self.assertEqual(tracker.observe(15, 1, 500), "IN_ORDER")
        self.assertEqual(tracker.inferred_missing, 3)
        self.assertEqual(tracker.duplicates, 1)

    def test_late_record_does_not_move_anchor(self):
        tracker = SequenceTracker(100)
        tracker.observe(14, 1, 0)
        self.assertEqual(tracker.observe(13, 1, 1), "LATE_OR_OUT_OF_ORDER")
        self.assertEqual(tracker.observe(15, 1, 100), "IN_ORDER")
        self.assertEqual(tracker.inferred_missing, 0)

    def test_session_change_is_not_gap(self):
        tracker = SequenceTracker(100)
        tracker.observe(50, 123, 0)
        self.assertEqual(tracker.observe(0, 456, 600), "BASELINE")
        self.assertEqual(tracker.observe(1, 456, 700), "IN_ORDER")
        self.assertEqual(tracker.session_changes, 1)
        self.assertEqual(tracker.inferred_missing, 0)

    def test_unknown_session_invalidates_anchor(self):
        tracker = SequenceTracker(100)
        tracker.observe(10, 123, 0)
        self.assertEqual(tracker.observe(11, None, 100), "UNATTRIBUTED")
        self.assertEqual(tracker.observe(14, 123, 400), "BASELINE")
        self.assertEqual(tracker.unattributed, 1)
        self.assertEqual(tracker.inferred_missing, 0)

    def test_half_range_is_ambiguous(self):
        tracker = SequenceTracker(100)
        tracker.observe(0, 1, 0)
        self.assertEqual(tracker.observe(128, 1, 100), "AMBIGUOUS")
        self.assertEqual(tracker.inferred_missing, 0)
        self.assertEqual(tracker.ambiguous_rebases, 1)

    def test_long_silence_is_not_claimed_as_zero_loss(self):
        tracker = SequenceTracker(100)
        tracker.observe(0, 1, 0)
        self.assertEqual(tracker.observe(1, 1, 12800), "AMBIGUOUS")
        self.assertEqual(tracker.inferred_missing, 0)
        self.assertEqual(tracker.ambiguous_rebases, 1)

    def test_timestamp_wrap(self):
        tracker = SequenceTracker(100)
        tracker.observe(255, 0, 4294967200)
        self.assertEqual(tracker.observe(0, 0, 4), "IN_ORDER")
        self.assertEqual(tracker.wraps, 1)

    def test_invalid_inputs(self):
        for seq, session, timestamp in ((256, 1, 0), (-1, 1, 0), (1, -1, 0), (1, 1, -1)):
            with self.assertRaises(ValueError):
                SequenceTracker(100).observe(seq, session, timestamp)

    def test_healthy_capture_with_multiple_wraps(self):
        report = healthy_sequence_run(60).summary(60, 60, True)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["sequence_check"]["status"], "PASS")
        self.assertEqual(report["sequence_check"]["streams"]["motion_power"]["wraps"], 2)

    def test_old_format_remains_unverified(self):
        from test_capture_can import healthy_run
        analyzer = healthy_run()
        report = analyzer.summary(10, 10, True)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["sequence_check"]["status"], "UNVERIFIED")
        analyzer.require_sequences = True
        self.assertEqual(analyzer.summary(10, 10, True)["status"], "REVIEW")

    def test_gap_capture_requires_review(self):
        analyzer = healthy_sequence_run()
        analyzer.add_line("motion_power,20000,100,125,3.300,4.2,101,1234", 10)
        report = analyzer.summary(10.1, 10.1, True)
        self.assertEqual(report["sequence_check"]["streams"]["motion_power"]["inferred_missing"], 1)
        self.assertEqual(report["status"], "REVIEW")

    def test_unknown_session_capture_requires_review(self):
        analyzer = healthy_sequence_run()
        analyzer.add_line("motion_power,20000,100,125,3.300,4.2,100,UNKNOWN", 10)
        report = analyzer.summary(10.1, 10.1, True)
        self.assertEqual(report["sequence_check"]["streams"]["motion_power"]["unattributed"], 1)
        self.assertEqual(report["status"], "REVIEW")

    def test_dashboard_reset_rebaselines_without_gap_inflation(self):
        analyzer = healthy_sequence_run()
        analyzer.add_line("motion_power,500,100,125,3.300,4.2,100,1234", 10)
        report = analyzer.summary(10.1, 10.1, True)
        self.assertEqual(report["dashboard_restarts"], 1)
        self.assertEqual(report["sequence_check"]["streams"]["motion_power"]["inferred_missing"], 0)

    def test_session_transition_reports_separately(self):
        analyzer = healthy_sequence_run()
        analyzer.add_line("motion_power,20000,100,125,3.300,4.2,0,5678", 10)
        report = analyzer.summary(10.1, 10.1, True)
        stats = report["sequence_check"]["streams"]["motion_power"]
        self.assertEqual(stats["session_changes"], 1)
        self.assertEqual(stats["inferred_missing"], 0)
        self.assertEqual(report["status"], "REVIEW")

    def test_late_heartbeat_does_not_invent_restart(self):
        analyzer = Analyzer()
        analyzer.add_line("heartbeat,10000,1,1,3,100,10,1234", 0)
        analyzer.add_line("heartbeat,11000,1,1,3,99,9,1234", 1)
        self.assertEqual(analyzer.sensor_restarts, 0)
        self.assertEqual(analyzer.sequence_trackers["heartbeat"].late_or_out_of_order, 1)

    def test_sensor_millis_uptime_wrap_is_not_restart(self):
        analyzer = Analyzer()
        analyzer.add_line("heartbeat,10000,1,1,3,4294967,10,1234", 0)
        analyzer.add_line("heartbeat,11000,1,1,3,0,11,1234", 1)
        self.assertEqual(analyzer.sensor_restarts, 0)

    def test_metadata_csv_columns_and_validation(self):
        for line in ("heartbeat,100,1,1,3,1,1,0",
                     "condition,100,RUN,0,0,90.00,24.0,1,1234",
                     "motion_power,100,100,125,3.300,4.2,1,UNKNOWN"):
            fields = parse_telemetry(line)
            self.assertEqual(len(csv_record(fields, 0)), 10)
            self.assertEqual(csv_record(fields, 0)[-2:], fields[-2:])
        for suffix in ("256,1", "1,-1", "1,4294967296", "1,garbage"):
            with self.assertRaises(ValueError):
                parse_telemetry("motion_power,100,100,125,3.300,4.2," + suffix)

    def test_session_announcements(self):
        analyzer = Analyzer()
        analyzer.add_line("#session,1234,5678,INITIAL", 0)
        analyzer.add_line("#session,2234,9999,CHANGED", 1)
        analyzer.add_line("#session,1234,9999,BOGUS", 2)
        self.assertEqual(len(analyzer.session_announcements), 2)
        self.assertEqual(analyzer.malformed_lines, 1)

    def test_session_golden_vector(self):
        message = Session(1, 1, 0x12345678, 3)
        self.assertEqual(message.encode().hex(), "0101785634120300")
        self.assertEqual(Session.decode(message.encode()), message)

    def test_session_rejects_reserved_and_wrong_metadata(self):
        for payload in (bytes.fromhex("0101785634120301"),
                        bytes.fromhex("0201785634120300"),
                        bytes.fromhex("0102785634120300"),
                        bytes.fromhex("0101785634120200"), b"short"):
            with self.assertRaises(ValueError):
                Session.decode(payload)


if __name__ == "__main__":
    unittest.main()
