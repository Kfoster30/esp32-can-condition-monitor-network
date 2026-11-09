"""Non-flashing, single-pose MPU diagnostic capture; never certifies calibration."""

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import statistics
import time
import uuid


AXES = ("ax_g", "ay_g", "az_g", "norm_g")
POSES = ("z_up", "z_down", "x_up", "x_down", "y_up", "y_down")
HEADER = "type,t_ms,status,ax_g,ay_g,az_g,norm_g"
FAILURES = {"I2C_INIT_FAILED", "READ_FAILED", "ID_MISMATCH", "WRITE_FAILED",
            "RESET_NOT_COMPLETE", "SETTINGS_MISMATCH", "DATA_READY_TIMEOUT",
            "NEAR_FULL_SCALE"}


def parse_sample(line):
    """Keep explicit failures; reject nonfinite/unlabeled/inconsistent values."""
    if line.startswith("#") or line == HEADER:
        return None
    fields = line.split(",")
    if len(fields) != 7 or fields[0] != "mpu6050_measure":
        raise ValueError("Unexpected diagnostic record")
    stamp = int(fields[1])
    if not 0 <= stamp < 2**32:
        raise ValueError("Invalid device timestamp")
    status = fields[2]
    values = list(map(float, fields[3:]))
    if status == "HW_SAMPLE":
        if not all(math.isfinite(value) for value in values):
            raise ValueError("Nonfinite HW_SAMPLE")
        if any(abs(value) >= 4 for value in values[:3]) or values[3] < 0:
            raise ValueError("Outside configured +/-4 g range")
        if abs(math.sqrt(sum(value * value for value in values[:3])) - values[3]) > 0.0005:
            raise ValueError("Inconsistent reported norm")
    elif status in FAILURES and all(math.isnan(value) for value in values):
        values = [None] * 4
    else:
        raise ValueError("Unknown status or incorrectly labeled unavailable data")
    return {"device_t_ms": stamp, "status": status, **dict(zip(AXES, values))}


class Collector:
    def __init__(self):
        self.pending = b""
        self.records = []
        self.malformed = 0
        self.comments = 0

    def feed(self, chunk, elapsed):
        self.pending += chunk
        while b"\n" in self.pending:
            line, self.pending = self.pending.split(b"\n", 1)
            try:
                parsed = parse_sample(line.rstrip(b"\r").decode("utf-8", errors="strict"))
                if parsed is not None:
                    self.records.append({"host_elapsed_s": elapsed, **parsed})
                else:
                    self.comments += 1
            except (ValueError, OverflowError):
                # Raw bytes remain intact; malformed lines never become samples.
                self.malformed += 1


def analyze(records, seconds, complete=True, malformed=0, error=None):
    good = [row for row in records if row["status"] == "HW_SAMPLE"]
    failed = len(records) - len(good)
    reasons = []
    if not complete:
        reasons.append("Capture incomplete")
    if error:
        reasons.append(error)
    if malformed:
        reasons.append(f"Malformed/unexpected/undecodable complete lines: {malformed}")
    if failed:
        reasons.append(f"Unavailable acquisitions: {failed}")
    if len(good) < max(10, math.ceil(seconds * 2)):
        reasons.append("Too few hardware samples for this pose window")
    resets = duplicates = 0
    intervals = []
    for before, after in zip(records, records[1:]):
        delta = (after["device_t_ms"] - before["device_t_ms"]) % 2**32
        if delta >= 2**31:
            resets += 1
        elif delta == 0:
            duplicates += 1
        else:
            intervals.append(delta)
    if resets or duplicates:
        reasons.append("Device timestamp reset/backstep or duplicate observed")
    if intervals and max(intervals) > 1000:
        reasons.append("Device record interval exceeded 1000 ms")
    if good and (good[0]["host_elapsed_s"] > 2 or
                 good[-1]["host_elapsed_s"] < seconds - 2):
        reasons.append("Hardware samples do not cover both capture boundaries")
    stats = {}
    for axis in AXES:
        values = [row[axis] for row in good]
        stats[axis] = (dict(mean=statistics.fmean(values), minimum=min(values),
                            maximum=max(values), stddev=statistics.pstdev(values))
                       if values else None)
    return {"status": "REVIEW" if reasons else "RECORDED",
            "accuracy_status": "NOT_VALIDATED", "review_reasons": reasons,
            "hardware_samples": len(good), "unavailable_samples": failed,
            "status_counts": dict(Counter(row["status"] for row in records)),
            "malformed_lines": malformed, "observed_timestamp_restarts": resets,
            "duplicate_timestamps": duplicates,
            "max_device_interval_ms": max(intervals) if intervals else None,
            "statistics": stats}


def capture(args):
    # A fresh directory prevents accidental replacement of a previous pose.
    started = datetime.now(timezone.utc)
    root = Path(args.output_root) if args.output_root else Path(__file__).resolve().parents[1] / "data"
    folder = root / (f"mpu-pose-{started:%Y%m%dT%H%M%SZ}-{args.pose}-" + uuid.uuid4().hex[:6])
    folder.mkdir(parents=True, exist_ok=False)
    print(f"Output folder: {folder}", flush=True)
    collector = Collector()
    port = None
    complete = False
    error = None
    elapsed = 0.0
    try:
        import serial
        # No reset/flash commands or explicit flush. Windows pyserial.open()
        # itself purges driver buffers; raw.log covers bytes returned after open.
        port = serial.Serial(port=None, baudrate=115200, timeout=0.1)
        port.dtr = False
        port.rts = False
        port.port = args.port
        with (folder / "raw.log").open("wb") as raw:
            port.open()
            begin = time.monotonic()
            while True:
                elapsed = time.monotonic() - begin
                if elapsed >= args.seconds:
                    complete = True
                    break
                chunk = port.read(256)
                elapsed = time.monotonic() - begin
                if chunk:
                    raw.write(chunk)
                    collector.feed(chunk, elapsed)
    except KeyboardInterrupt:
        error = "Interrupted by operator"
    except Exception as exc:
        error = f"Serial/capture error: {type(exc).__name__}: {exc}"
    finally:
        if port is not None:
            try:
                port.close()
            except Exception as exc:
                close_error = f"Serial close error: {type(exc).__name__}: {exc}"
                error = f"{error}; {close_error}" if error else close_error
    summary = analyze(collector.records, args.seconds, complete,
                      collector.malformed, error)
    summary.update(pose_label=args.pose, pose_geometry="OPERATOR_REPORTED_NOT_VERIFIED",
                   port=args.port, baud=115200, started_utc=started.isoformat(),
                   requested_seconds=args.seconds, elapsed_seconds=round(elapsed, 3),
                   capture_complete=complete, comment_header_lines=collector.comments,
                   final_partial_bytes=len(collector.pending),
                   limitations=["Gravity included; not vibration RMS or a complete internal-rate capture.",
                                "Screens recording quality, not accuracy, stationarity, or pose alignment.",
                                "No automatic offset/gain correction or vector normalization is applied.",
                                "No sequence/session metadata or device-image attestation in this diagnostic.",
                                "Initial partial lines remain raw and cause REVIEW if malformed; final partial excluded."])
    (folder / "samples.json").write_text(json.dumps(collector.records, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    (folder / "summary.json").write_text(json.dumps(summary, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Result: {summary['status']} (accuracy NOT VALIDATED)")
    print(f"Hardware samples: {summary['hardware_samples']}; unavailable: {summary['unavailable_samples']}")
    for axis, values in summary["statistics"].items():
        if values:
            print(f"  {axis}: mean={values['mean']:.5f}, stddev={values['stddev']:.5f}, "
                  f"range={values['minimum']:.5f}..{values['maximum']:.5f}")
    for reason in summary["review_reasons"]:
        print(f"REVIEW: {reason}")
    print(f"Summary: {folder / 'summary.json'}")
    return 0 if summary["status"] == "RECORDED" else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--pose", required=True, choices=POSES)
    parser.add_argument("--seconds", type=float, default=15)
    parser.add_argument("--output-root")
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 10 <= args.seconds <= 60:
        parser.error("--seconds must be finite and between 10 and 60")
    raise SystemExit(capture(args))


if __name__ == "__main__":
    main()
