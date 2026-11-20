"""Unit tests for the hardware-validation Serial logger (no serial port needed)."""
import io
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import deskos_serial_log as log  # noqa: E402

TOKEN = "00112233445566778899aabbccddeeff"


def memory(minutes: float, heap: int, block: int = 110000) -> str:
    return (f"MEMORY: uptime_ms={int(minutes * 60000)} mode=SHELL app=HOME switches=3 "
            f"free_heap={heap} largest_free_block={block} min_free_heap=90000 loop_stack_free=5000")


class RedactionTests(unittest.TestCase):
    def test_token_and_agent_command_redacted(self):
        line = f"PC: AGENT python deskos_agent.py --host 192.168.1.50 --token {TOKEN} --save-config"
        out = log.redact(line)
        self.assertNotIn(TOKEN, out)
        self.assertIn("--token <REDACTED>", out)
        self.assertIn("192.168.1.50", out)          # IPs are not secrets; kept for diagnosis.
        self.assertNotIn(TOKEN, log.redact(f"PC: TOKEN {TOKEN.upper()}"))
        self.assertFalse(log.looks_secret(out))

    def test_password_ssid_env(self):
        self.assertEqual(log.redact("WIFI PASS my secret pw"), "WIFI PASS <REDACTED>")
        self.assertEqual(log.redact("set DESKOS_TOKEN=abc"), "set DESKOS_TOKEN=<REDACTED>")
        net = 'NET: state=CONNECTED ssid="HomeNet" pass_len=12 enabled=1'
        self.assertIn('ssid="<REDACTED>"', log.redact(net))
        self.assertIn('ssid="HomeNet"', log.redact(net, keep_ssid=True))
        self.assertEqual(log.redact("WIFI SSID HomeNet"), "WIFI SSID <REDACTED>")

    def test_sha256_hashes_are_not_mangled(self):
        sha = "41211AAB26695D850F641052B198B73401821A550E858B0C2A58FCF55B0506C1"  # 64 hex, not a token.
        self.assertEqual(log.redact(sha), sha)


class AnalyzeTests(unittest.TestCase):
    def test_healthy_stress_log(self):
        lines = ["[2026-11-20 10:00:00.000] FIRMWARE_VERSION: DESKOS_1.0.0-RC1", "NET: CONNECTED ip=192.168.1.50 rssi=-50"]
        lines += [memory(m, 150000 + (m % 3) * 50) for m in range(0, 70)]
        lines += ["HTTPS: code=200 bytes=640 ms=1800 cert_date=VALID stack_free=4000 heap=120000 max_block=90000 min_heap=80000",
                  "APP_SWITCH: id=1 app=PET duration_us=900 switches=1 free_heap=1 largest_free_block=1",
                  "MISSION_REWARD: key=mission:1:1 result=GRANTED bonus_xp=10 total_xp=35",
                  "FOCUS: ms=1 mode=BREAK state=COMPLETED remaining_ms=300000 xp=50 sessions=2 id=2 rewarded_id=2 save_pending=0 error=NONE"]
        s = log.analyze_lines(lines)
        self.assertEqual((s["boots"], s["versions"]), (1, ["DESKOS_1.0.0-RC1"]))
        self.assertEqual(s["memory_samples"], 70)
        self.assertEqual((s["https_ok"], s["cert_date"]), (1, {"VALID": 1}))
        self.assertEqual((s["app_switches"], s["mission_rewards"], s["focus_last_xp"]), (1, {"GRANTED": 1}, 50))
        self.assertEqual(s["attention"], [])
        self.assertIn("AUTO-CHECKS: OK", log.format_summary(s))

    def test_flags_resets_leaks_fragmentation_and_secrets(self):
        lines = ["FIRMWARE_VERSION: DESKOS_1.0.0-RC1", "Brownout detector was triggered",
                 "rst:0xc (SW_CPU_RESET),boot:0x13", "FIRMWARE_VERSION: DESKOS_1.0.0-RC1",
                 "BLOB_STORE: WRITE_OR_READBACK_FAIL ns=deskos-pet key=state bytes=24",
                 "TOUCH_CAL: NVS_SAVE_FAIL",
                 "HTTPS: code=-101 bytes=0 ms=900 cert_date=EXPIRED stack_free=4000 heap=1 max_block=1 min_heap=1",
                 f"PC: TOKEN {TOKEN}"]
        lines += [memory(m, 160000 - m * 200, block=30000) for m in range(5, 70)]  # -12 KB/h, fragmented.
        s = log.analyze_lines(lines)
        text = " | ".join(s["attention"])
        for expected in ("reset/crash", "2 boots", "storage failure", "largest_free_block", "declining",
                         "HTTPS never succeeded", "EXPIRED", "unredacted"):
            self.assertIn(expected, text)
        self.assertLess(s["heap_slope_b_per_h"], -10000)
        self.assertEqual(len(s["storage_failures"]), 2)
        self.assertEqual(log.analyze_lines(["FOCUS: ms=1 xp=5 save_pending=0 error=NONE",
                                            "FOCUS_STORE: SAVED generation=3"])["storage_failures"], [])

    def test_cli_analyze_and_redacted_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            src, dst = Path(tmp) / "raw.log", Path(tmp) / "shared.log"
            src.write_text(f"FIRMWARE_VERSION: X\nPC: TOKEN {TOKEN}\n", encoding="utf-8")
            out = io.StringIO()
            with redirect_stdout(out):
                self.assertEqual(log.main(["analyze", str(src), "--redact-to", str(dst)]), 0)
            self.assertNotIn(TOKEN, dst.read_text(encoding="utf-8"))
            self.assertIn("boots=1", out.getvalue())

    def test_capture_sends_only_read_only_commands(self):
        source = Path(log.__file__).read_text(encoding="utf-8")
        self.assertEqual(sorted(set(__import__("re").findall(r"port\.write\((b\"[^\"]*\")", source))), ['b"STATUS\\n"'])


if __name__ == "__main__":
    unittest.main(verbosity=2)
