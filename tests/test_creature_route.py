#!/usr/bin/env python3
"""Asset-free subprocess test of deterministic creature navigation."""
import csv
import io
import pathlib
import subprocess
import sys
import tempfile
import unittest


class NavigationCLI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.field_exe = pathlib.Path(sys.argv[1]).resolve()
        cls.route_exe = pathlib.Path(sys.argv[2]).resolve()

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.snapshot = pathlib.Path(tmp.name) / "terrain.snapshot"
        proc = self.run_command(
            self.field_exe, "run", 16, 16, 4, 123, "0.18", "0.1", self.snapshot)
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def run_command(self, *args):
        return subprocess.run([*map(str, args)], capture_output=True,
                              text=True, timeout=20)

    def test_route_is_repeatable_and_cardinal(self):
        first = self.run_command(self.route_exe, self.snapshot, 0, 0, 15, 15)
        second = self.run_command(self.route_exe, self.snapshot, 0, 0, 15, 15)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(first.stdout, second.stdout)
        self.assertEqual(first.stderr, second.stderr)
        path = [(int(r["x"]), int(r["z"]))
                for r in csv.DictReader(io.StringIO(first.stdout))]
        self.assertEqual(path[0], (0, 0))
        self.assertEqual(path[-1], (15, 15))
        self.assertGreaterEqual(len(path), 31)
        for a, b in zip(path, path[1:]):
            self.assertEqual(abs(a[0]-b[0])+abs(a[1]-b[1]), 1)
        self.assertIn("cost_milli=", first.stderr)

    def test_zero_distance_and_unreachable(self):
        same = self.run_command(self.route_exe, self.snapshot, 1, 1, 1, 1)
        self.assertEqual(same.returncode, 0, same.stderr)
        self.assertEqual(same.stdout, "x,z\n1,1\n")
        # All samples are <= 0.6, so a waterline of 0.9 blocks both ends.
        no_route = self.run_command(
            self.route_exe, self.snapshot, 1, 1, 14, 14, .9, .2, 4)
        self.assertEqual(no_route.returncode, 3)
        self.assertEqual(no_route.stdout, "NO_ROUTE\n")

    def test_errors(self):
        for args in [
            (self.route_exe, self.snapshot, -1, 0, 2, 2),
            (self.route_exe, self.snapshot, 0, 0, 999, 2),
            (self.route_exe, self.snapshot, 0, 0, 2, 2, -1),
            (self.route_exe, self.snapshot, 0, 0, 2, 2, 0.1, "nan"),
            (self.route_exe, self.snapshot, 0, 0, 2, 2, 0.1, 1, -3),
        ]:
            self.assertEqual(self.run_command(*args).returncode, 2)
        self.assertEqual(
            self.run_command(self.route_exe, "missing.snapshot", 0, 0, 1, 1).returncode,
            1)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_creature_route.py <field_exe> <route_exe>")
    unittest.main(argv=[sys.argv[0]], verbosity=2)
