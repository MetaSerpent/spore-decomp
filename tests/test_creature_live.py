#!/usr/bin/env python3
"""Actual native creature 3D prototype under Xvfb; no human E2E."""
import pathlib
import subprocess
import sys
import tempfile
import unittest


class CreatureDesktop(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.executable = pathlib.Path(sys.argv[1]).resolve()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.capture = pathlib.Path(self.temp.name) / "scene.ppm"

    def run_demo(self, count):
        return subprocess.run(
            ["xvfb-run", "-a", str(self.executable), "--frames", str(count),
             "--ppm", str(self.capture)],
            capture_output=True, text=True, timeout=45)

    def test_native_window_renders_procedural_creatures(self):
        first = self.run_demo(3)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertIn("frames=3", first.stdout)
        data = self.capture.read_bytes()
        header = b"P6\n960 600\n255\n"
        self.assertTrue(data.startswith(header))
        self.assertEqual(len(data), len(header) + 960 * 600 * 3)
        again = self.run_demo(3)
        self.assertEqual(again.returncode, 0, again.stderr)
        self.assertEqual(self.capture.read_bytes(), data)
        longer = self.run_demo(12)
        self.assertEqual(longer.returncode, 0, longer.stderr)
        self.assertNotEqual(self.capture.read_bytes(), data)

    def test_invalid_arguments_rejected(self):
        bad = subprocess.run(
            [str(self.executable), "--frames", "0"],
            capture_output=True, text=True, timeout=10)
        self.assertNotEqual(bad.returncode, 0)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_creature_live.py <creature_live executable>")
    unittest.main(argv=[sys.argv[0]],verbosity=2)
