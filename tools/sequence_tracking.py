"""Bounded modulo-256 sequence analysis for one ordered publisher per stream.

Counts inferred missing publication sequences, not physical CAN errors. Long
silences, half-range jumps, and missing boot-session attribution are uncertainty,
not invented loss. A session change rebaselines rather than creating a gap.
"""


def elapsed_ms(current, previous):
    if current >= previous:
        return current - previous
    if previous > 0xF0000000 and current < 0x0FFFFFFF:
        return current + 2**32 - previous
    return None


class SequenceTracker:
    def __init__(self, period_ms):
        self.period_ms = period_ms
        self.session = None
        self.anchor = None
        self.accepted_at_ms = None
        self.samples = 0
        self.inferred_missing = 0
        self.duplicates = 0
        self.late_or_out_of_order = 0
        self.ambiguous_rebases = 0
        self.unattributed = 0
        self.session_changes = 0
        self.rebases = 0
        self.wraps = 0

    def clear_anchor(self):
        self.anchor = None
        self.accepted_at_ms = None

    def _accept(self, sequence, timestamp):
        self.anchor, self.accepted_at_ms = sequence, timestamp

    def observe(self, sequence, session, timestamp):
        if not 0 <= sequence <= 255 or not 0 <= timestamp < 2**32:
            raise ValueError("Invalid sequence or device timestamp")
        if session is not None and not 0 <= session < 2**32:
            raise ValueError("Invalid boot-session identifier")
        self.samples += 1
        if session is None:
            self.unattributed += 1
            self.clear_anchor()
            return "UNATTRIBUTED"
        if self.session != session:
            if self.session is not None:
                self.session_changes += 1
            self.session = session
            self.clear_anchor()
        if self.anchor is None:
            self.rebases += 1
            self._accept(sequence, timestamp)
            return "BASELINE"
        delta = (sequence - self.anchor) & 0xFF
        age = elapsed_ms(timestamp, self.accepted_at_ms)
        if age is None or age >= self.period_ms * 128 or delta == 128:
            self.ambiguous_rebases += 1
            self.rebases += 1
            self._accept(sequence, timestamp)
            return "AMBIGUOUS"
        if delta == 0:
            self.duplicates += 1
            return "DUPLICATE"
        if delta > 128:
            self.late_or_out_of_order += 1
            return "LATE_OR_OUT_OF_ORDER"
        if sequence < self.anchor:
            self.wraps += 1
        self._accept(sequence, timestamp)
        if delta > 1:
            self.inferred_missing += delta - 1
            return "GAP"
        return "IN_ORDER"

    def summary(self):
        return {key: getattr(self, key) for key in (
            "samples", "inferred_missing", "duplicates", "late_or_out_of_order",
            "ambiguous_rebases", "unattributed", "session_changes", "rebases", "wraps")}
