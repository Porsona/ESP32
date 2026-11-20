"""Unit tests for the DeskOS PC agent (stdlib unittest, no network needed)."""
import hashlib
import hmac
import io
import re
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import deskos_agent as agent  # noqa: E402

TOKEN_HEX = "00112233445566778899aabbccddeeff"
TOKEN = bytes.fromhex(TOKEN_HEX)


class PacketTests(unittest.TestCase):
    def test_format_and_hmac(self):
        m = agent.Metrics(456, 623, 10240, 16384, 711, 12, 340, 87, 1)
        packet = agent.build_packet(TOKEN, 42, 1763600000, "my-laptop", m).decode("ascii")
        self.assertTrue(packet.startswith("DPM1;n=42;t=1763600000;h=MY-LAPTOP;cpu=456;ram=623;"))
        body, mac = packet.rsplit("mac=", 1)
        self.assertTrue(body.endswith(";"))
        self.assertEqual(mac, hmac.new(TOKEN, body.encode(), hashlib.sha256).hexdigest())
        self.assertRegex(mac, r"^[0-9a-f]{64}$")
        self.assertLessEqual(len(packet), agent.MAX_PACKET)

    def test_values_are_clamped(self):
        m = agent.Metrics(5000, -3, 99, 50, 2000, -9, -9, 300, 7)
        packet = agent.build_packet(TOKEN, 1, 0, "x", m).decode()
        for field in ("cpu=1000;", "ram=0;", "dsk=1000;", "up=-1;", "dn=-1;", "bat=100;", "chg=1;"):
            self.assertIn(field, packet)

    def test_host_sanitized(self):
        self.assertEqual(agent.sanitize_host("Desk top;rm -rf"), "DESKTOPRM-RF")
        self.assertEqual(agent.sanitize_host("a" * 40), "A" * 15)
        self.assertEqual(agent.sanitize_host("@@@"), "PC")

    def test_token_validation(self):
        self.assertEqual(agent.parse_token(TOKEN_HEX.upper()), TOKEN)
        for bad in ("", "abc", TOKEN_HEX + "0", "zz" * 16):
            with self.assertRaises(ValueError):
                agent.parse_token(bad)

    def test_lan_only(self):
        for ok in ("192.168.1.50", "10.0.0.7", "172.16.3.4", "169.254.10.10"):
            self.assertEqual(agent.check_lan_target(ok), ok)
        for bad in ("8.8.8.8", "1.1.1.1", "127.0.0.1", "0.0.0.0", "224.0.0.1", "192.168.1.255"):
            with self.assertRaises(ValueError):
                agent.check_lan_target(bad)

    def test_counter_strictly_increases(self):
        c = agent.Counter()
        values = [c.next() for _ in range(1000)]
        self.assertTrue(all(b > a for a, b in zip(values, values[1:])))

    def test_emit_test_packet_is_deterministic(self):
        out = io.StringIO()
        with redirect_stdout(out):
            agent.main(["--emit-test-packet", "--token", TOKEN_HEX])
        first = out.getvalue()
        out = io.StringIO()
        with redirect_stdout(out):
            agent.main(["--emit-test-packet", "--token", TOKEN_HEX])
        self.assertEqual(first, out.getvalue())
        self.assertIn("h=DESKOS-TEST;", first)

    def test_send_only_no_remote_execution(self):
        source = Path(agent.__file__).read_text(encoding="utf-8")
        code = "\n".join(line.split("#", 1)[0] for line in source.splitlines())
        for forbidden in (r"\.bind\(", r"\.listen\(", r"\.accept\(", r"\.recv", r"\beval\(", r"\bexec\(",
                          r"subprocess", r"os\.system", r"SOCK_STREAM"):
            self.assertIsNone(re.search(forbidden, code), forbidden)

    def test_collector_runs(self):
        m = agent.MetricsCollector().collect()
        self.assertTrue(0 <= m.cpu_permille <= 1000 and 0 <= m.ram_permille <= 1000)
        self.assertTrue(m.ram_used_mb <= m.ram_total_mb)


if __name__ == "__main__":
    unittest.main(verbosity=2)
