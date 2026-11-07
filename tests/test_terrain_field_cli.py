#!/usr/bin/env python3
"""Asset-free, subprocess-level verification of the terraforming runtime CLI."""
import csv
import io
import pathlib
import subprocess
import sys
import tempfile
import unittest


class TerrainFieldCliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = pathlib.Path(sys.argv[1]).resolve()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.snapshot = pathlib.Path(self.tmp.name) / "world.snap"

    def run_cli(self, *args):
        return subprocess.run([str(self.exe), *map(str, args)],
                              text=True, capture_output=True, timeout=20)

    def test_replay_identical_across_runs(self):
        params = ("run", 20, 16, 75, 1234567, "0.23", "0.5")
        first = self.run_cli(*params, self.snapshot)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertTrue(self.snapshot.exists())
        second = self.run_cli(*params)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(first.stdout, second.stdout)
        rows = list(csv.DictReader(io.StringIO(first.stdout)))
        self.assertEqual(len(rows), 75)
        self.assertEqual(rows[-1]["tick"], "75")
        self.assertTrue(all(0 <= float(r["mean"]) <= 1 for r in rows))
        self.assertTrue(all(int(r["planet_type"]) in range(1, 6) for r in rows))
        report = self.run_cli("inspect", self.snapshot)
        self.assertEqual(report.returncode, 0, report.stderr)
        inspected = list(csv.DictReader(io.StringIO(report.stdout)))
        self.assertEqual(len(inspected), 1)
        self.assertEqual(inspected[0]["checksum"], rows[-1]["checksum"])
        self.assertEqual(inspected[0]["mean"], rows[-1]["mean"])
        self.assertEqual(inspected[0]["planet_type"], rows[-1]["planet_type"])

    def test_seed_changes_terrain(self):
        first = self.run_cli("run", 12, 12, 15, 1, "0.3", "0.3")
        second = self.run_cli("run", 12, 12, 15, 2, "0.3", "0.3")
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertNotEqual(first.stdout.splitlines()[-1].split(",")[-1],
                            second.stdout.splitlines()[-1].split(",")[-1])

    def test_bad_inputs_never_create_snapshot(self):
        for args in [
            ("run", 1, 12, 3, 5, ".2", ".3", self.snapshot),
            ("run", 12, 12, -1, 5, ".2", ".3", self.snapshot),
            ("run", 12, 12, 3, 5, "0", ".3", self.snapshot),
            ("run", 12, 12, 3, 5, ".2", "1.3", self.snapshot),
            ("run", 1000, 1000, 10000, 5, ".2", ".3", self.snapshot),
        ]:
            p = self.run_cli(*args)
            self.assertNotEqual(p.returncode, 0, (args, p.stdout, p.stderr))
            self.assertFalse(self.snapshot.exists())
        self.assertNotEqual(self.run_cli("inspect", self.snapshot).returncode, 0)
        self.snapshot.write_text("SPORE_TERRAFORM_FIELD 1\n2 2\n0 0 0 bad\n")
        self.assertNotEqual(self.run_cli("inspect", self.snapshot).returncode, 0)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_terrain_field_cli.py <path to executable>")
    unittest.main(argv=[sys.argv[0]], verbosity=2)
