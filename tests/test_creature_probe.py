#!/usr/bin/env python3
"""Subprocess regression for a real generated creature mesh and collision probe."""
import pathlib
import subprocess
import sys
import tempfile
import unittest


class CreatureProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = pathlib.Path(sys.argv[1]).resolve()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = pathlib.Path(self.temp.name) / "creature.obj"

    def run_probe(self, *args):
        return subprocess.run([str(self.exe), *map(str,args)], capture_output=True,
                              text=True, timeout=20)

    def test_gait_changes_geometry_not_topology(self):
        result = self.run_probe("mesh", ".8", ".9", ".8", ".7", "0", self.path)
        self.assertEqual(result.returncode, 0, result.stderr)
        fixed = self.path.read_text()
        v = [line for line in fixed.splitlines() if line.startswith("v ")]
        f = [line for line in fixed.splitlines() if line.startswith("f ")]
        self.assertGreater(len(v), 100)
        self.assertGreater(len(f), 100)
        other = self.run_probe("mesh", ".8", ".9", ".8", ".7", "1.6", self.path)
        self.assertEqual(other.returncode, 0, other.stderr)
        shifted = self.path.read_text()
        self.assertNotEqual(fixed, shifted)
        self.assertEqual(
            sum(line.startswith("v ") for line in shifted.splitlines()),len(v))
        self.assertEqual(
            sum(line.startswith("f ") for line in shifted.splitlines()),len(f))
        self.assertEqual(
            self.run_probe("mesh", ".8", ".9", ".8", ".7", "1.6",
                           self.path).returncode, 0)
        self.assertEqual(self.path.read_text(), shifted)

    def test_contact_resolution_and_validation(self):
        result = self.run_probe("contact",3,3,.4,3.2,3,.4)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("pairs=1", result.stdout)
        result = self.run_probe("contact",2,2,.2,5,5,.2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("pairs=0", result.stdout)
        self.assertEqual(
            self.run_probe("mesh",-1,.5,.5,.5,0,self.path).returncode,2)
        self.assertEqual(
            self.run_probe("contact",1,1,2,2,2,.4).returncode,2)


if __name__ == "__main__":
    if len(sys.argv)!=2: raise SystemExit("usage: test_creature_probe.py <probe>")
    unittest.main(argv=[sys.argv[0]],verbosity=2)
