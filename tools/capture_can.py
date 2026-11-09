"""Timed, non-flashing dashboard capture and telemetry-level stability checks."""

import argparse
import csv
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import subprocess
import time
import uuid

from can_protocol import (WARN_TEMPERATURE_MISSING, WARN_TEMPERATURE_STALE,
                          WARN_POWER_MISSING, WARN_MOTION_MISSING, FLAG_DHT_TEMPERATURE,
                          FLAG_INA_POWER, FLAG_MPU_MOTION, MOTION_QUALITY_MISSING,
                          MOTION_QUALITY_VALID, MOTION_UNAVAILABLE, valid_source_flags)
from sequence_tracking import SequenceTracker


PERIODS_MS = {"heartbeat": 1000, "condition": 200, "motion_power": 100}
COUNTER_MODULUS = 2**32


def counter_delta(current, previous):
    if current >= previous:
        return current - previous
    if previous > 0xF0000000 and current < 0x0FFFFFFF:
        return current + COUNTER_MODULUS - previous
    return None


def parse_telemetry(line):
    """Return fields for known telemetry; reject malformed known records."""
    original = line.split(",")
    kind = original[0]
    if kind not in PERIODS_MS:
        return None
    expected = 7 if kind == "condition" else 6
    if len(original) not in (expected, expected + 2):
        raise ValueError("Unexpected field count")
    fields = original[:expected]
    if len(original) == expected + 2:
        if not 0 <= int(original[-2]) <= 255:
            raise ValueError("Invalid sequence metadata")
        if original[-1] != "UNKNOWN" and not 0 <= int(original[-1]) < COUNTER_MODULUS:
            raise ValueError("Invalid session metadata")
    timestamp = int(fields[1])
    if not 0 <= timestamp < COUNTER_MODULUS:
        raise ValueError("Invalid dashboard timestamp")
    if kind == "heartbeat":
        version, role, flags, uptime = map(int, fields[2:])
        if version != 1 or role != 1 or not valid_source_flags(flags):
            raise ValueError("Unexpected heartbeat metadata")
        if not 0 <= uptime < COUNTER_MODULUS:
            raise ValueError("Invalid sensor uptime")
    elif kind == "condition":
        if fields[2] not in {"SELF_TEST", "RUN", "DEGRADED", "FAULT"}:
            raise ValueError("Invalid condition state")
        if any(not 0 <= int(value) <= 255 for value in fields[3:5]):
            raise ValueError("Invalid warning/fault bits")
        if not math.isfinite(float(fields[5])):
            raise ValueError("Invalid condition percentage")
        warnings = int(fields[3])
        if warnings & (WARN_POWER_MISSING | WARN_MOTION_MISSING) and fields[2] not in {"DEGRADED", "FAULT"}:
            raise ValueError("Missing power or motion must degrade the condition state")
        missing = bool(warnings & WARN_TEMPERATURE_MISSING)
        stale = bool(warnings & WARN_TEMPERATURE_STALE)
        temperature = float(fields[6])
        if missing:
            if stale or not math.isnan(temperature) or fields[2] not in {"DEGRADED", "FAULT"}:
                raise ValueError("Missing temperature must be labeled and nan")
        elif not math.isfinite(temperature):
            raise ValueError("Invalid unlabeled temperature")
        if stale and fields[2] not in {"DEGRADED", "FAULT"}:
            raise ValueError("Stale temperature must degrade the state")
    else:
        quality = int(fields[2])
        if not 0 <= quality <= 255:
            raise ValueError("Invalid motion metadata")
        if fields[3] == "nan":
            if quality != MOTION_QUALITY_MISSING:
                raise ValueError("Unavailable motion requires missing quality")
        elif not 0 <= int(fields[3]) <= MOTION_UNAVAILABLE:
            raise ValueError("Motion outside the wire range")
        bus, current = map(float, fields[4:])
        if not (math.isnan(bus) and math.isnan(current)):
            if (not math.isfinite(bus) or not math.isfinite(current) or
                    not 0 <= bus <= 65.534 or not -3276.7 <= current <= 3276.7):
                raise ValueError("Invalid or half-missing power values")
    return original


def csv_record(fields, elapsed):
    base_count = 7 if fields[0] == "condition" else 6
    core = fields[:base_count]
    metadata = fields[base_count:] if len(fields) > base_count else ["", ""]
    return [f"{elapsed:.3f}"] + core + [""] * (7 - len(core)) + metadata


class Analyzer:
    def __init__(self, require_hardware_temperature=False, require_sequences=False,
                 require_hardware_power=False, require_hardware_motion=False):
        self.streams = {
            kind: {"count": 0, "interval_count": 0, "interval_sum_ms": 0,
                   "max_interval_ms": 0, "late_intervals": 0,
                   "first_host_s": None, "last_host_s": None, "last_t_ms": None}
            for kind in PERIODS_MS
        }
        self.last_dashboard_ms = None
        self.last_sensor_uptime = None
        self.dashboard_restarts = 0
        self.sensor_restarts = 0
        self.startup_banners = 0
        self.warning_error_lines = 0
        self.malformed_lines = 0
        self.decode_errors = 0
        self.other_lines = 0
        self.sensor_role_seen = False
        self.events = []
        self.require_hardware_temperature = require_hardware_temperature
        self.heartbeat_flags_seen = set()
        self.firmware_versions_observed = set()
        self.temperature_missing_records = 0
        self.temperature_stale_records = 0
        self.require_hardware_power = require_hardware_power
        self.power_missing_records = 0
        self.power_missing_condition_records = 0
        self.require_hardware_motion = require_hardware_motion
        self.motion_missing_records = 0
        self.motion_missing_condition_records = 0
        self.motion_valid_records = 0
        self.motion_min_mg = None
        self.motion_max_mg = None
        self.unexpected_lines = 0
        self.require_sequences = require_sequences
        self.sequence_trackers = {kind: SequenceTracker(period) for kind, period in PERIODS_MS.items()}
        self.sequence_events = []
        self.session_announcements = []

    def add_line(self, line, elapsed):
        line = line.strip()
        if not line:
            return None
        if line.startswith("#session,"):
            fields = line.split(",")
            try:
                if (len(fields) != 4 or fields[3] not in {"INITIAL", "CHANGED"} or
                        not 0 <= int(fields[1]) < COUNTER_MODULUS or
                        not 0 <= int(fields[2]) < COUNTER_MODULUS):
                    raise ValueError("Invalid session announcement")
                if len(self.session_announcements) < 100:
                    self.session_announcements.append({"device_t_ms": int(fields[1]),
                                                       "session": int(fields[2]), "kind": fields[3]})
            except ValueError:
                self.malformed_lines += 1
            return None
        if line.startswith("#event,"):
            fields = line.split(",")
            if (len(fields) != 3 or fields[1] not in
                    {"data_timeout", "heartbeat_timeout"} or fields[2] not in {"0", "1"}):
                self.malformed_lines += 1
            else:
                self.events.append({"host_elapsed_s": round(elapsed, 3),
                                    "event": fields[1], "active": int(fields[2])})
            return None
        if line.startswith(("#warning,", "#error,", "E (")):
            self.warning_error_lines += 1
            return None
        if line.startswith("#project="):
            self.startup_banners += 1
            if ",firmware=" in line:
                self.firmware_versions_observed.add(line.rsplit(",firmware=", 1)[1])
            return None
        if line.startswith("#role=SENSOR"):
            self.sensor_role_seen = True
            return None
        try:
            fields = parse_telemetry(line)
        except (ValueError, OverflowError):
            self.malformed_lines += 1
            return None
        if fields is None:
            self.other_lines += 1
            if not line.startswith("#"):
                self.unexpected_lines += 1
            return None
        kind, timestamp = fields[0], int(fields[1])
        if kind == "condition":
            warnings = int(fields[3])
            self.temperature_missing_records += bool(warnings & WARN_TEMPERATURE_MISSING)
            self.temperature_stale_records += bool(warnings & WARN_TEMPERATURE_STALE)
            self.power_missing_condition_records += bool(warnings & WARN_POWER_MISSING)
            self.motion_missing_condition_records += bool(warnings & WARN_MOTION_MISSING)
        if kind == "motion_power":
            self.power_missing_records += math.isnan(float(fields[4]))
            if fields[3] == "nan":
                self.motion_missing_records += 1
            elif int(fields[2]) == MOTION_QUALITY_VALID and int(fields[3]) < MOTION_UNAVAILABLE:
                value = int(fields[3])
                self.motion_valid_records += 1
                self.motion_min_mg = value if self.motion_min_mg is None else min(self.motion_min_mg, value)
                self.motion_max_mg = value if self.motion_max_mg is None else max(self.motion_max_mg, value)
        if (self.last_dashboard_ms is not None and
                counter_delta(timestamp, self.last_dashboard_ms) is None):
            self.dashboard_restarts += 1
            for stream in self.streams.values():
                stream["last_t_ms"] = None
            for tracker in self.sequence_trackers.values():
                tracker.clear_anchor()
        self.last_dashboard_ms = timestamp
        stream = self.streams[kind]
        if stream["last_t_ms"] is not None:
            interval = counter_delta(timestamp, stream["last_t_ms"])
            stream["interval_count"] += 1
            stream["interval_sum_ms"] += interval
            stream["max_interval_ms"] = max(stream["max_interval_ms"], interval)
            if interval > PERIODS_MS[kind] * 1.5:
                stream["late_intervals"] += 1
        if kind == "heartbeat":
            self.heartbeat_flags_seen.add(int(fields[4]))
        stream["count"] += 1
        if stream["first_host_s"] is None:
            stream["first_host_s"] = elapsed
        stream["last_host_s"] = elapsed
        stream["last_t_ms"] = timestamp
        sequence_event = None
        base_count = 7 if kind == "condition" else 6
        if len(fields) == base_count + 2:
            sequence = int(fields[-2])
            session = None if fields[-1] == "UNKNOWN" else int(fields[-1])
            sequence_event = self.sequence_trackers[kind].observe(sequence, session, timestamp)
            if sequence_event not in {"BASELINE", "IN_ORDER"} and len(self.sequence_events) < 100:
                self.sequence_events.append({"stream": kind, "device_t_ms": timestamp,
                                             "sequence": sequence, "session": session, "event": sequence_event})
        if kind == "heartbeat" and sequence_event not in {"DUPLICATE", "LATE_OR_OUT_OF_ORDER"}:
            uptime = int(fields[5])
            # Sender uptime is millis()/1000, so its natural wrap is ~49.7 days,
            # not 2**32 seconds. This is still an inferred restart indicator.
            near_millis_wrap = (self.last_sensor_uptime is not None and
                                4294957 <= self.last_sensor_uptime <= 4294967 and uptime <= 10)
            if (self.last_sensor_uptime is not None and not near_millis_wrap and
                    counter_delta(uptime, self.last_sensor_uptime) is None):
                self.sensor_restarts += 1
            self.last_sensor_uptime = uptime
        return fields

    def summary(self, elapsed, requested, complete, serial_error=None):
        reasons = []
        if not complete:
            reasons.append("Capture did not complete its requested duration.")
        if serial_error:
            reasons.append("Serial connection failed: " + serial_error)
        if self.sensor_role_seen:
            reasons.append("Sensor role detected: use the dashboard serial port.")
        if any(event["active"] for event in self.events):
            reasons.append("At least one communication timeout asserted.")
        if self.temperature_missing_records or self.temperature_stale_records:
            reasons.append("Temperature had missing or stale records during capture.")
        if self.power_missing_records or self.power_missing_condition_records:
            reasons.append("Power readings were unavailable during capture.")
        if self.motion_missing_records or self.motion_missing_condition_records:
            reasons.append("Motion readings were unavailable during capture.")
        if self.require_hardware_temperature and (not self.heartbeat_flags_seen or
                any(not flags & FLAG_DHT_TEMPERATURE for flags in self.heartbeat_flags_seen)):
            reasons.append("Expected DHT11 hardware mode (source flag bit 1) throughout capture.")
        if self.require_hardware_power and (not self.heartbeat_flags_seen or
                any(not flags & FLAG_INA_POWER for flags in self.heartbeat_flags_seen)):
            reasons.append("Expected INA219 hardware mode (source flag bit 2) throughout capture.")
        if self.require_hardware_motion:
            if not self.heartbeat_flags_seen or any(
                    not flags & FLAG_MPU_MOTION for flags in self.heartbeat_flags_seen):
                reasons.append("Expected MPU6050 hardware mode (source flag bit 3) throughout capture.")
            if self.motion_valid_records + self.motion_missing_records != self.streams["motion_power"]["count"]:
                reasons.append("Some motion records had invalid hardware quality or values.")
        sequence_streams = {}
        sequence_anomaly = False
        sequence_covered = True
        for kind, tracker in self.sequence_trackers.items():
            stats = tracker.summary()
            stats["missing_metadata"] = self.streams[kind]["count"] - tracker.samples
            sequence_streams[kind] = stats
            if tracker.samples < 2 or stats["missing_metadata"] or tracker.unattributed:
                sequence_covered = False
            for metric in ("inferred_missing", "duplicates", "late_or_out_of_order",
                           "ambiguous_rebases", "session_changes", "unattributed"):
                if stats[metric]:
                    sequence_anomaly = True
                    reasons.append(f"{kind} sequence {metric}: {stats[metric]}.")
        sequence_status = "REVIEW" if sequence_anomaly else "PASS" if sequence_covered else "UNVERIFIED"
        if self.require_sequences and sequence_status != "PASS":
            reasons.append("Required sequence check did not pass (missing metadata, unknown session, or anomaly).")
        for attr in ("dashboard_restarts", "sensor_restarts", "startup_banners",
                     "warning_error_lines", "malformed_lines", "decode_errors", "unexpected_lines"):
            if getattr(self, attr):
                reasons.append(f"Observed {attr}: {getattr(self, attr)}.")
        streams = {}
        for kind, stream in self.streams.items():
            intervals = stream["interval_count"]
            mean = stream["interval_sum_ms"] / intervals if intervals else None
            first, last = stream["first_host_s"], stream["last_host_s"]
            coverage = last - first if first is not None else 0
            trailing = elapsed - last if last is not None else elapsed
            rate = 1000 / mean if mean else None
            streams[kind] = {
                "count": stream["count"], "expected_period_ms": PERIODS_MS[kind],
                "mean_period_ms": round(mean, 3) if mean is not None else None,
                "observed_rate_hz": round(rate, 4) if rate is not None else None,
                "max_interval_ms": stream["max_interval_ms"],
                "late_intervals": stream["late_intervals"],
                "host_coverage_s": round(coverage, 3),
                "first_host_s": first, "last_host_s": last,
            }
            if mean is None or abs(mean - PERIODS_MS[kind]) > PERIODS_MS[kind] * 0.1:
                reasons.append(f"{kind}: mean period missing or outside +/-10%.")
            if stream["late_intervals"]:
                reasons.append(f"{kind}: intervals exceeded 1.5 times the target period.")
            if stream["count"] < requested * 1000 / PERIODS_MS[kind] * 0.9:
                reasons.append(f"{kind}: fewer than 90% of expected records captured.")
            # Host timestamps catch prolonged serial stalls or silence at either edge.
            edge_limit = max(2.0, 2 * PERIODS_MS[kind] / 1000)
            if first is None or first > edge_limit or trailing > edge_limit:
                reasons.append(f"{kind}: missing traffic near a capture boundary.")
        return {
            "status": "PASS" if not reasons else "REVIEW",
            "requested_seconds": requested, "elapsed_seconds": round(elapsed, 3),
            "capture_complete": complete, "streams": streams,
            "timeout_events": self.events, "dashboard_restarts": self.dashboard_restarts,
            "sensor_restarts": self.sensor_restarts, "startup_banners": self.startup_banners,
            "warning_error_lines": self.warning_error_lines,
            "malformed_lines": self.malformed_lines, "decode_errors": self.decode_errors,
            "other_lines": self.other_lines, "review_reasons": reasons,
            "unexpected_lines": self.unexpected_lines,
            "heartbeat_flags_seen": sorted(self.heartbeat_flags_seen),
            "firmware_versions_observed": sorted(self.firmware_versions_observed),
            "temperature_missing_records": self.temperature_missing_records,
            "temperature_stale_records": self.temperature_stale_records,
            "require_hardware_temperature": self.require_hardware_temperature,
            "power_missing_records": self.power_missing_records,
            "power_missing_condition_records": self.power_missing_condition_records,
            "require_hardware_power": self.require_hardware_power,
            "motion_missing_records": self.motion_missing_records,
            "motion_missing_condition_records": self.motion_missing_condition_records,
            "motion_min_mg": self.motion_min_mg,
            "motion_max_mg": self.motion_max_mg,
            "require_hardware_motion": self.require_hardware_motion,
            "sequence_check": {"status": sequence_status, "required": self.require_sequences,
                               "streams": sequence_streams, "events_first_100": self.sequence_events,
                               "session_announcements_first_100": self.session_announcements},
            "limitations": [
                "Measures decoded serial telemetry, not raw CAN frames or electrical signals.",
                "Sequence checks require 0.3+ sequence/session metadata; older serial formats remain unverified.",
                "Missing counts infer publication-sequence skips, not physical CAN error locations or causes.",
                "Modulo-256 classification assumes one ordered publisher; half-range jumps and long silence are ambiguous.",
                "32-bit boot-session identifiers are diagnostic, not authenticated or guaranteed collision-free.",
                "Hardware-source flags describe firmware mode; physical plausibility/reference checks remain manual.",
                "INA219 current is the resistor-limited LED branch, not ESP32/USB total current.",
                "Sensor restarts are inferred from decreasing heartbeat uptime; short unobserved restarts may be missed.",
                "Timing tolerances are bench screening limits, not hard real-time certification.",
            ],
        }


def source_commit(project):
    try:
        return subprocess.check_output(
            ["git", "-c", f"safe.directory={project.as_posix()}", "-C", str(project),
             "rev-parse", "HEAD"], text=True, stderr=subprocess.DEVNULL,
            timeout=5).strip()
    except (OSError, subprocess.SubprocessError):
        return "unavailable"


def capture(args):
    try:
        import serial
    except ImportError:
        print("Missing pyserial. Run this with the PlatformIO Python interpreter.")
        return 1
    project = Path(__file__).resolve().parents[1]
    start_utc = datetime.now(timezone.utc)
    name = start_utc.strftime("can-soak-%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:6]
    output = Path(args.output_root).resolve() / name if args.output_root else project / "data" / name
    output.mkdir(parents=True, exist_ok=False)
    analyzer = Analyzer(getattr(args, "require_hardware_temperature", False),
                        getattr(args, "require_sequences", False),
                        getattr(args, "require_hardware_power", False),
                        getattr(args, "require_hardware_motion", False))
    complete, serial_error = False, None
    port = serial.Serial(port=None, baudrate=115200, timeout=0.25)
    # Set line states BEFORE opening; do not intentionally reset or enter bootloader.
    port.dtr = False
    port.rts = False
    port.port = args.port
    start = time.monotonic()
    print(f"Output folder: {output}", flush=True)
    try:
        port.open()
        start = time.monotonic()
        with (output / "raw.log").open("wb") as raw, (output / "telemetry.csv").open(
                "w", newline="", encoding="utf-8") as csv_file:
            writer = csv.writer(csv_file)
            writer.writerow(["host_elapsed_s", "type", "device_t_ms", "field_1",
                             "field_2", "field_3", "field_4", "field_5", "sequence", "session"])
            pending = bytearray()
            next_progress = 30
            while time.monotonic() - start < args.seconds:
                chunk = port.read(min(max(port.in_waiting, 1), 4096))
                elapsed = time.monotonic() - start
                if chunk:
                    raw.write(chunk)
                    pending.extend(chunk)
                while b"\n" in pending:
                    record, _, remaining = pending.partition(b"\n")
                    pending = bytearray(remaining)
                    try:
                        line = record.decode("utf-8")
                    except UnicodeDecodeError:
                        analyzer.decode_errors += 1
                        continue
                    fields = analyzer.add_line(line, elapsed)
                    if fields is not None:
                        writer.writerow(csv_record(fields, elapsed))
                if len(pending) > 4096:
                    analyzer.malformed_lines += 1
                    pending.clear()
                if elapsed >= next_progress:
                    raw.flush()
                    csv_file.flush()
                    counts = ", ".join(f"{kind}={stream['count']}" for kind, stream in analyzer.streams.items())
                    print(f"{elapsed:.0f}/{args.seconds:g} seconds: {counts}", flush=True)
                    next_progress += 30
            # One partial final record is normal at a fixed-duration boundary.
            complete = True
    except KeyboardInterrupt:
        print("Stopped early; saving an incomplete summary.", flush=True)
    except (serial.SerialException, OSError) as exc:
        serial_error = str(exc)
        print(f"Capture error: {exc}", flush=True)
    finally:
        port.close()
    elapsed = time.monotonic() - start
    report = analyzer.summary(elapsed, args.seconds, complete, serial_error)
    report.update({"port": args.port, "baud": 115200,
                   "started_utc": start_utc.isoformat(),
                   "local_source_commit": source_commit(project)})
    (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"\nResult: {report['status']}")
    for kind, stream in report["streams"].items():
        print(f"  {kind}: {stream['count']} records; mean={stream['mean_period_ms']} ms; "
              f"max gap={stream['max_interval_ms']} ms")
    for reason in report["review_reasons"]:
        print("  REVIEW: " + reason)
    print(f"Power unavailable: {report['power_missing_records']} motion/power records, "
          f"{report['power_missing_condition_records']} condition warnings")
    if report["require_hardware_motion"] or report["motion_missing_records"]:
        print(f"Motion: {report['motion_min_mg']}–{report['motion_max_mg']} mg, "
              f"{report['motion_missing_records']} unavailable records, "
              f"{report['motion_missing_condition_records']} condition warnings")
    print("Sequence check: " + report["sequence_check"]["status"])
    for kind, stats in report["sequence_check"]["streams"].items():
        print(f"  {kind}: missing={stats['inferred_missing']}, duplicates={stats['duplicates']}, "
              f"late={stats['late_or_out_of_order']}, ambiguous={stats['ambiguous_rebases']}, "
              f"unattributed={stats['unattributed']}, session_changes={stats['session_changes']}")
    print(f"Summary: {output / 'summary.json'}", flush=True)
    return 0 if report["status"] == "PASS" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Identify the physical dashboard's current serial port")
    parser.add_argument("--seconds", type=float, default=600, help="Capture duration (default: 600)")
    parser.add_argument("--output-root", help="Optional directory for uniquely named capture folders")
    parser.add_argument("--require-hardware-temperature", action="store_true",
                        help="Require DHT11 mixed-source mode and healthy temperature records")
    parser.add_argument("--require-sequences", action="store_true",
                        help="Require sequence/session metadata on every telemetry record and no anomalies")
    parser.add_argument("--require-hardware-power", action="store_true",
                        help="Require INA219 mode and no unavailable power records")
    parser.add_argument("--require-hardware-motion", action="store_true",
                        help="Require MPU6050 mode and no unavailable motion records")
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or args.seconds < 5:
        parser.error("--seconds must be a finite number of at least 5")
    raise SystemExit(capture(args))


if __name__ == "__main__":
    main()
