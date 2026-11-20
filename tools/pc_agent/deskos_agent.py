#!/usr/bin/env python3
"""CYD DeskOS PC Agent (protocol DPM1).

Sends CPU/RAM/disk/network/battery telemetry from a Windows PC to the CYD
DeskOS ESP32 on the local network, authenticated with HMAC-SHA256.

Security properties (see docs/PC_AGENT_SETUP.md):
  * Send-only UDP: this program never binds, listens or accepts connections.
  * It never executes, evaluates or forwards anything it receives (it receives nothing).
  * It refuses to send to non-private (Internet) addresses.
  * The token is read from an argument, the DESKOS_TOKEN environment variable or
    %APPDATA%\\DeskOS\\agent.json - never from this repository.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import hmac
import ipaddress
import json
import os
import re
import socket
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

PROTOCOL = "DPM1"
DEFAULT_PORT = 47801
MAX_PACKET = 400
TOKEN_RE = re.compile(r"^[0-9a-fA-F]{32}$")

try:  # Optional: better metrics (network throughput). Not required.
    import psutil  # type: ignore
except Exception:  # pragma: no cover - depends on the machine
    psutil = None


@dataclass
class Metrics:
    cpu_permille: int = 0
    ram_permille: int = 0
    ram_used_mb: int = 0
    ram_total_mb: int = 0
    disk_permille: int = 0
    up_kbps: int = -1
    down_kbps: int = -1
    battery_percent: int = -1
    charging: int = 0


def clamp(value: float, low: int, high: int) -> int:
    return max(low, min(high, int(round(value))))


def sanitize_host(name: str) -> str:
    cleaned = re.sub(r"[^A-Z0-9_.-]", "", name.upper())[:15]
    return cleaned or "PC"


def parse_token(text: str) -> bytes:
    text = (text or "").strip()
    if not TOKEN_RE.match(text):
        raise ValueError("token must be 32 hex characters (Serial: PC TOKEN on the ESP32)")
    return bytes.fromhex(text)


def check_lan_target(host: str) -> str:
    """Resolve and accept only private/link-local IPv4 addresses (LAN only)."""
    address = socket.gethostbyname(host)
    ip = ipaddress.ip_address(address)
    if ip.version != 4 or not (ip.is_private or ip.is_link_local) or ip.is_loopback or ip.is_multicast \
            or ip.is_unspecified or address.endswith(".255"):
        raise ValueError(f"{address} is not a private LAN unicast address; refusing to send")
    return address


def build_packet(token: bytes, counter: int, unix_time: int, host: str, m: Metrics) -> bytes:
    if len(token) != 16:
        raise ValueError("token must be 16 bytes")
    body = (f"{PROTOCOL};n={counter};t={unix_time};h={sanitize_host(host)};"
            f"cpu={clamp(m.cpu_permille, 0, 1000)};ram={clamp(m.ram_permille, 0, 1000)};"
            f"ru={clamp(m.ram_used_mb, 0, 16777216)};rt={clamp(m.ram_total_mb, 0, 16777216)};"
            f"dsk={clamp(m.disk_permille, 0, 1000)};up={clamp(m.up_kbps, -1, 100000000)};"
            f"dn={clamp(m.down_kbps, -1, 100000000)};bat={clamp(m.battery_percent, -1, 100)};"
            f"chg={1 if m.charging else 0};")
    mac = hmac.new(token, body.encode("ascii"), hashlib.sha256).hexdigest()
    packet = (body + "mac=" + mac).encode("ascii")
    if len(packet) > MAX_PACKET:
        raise ValueError("packet too large")
    return packet


class Counter:
    """Strictly increasing message counter (milliseconds, never repeats)."""

    def __init__(self) -> None:
        self.last = 0

    def next(self) -> int:
        self.last = max(self.last + 1, int(time.time() * 1000))
        return self.last


# --- Windows fallbacks (stdlib ctypes only) ---------------------------------
class _FILETIME(ctypes.Structure):
    _fields_ = [("low", ctypes.c_uint32), ("high", ctypes.c_uint32)]


class _MEMORYSTATUSEX(ctypes.Structure):
    _fields_ = [("dwLength", ctypes.c_uint32), ("dwMemoryLoad", ctypes.c_uint32),
                ("ullTotalPhys", ctypes.c_uint64), ("ullAvailPhys", ctypes.c_uint64),
                ("ullTotalPageFile", ctypes.c_uint64), ("ullAvailPageFile", ctypes.c_uint64),
                ("ullTotalVirtual", ctypes.c_uint64), ("ullAvailVirtual", ctypes.c_uint64),
                ("ullAvailExtendedVirtual", ctypes.c_uint64)]


class _SYSTEM_POWER_STATUS(ctypes.Structure):
    _fields_ = [("ACLineStatus", ctypes.c_uint8), ("BatteryFlag", ctypes.c_uint8),
                ("BatteryLifePercent", ctypes.c_uint8), ("SystemStatusFlag", ctypes.c_uint8),
                ("BatteryLifeTime", ctypes.c_uint32), ("BatteryFullLifeTime", ctypes.c_uint32)]


class MetricsCollector:
    def __init__(self) -> None:
        self.windows = sys.platform == "win32"
        self._cpu_prev: Optional[tuple[int, int]] = None
        self._net_prev: Optional[tuple[float, int, int]] = None
        self.system_drive = (os.environ.get("SystemDrive", "C:") + "\\") if self.windows else "/"
        if psutil:
            psutil.cpu_percent(interval=None)  # Prime: first call always returns 0.

    def _cpu_windows(self) -> Optional[float]:
        idle, kernel, user = _FILETIME(), _FILETIME(), _FILETIME()
        if not ctypes.windll.kernel32.GetSystemTimes(ctypes.byref(idle), ctypes.byref(kernel), ctypes.byref(user)):
            return None
        to_int = lambda ft: (ft.high << 32) | ft.low  # noqa: E731
        idle_t, total_t = to_int(idle), to_int(kernel) + to_int(user)  # Kernel time includes idle.
        prev, self._cpu_prev = self._cpu_prev, (idle_t, total_t)
        if not prev or total_t == prev[1]:
            return None
        return 100.0 * (1.0 - (idle_t - prev[0]) / (total_t - prev[1]))

    def collect(self) -> Metrics:
        m = Metrics()
        if psutil:
            m.cpu_permille = clamp(psutil.cpu_percent(interval=None) * 10, 0, 1000)
            vm = psutil.virtual_memory()
            m.ram_permille = clamp(vm.percent * 10, 0, 1000)
            m.ram_total_mb = vm.total // (1024 * 1024)
            m.ram_used_mb = min(m.ram_total_mb, (vm.total - vm.available) // (1024 * 1024))
            m.disk_permille = clamp(psutil.disk_usage(self.system_drive).percent * 10, 0, 1000)
            now, net = time.monotonic(), psutil.net_io_counters()
            if self._net_prev and now > self._net_prev[0]:
                dt = now - self._net_prev[0]
                m.up_kbps = clamp((net.bytes_sent - self._net_prev[1]) / 1024 / dt, 0, 100000000)
                m.down_kbps = clamp((net.bytes_recv - self._net_prev[2]) / 1024 / dt, 0, 100000000)
            self._net_prev = (now, net.bytes_sent, net.bytes_recv)
            battery = psutil.sensors_battery() if hasattr(psutil, "sensors_battery") else None
            if battery is not None:
                m.battery_percent = clamp(battery.percent, 0, 100)
                m.charging = 1 if battery.power_plugged else 0
            return m
        if self.windows:
            cpu = self._cpu_windows()
            m.cpu_permille = clamp((cpu or 0.0) * 10, 0, 1000)
            status = _MEMORYSTATUSEX()
            status.dwLength = ctypes.sizeof(_MEMORYSTATUSEX)
            if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
                m.ram_total_mb = status.ullTotalPhys // (1024 * 1024)
                m.ram_used_mb = (status.ullTotalPhys - status.ullAvailPhys) // (1024 * 1024)
                m.ram_permille = clamp(m.ram_used_mb * 1000 / max(1, m.ram_total_mb), 0, 1000)
            free, total = ctypes.c_uint64(), ctypes.c_uint64()
            if ctypes.windll.kernel32.GetDiskFreeSpaceExW(self.system_drive, None, ctypes.byref(total), ctypes.byref(free)):
                m.disk_permille = clamp((total.value - free.value) * 1000 / max(1, total.value), 0, 1000)
            power = _SYSTEM_POWER_STATUS()
            if ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(power)) and power.BatteryFlag != 128 \
                    and power.BatteryLifePercent != 255:
                m.battery_percent = clamp(power.BatteryLifePercent, 0, 100)
                m.charging = 1 if power.ACLineStatus == 1 else 0
        return m


# --- Configuration -------------------------------------------------------------
def config_path() -> Path:
    base = os.environ.get("APPDATA") or str(Path.home() / ".config")
    return Path(base) / "DeskOS" / "agent.json"


def load_config() -> dict:
    path = config_path()
    if path.is_file():
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            return data if isinstance(data, dict) else {}
        except (OSError, ValueError):
            print(f"warning: ignoring unreadable config {path}", file=sys.stderr)
    return {}


def save_config(host: str, port: int, token_hex: str) -> Path:
    path = config_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"host": host, "port": port, "token": token_hex}, indent=2), encoding="utf-8")
    return path


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="CYD DeskOS PC Agent (send-only, LAN-only)")
    parser.add_argument("--host", help="ESP32 IP shown on the PC MONITOR screen, e.g. 192.168.1.50")
    parser.add_argument("--port", type=int, help=f"UDP port (default {DEFAULT_PORT})")
    parser.add_argument("--token", help="32-hex token from Serial 'PC TOKEN' (prefer env DESKOS_TOKEN)")
    parser.add_argument("--interval", type=float, default=2.0, help="seconds between packets (1-60)")
    parser.add_argument("--save-config", action="store_true", help="store host/port/token in %%APPDATA%%\\DeskOS")
    parser.add_argument("--once", action="store_true", help="send one packet and exit")
    parser.add_argument("--dry-run", action="store_true", help="collect and print metrics, do not send")
    parser.add_argument("--emit-test-packet", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    if args.emit_test_packet:  # Deterministic packet for the firmware cross-check test.
        token = parse_token(args.token or "")
        m = Metrics(456, 623, 10240, 16384, 711, 12, 340, 87, 1)
        sys.stdout.write(build_packet(token, 1700000000123, 1763600000, "deskos-test", m).decode("ascii"))
        return 0

    config = load_config()
    host = args.host or os.environ.get("DESKOS_HOST") or config.get("host")
    port = int(args.port or os.environ.get("DESKOS_PORT") or config.get("port") or DEFAULT_PORT)
    token_hex = args.token or os.environ.get("DESKOS_TOKEN") or config.get("token") or ""
    interval = min(60.0, max(1.0, args.interval))
    collector = MetricsCollector()
    if args.dry_run:
        time.sleep(1.0)
        print(collector.collect())
        return 0
    if not host:
        parser.error("missing --host (or DESKOS_HOST / saved config)")
    try:
        token = parse_token(token_hex)
        address = check_lan_target(host)
    except (ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    if not 1 <= port <= 65535:
        print("error: invalid port", file=sys.stderr)
        return 2
    if args.save_config:
        print(f"saved {save_config(host, port, token_hex.lower())}")

    counter = Counter()
    hostname = sanitize_host(socket.gethostname())
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # Send-only: never bound by us.
    print(f"DeskOS agent -> {address}:{port} every {interval:g}s as {hostname}. Ctrl+C to stop.")
    last_error = 0.0
    sent = 0
    try:
        while True:
            started = time.monotonic()
            packet = build_packet(token, counter.next(), int(time.time()), hostname, collector.collect())
            try:
                sock.sendto(packet, (address, port))
                sent += 1
            except OSError as error:  # Wi-Fi down, sleep/resume...: keep running, log at most once a minute.
                if started - last_error > 60:
                    print(f"send failed ({error}); will keep retrying", file=sys.stderr)
                    last_error = started
            if args.once:
                break
            time.sleep(max(0.1, interval - (time.monotonic() - started)))
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
    print(f"stopped after {sent} packets")
    return 0


if __name__ == "__main__":
    sys.exit(main())
