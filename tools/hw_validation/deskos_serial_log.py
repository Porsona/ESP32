#!/usr/bin/env python3
"""CYD DeskOS hardware-validation Serial logger and log analyzer.

capture: reads the ESP32 Serial port (115200 baud), timestamps every line,
         REDACTS secrets before anything is printed or written, optionally sends
         the read-only command STATUS every N minutes, and analyzes the log at the end.
analyze: summarizes an existing log (ours or a `pio device monitor` capture).

Secrets policy: the PC Monitor token (32 hex), `--token ...`, `WIFI PASS ...`,
DESKOS_TOKEN=... and (unless --keep-ssid) the Wi-Fi SSID are replaced before
writing, so the log file can be shared. The analyzer never declares a hardware
test PASS: it only flags automatic checks; visual checks stay with the tester.

Capture needs pyserial, which ships with PlatformIO:
  %USERPROFILE%\\.platformio\\penv\\Scripts\\python.exe tools\\hw_validation\\deskos_serial_log.py capture --port COM5
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Iterable, Optional

# --- Redaction -----------------------------------------------------------------
_REDACTIONS = [
    (re.compile(r"(--token\s+)\S+", re.I), r"\1<REDACTED>"),
    (re.compile(r"(DESKOS_TOKEN\s*=\s*)\S+", re.I), r"\1<REDACTED>"),
    (re.compile(r"(WIFI\s+PASS\s+).*", re.I), r"\1<REDACTED>"),
    (re.compile(r"(?<![0-9A-Fa-f])[0-9A-Fa-f]{32}(?![0-9A-Fa-f])"), "<REDACTED-TOKEN>"),
]
_SSID_REDACTIONS = [
    (re.compile(r'(ssid=")[^"]*(")', re.I), r"\1<REDACTED>\2"),
    (re.compile(r"(WIFI\s+SSID\s+).*", re.I), r"\1<REDACTED>"),
]


def redact(line: str, keep_ssid: bool = False) -> str:
    for pattern, repl in _REDACTIONS + ([] if keep_ssid else _SSID_REDACTIONS):
        line = pattern.sub(repl, line)
    return line


def looks_secret(line: str) -> bool:
    """True when a line still contains something shaped like a token/password."""
    return bool(re.search(r"(?<![0-9A-Fa-f])[0-9A-Fa-f]{32}(?![0-9A-Fa-f])", line)
                or re.search(r"--token\s+(?!<REDACTED>)\S+", line, re.I)
                or re.search(r"WIFI\s+PASS\s+(?!<REDACTED>)\S+", line, re.I))


# --- Analysis --------------------------------------------------------------------
_PREFIX = re.compile(r"^(\[[^\]]*\]\s*|\d{2}:\d{2}:\d{2}\.\d{3}\s*>\s*)")
_KV = re.compile(r"(\w+)=(-?\d+)")
_RESET = re.compile(r"rst:0x|Brownout|Guru Meditation|abort\(\) was called|Task watchdog got triggered|"
                    r"CORRUPT HEAP|assert failed|Backtrace:|stack overflow", re.I)
_STORAGE_FAIL = re.compile(r"WRITE_OR_READBACK_FAIL|OPEN_WRITE_FAIL|SAVE_FAIL|CREATE_FAILED|"
                           r"INVALID_CANDIDATE|STORAGE_WRITE|STORAGE_READ")
WARMUP_MS = 5 * 60 * 1000
MIN_LARGEST_BLOCK = 45000      # TLS handshake needs roughly 40 KB contiguous.
HEAP_DECLINE_B_PER_H = -2048   # Sustained loss beyond this over >=30 min = leak suspect.


def _strip(line: str) -> str:
    return _PREFIX.sub("", line.rstrip("\r\n"))


def _slope_per_hour(points: list[tuple[int, int]]) -> Optional[float]:
    if len(points) < 3:
        return None
    n = len(points)
    mx = sum(p[0] for p in points) / n
    my = sum(p[1] for p in points) / n
    den = sum((p[0] - mx) ** 2 for p in points)
    if den == 0:
        return None
    return sum((p[0] - mx) * (p[1] - my) for p in points) / den * 3_600_000.0


def analyze_lines(lines: Iterable[str]) -> dict:
    s: dict = {"lines": 0, "boots": 0, "versions": [], "resets": [], "storage_failures": [],
               "memory_samples": 0, "free_heap_min": None, "largest_block_min": None, "min_free_heap": None,
               "heap_slope_b_per_h": None, "uptime_max_ms": 0,
               "https_ok": 0, "https_fail": 0, "https_ms_max": 0, "https_heap_min": None, "cert_date": {},
               "net_connected": 0, "net_disconnected": 0, "pc_rejects_logged": 0, "pc_last": None,
               "app_switches": 0, "mission_rewards": {}, "focus_last_xp": None, "secret_suspects": 0,
               "attention": []}
    post_warmup: list[tuple[int, int]] = []
    for raw in lines:
        line = _strip(raw)
        if not line:
            continue
        s["lines"] += 1
        if looks_secret(line):
            s["secret_suspects"] += 1
        if line.startswith("FIRMWARE_VERSION:"):
            s["boots"] += 1
            version = line.split(":", 1)[1].strip()
            if version not in s["versions"]:
                s["versions"].append(version)
        if _RESET.search(line):
            s["resets"].append(line[:120])
        if _STORAGE_FAIL.search(line) and "error=NONE" not in line:
            s["storage_failures"].append(line[:120])
        kv = {k: int(v) for k, v in _KV.findall(line)}
        if line.startswith("MEMORY:") and "uptime_ms" in kv:
            s["memory_samples"] += 1
            up, heap, block = kv["uptime_ms"], kv.get("free_heap"), kv.get("largest_free_block")
            s["uptime_max_ms"] = max(s["uptime_max_ms"], up)
            if heap is not None:
                s["free_heap_min"] = heap if s["free_heap_min"] is None else min(s["free_heap_min"], heap)
                if up >= WARMUP_MS:
                    post_warmup.append((up, heap))
            if block is not None:
                s["largest_block_min"] = block if s["largest_block_min"] is None else min(s["largest_block_min"], block)
            if "min_free_heap" in kv:
                s["min_free_heap"] = kv["min_free_heap"]
        elif line.startswith("HTTPS: code="):
            code = kv.get("code", -1)
            s["https_ok" if code == 200 else "https_fail"] += 1
            s["https_ms_max"] = max(s["https_ms_max"], kv.get("ms", 0))
            if "heap" in kv:
                h = kv["heap"]
                s["https_heap_min"] = h if s["https_heap_min"] is None else min(s["https_heap_min"], h)
            m = re.search(r"cert_date=(\w+)", line)
            if m:
                s["cert_date"][m.group(1)] = s["cert_date"].get(m.group(1), 0) + 1
        elif line.startswith("NET: CONNECTED"):
            s["net_connected"] += 1
        elif line.startswith("NET: DISCONNECTED"):
            s["net_disconnected"] += 1
        elif line.startswith("PC_LINK: REJECT"):
            s["pc_rejects_logged"] += 1
        elif line.startswith("PC: link="):
            s["pc_last"] = line[:160]
        elif line.startswith("APP_SWITCH:"):
            s["app_switches"] += 1
        elif line.startswith("MISSION_REWARD:"):
            m = re.search(r"result=(\w+)", line)
            if m:
                s["mission_rewards"][m.group(1)] = s["mission_rewards"].get(m.group(1), 0) + 1
        elif line.startswith("FOCUS: ") and "xp" in kv:
            s["focus_last_xp"] = kv["xp"]
    span = post_warmup[-1][0] - post_warmup[0][0] if len(post_warmup) >= 2 else 0
    slope = _slope_per_hour(post_warmup)
    s["heap_slope_b_per_h"] = None if slope is None else round(slope, 1)
    s["post_warmup_span_min"] = round(span / 60000, 1)
    if s["resets"]:
        s["attention"].append(f"{len(s['resets'])} reset/crash marker(s) (rst:/brownout/panic/WDT)")
    if s["boots"] > 1:
        s["attention"].append(f"{s['boots']} boots in one log: confirm each reboot was intentional")
    if s["storage_failures"]:
        s["attention"].append(f"{len(s['storage_failures'])} storage failure line(s)")
    if s["largest_block_min"] is not None and s["largest_block_min"] < MIN_LARGEST_BLOCK:
        s["attention"].append(f"largest_free_block fell to {s['largest_block_min']} B (< {MIN_LARGEST_BLOCK})")
    if slope is not None and span >= 30 * 60000 and slope < HEAP_DECLINE_B_PER_H:
        s["attention"].append(f"free_heap declining {slope:.0f} B/h over {span / 60000:.0f} min (leak suspect)")
    if s["https_fail"] and not s["https_ok"]:
        s["attention"].append("HTTPS never succeeded")
    for bad in ("EXPIRED", "NOT_YET_VALID", "BAD_DATE", "NO_PEER_CERT"):
        if s["cert_date"].get(bad):
            s["attention"].append(f"certificate date check {bad} x{s['cert_date'][bad]}")
    if s["secret_suspects"]:
        s["attention"].append(f"{s['secret_suspects']} line(s) look like unredacted secrets: do not share")
    return s


def format_summary(s: dict) -> str:
    out = ["=== DeskOS log summary (automatic checks only; visual checks stay with the tester) ===",
           f"lines={s['lines']} boots={s['boots']} versions={','.join(s['versions']) or '-'} "
           f"uptime_max={s['uptime_max_ms'] / 60000:.1f} min",
           f"memory: samples={s['memory_samples']} free_heap_min={s['free_heap_min']} "
           f"largest_block_min={s['largest_block_min']} min_free_heap={s['min_free_heap']} "
           f"slope_after_warmup={s['heap_slope_b_per_h']} B/h over {s['post_warmup_span_min']} min",
           f"https: ok={s['https_ok']} fail={s['https_fail']} ms_max={s['https_ms_max']} "
           f"heap_min={s['https_heap_min']} cert_date={s['cert_date']}",
           f"net: connected={s['net_connected']} disconnected={s['net_disconnected']}  "
           f"app_switches={s['app_switches']}  mission_rewards={s['mission_rewards']}  "
           f"focus_last_xp={s['focus_last_xp']}",
           f"pc: rejects_logged={s['pc_rejects_logged']} last={s['pc_last'] or '-'}"]
    out += [f"reset: {r}" for r in s["resets"][:10]]
    out += [f"storage: {r}" for r in s["storage_failures"][:10]]
    out.append("AUTO-CHECKS: " + ("OK (no automatic red flags)" if not s["attention"] else "ATTENTION"))
    out += [f"  - {a}" for a in s["attention"]]
    return "\n".join(out)


# --- Capture ---------------------------------------------------------------------
def capture(args: argparse.Namespace) -> int:
    try:
        import serial  # type: ignore
    except ImportError:
        print("pyserial missing. Use PlatformIO's Python:\n  %USERPROFILE%\\.platformio\\penv\\Scripts\\python.exe "
              "tools\\hw_validation\\deskos_serial_log.py capture --port COM5", file=sys.stderr)
        return 2
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"deskos_{datetime.now():%Y%m%d_%H%M%S}.log"
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = args.port, args.baud, 0.2
    port.dtr = False   # CYD auto-reset circuit: keep EN/IO0 released (no reboot on open).
    port.rts = False
    try:
        port.open()
    except serial.SerialException as error:
        print(f"cannot open {args.port}: {error} (close any other Serial Monitor first)", file=sys.stderr)
        return 2
    if args.reset:  # Pulse EN low with IO0 released: normal boot, so the boot log is captured.
        port.rts = True
        time.sleep(0.15)
        port.rts = False
    print(f"logging {args.port} -> {path} (Ctrl+C to stop; secrets are redacted)")
    started = time.monotonic()
    next_status = started + 3.0 if args.status_every > 0 else float("inf")
    pending = b""
    try:
        with path.open("w", encoding="utf-8", newline="\n") as log:
            while args.minutes <= 0 or time.monotonic() - started < args.minutes * 60:
                if time.monotonic() >= next_status:
                    port.write(b"STATUS\n")  # Read-only command; nothing secret is ever sent.
                    next_status = time.monotonic() + args.status_every * 60
                pending += port.read(4096)
                while b"\n" in pending:
                    raw, pending = pending.split(b"\n", 1)
                    text = redact(raw.decode("utf-8", errors="replace").rstrip("\r"), args.keep_ssid)
                    stamped = f"[{datetime.now():%Y-%m-%d %H:%M:%S.%f}"[:-3] + f"] {text}"
                    log.write(stamped + "\n")
                    log.flush()
                    print(stamped)
    except KeyboardInterrupt:
        pass
    finally:
        port.close()
    summary = analyze_lines(path.read_text(encoding="utf-8").splitlines())
    path.with_suffix(".summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(format_summary(summary))
    return 0


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="DeskOS Serial logger/analyzer (secrets redacted)")
    sub = parser.add_subparsers(dest="cmd", required=True)
    cap = sub.add_parser("capture", help="log the ESP32 Serial port")
    cap.add_argument("--port", required=True, help="e.g. COM5")
    cap.add_argument("--baud", type=int, default=115200)
    cap.add_argument("--minutes", type=float, default=0, help="stop after N minutes (0 = until Ctrl+C)")
    cap.add_argument("--status-every", type=float, default=10, help="send STATUS every N minutes (0 = never)")
    cap.add_argument("--reset", action="store_true", help="reboot the board once to capture the boot log")
    cap.add_argument("--keep-ssid", action="store_true", help="do not redact the Wi-Fi SSID")
    cap.add_argument("--out-dir", default="hw_logs", help="output folder (git-ignored)")
    ana = sub.add_parser("analyze", help="summarize an existing log")
    ana.add_argument("log")
    ana.add_argument("--json", action="store_true")
    ana.add_argument("--redact-to", help="also write a redacted copy of the log here")
    args = parser.parse_args(argv)
    if args.cmd == "capture":
        return capture(args)
    lines = Path(args.log).read_text(encoding="utf-8", errors="replace").splitlines()
    if args.redact_to:
        Path(args.redact_to).write_text("\n".join(redact(l) for l in lines) + "\n", encoding="utf-8")
    summary = analyze_lines(lines)
    print(json.dumps(summary, indent=2) if args.json else format_summary(summary))
    return 0


if __name__ == "__main__":
    sys.exit(main())
