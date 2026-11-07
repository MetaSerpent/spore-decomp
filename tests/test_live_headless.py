#!/usr/bin/env python3
"""Actual X11/GDI software window smoke without human interaction.

Executed under a virtual X11 display (xvfb-run) on Linux. Does not simulate
game assets or claim to test retail Spore. The native render loop must create
a window, draw three complete frames, then exit cleanly.
"""
import pathlib
import subprocess
import sys
import tempfile
import unittest


class VirtualDisplayWindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.terrain_exe = pathlib.Path(sys.argv[1]).resolve()
        cls.live_exe = pathlib.Path(sys.argv[2]).resolve()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.snapshot = self.root / "terrain.snapshot"

    def run_exe(self, *cmd):
        return subprocess.run([*map(str, cmd)], capture_output=True,
                              text=True, timeout=35)

    def test_window_creation_and_framebuffer_blits(self):
        make = self.run_exe(
            self.terrain_exe, "run", 16, 12, 3, 123, "0.25", "0.4",
            self.snapshot)
        self.assertEqual(make.returncode, 0, make.stderr)
        process = self.run_exe(
            "xvfb-run", "-a", self.live_exe, self.snapshot, "--frames", 3)
        self.assertEqual(process.returncode, 0,
                         f"{process.stdout}\n{process.stderr}")
        self.assertIn("Native terrain viewer", process.stdout)
        self.assertFalse((self.root / "terrain.snapshot.edited.snapshot").exists())

    def test_bad_input_exits_without_starting_window(self):
        bad = self.run_exe(
            "xvfb-run", "-a", self.live_exe, self.snapshot, "--frames", 2)
        self.assertEqual(bad.returncode, 1)
        broken = self.run_exe(
            "xvfb-run", "-a", self.live_exe, self.snapshot, "--frames", 0)
        self.assertNotEqual(broken.returncode, 0)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_live_headless.py <terrain_exe> <live_exe>")
    unittest.main(argv=[sys.argv[0]], verbosity=2)
