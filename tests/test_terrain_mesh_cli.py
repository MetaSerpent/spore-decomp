#!/usr/bin/env python3
"""Black-box, retail-asset-free OBJ export verification."""
import pathlib
import subprocess
import sys
import tempfile
import unittest


class MeshIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.world_exe = pathlib.Path(sys.argv[1]).resolve()
        cls.mesh_exe = pathlib.Path(sys.argv[2]).resolve()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        folder = pathlib.Path(self.tmp.name)
        self.world = folder / "terrain.snapshot"
        self.mesh = folder / "terrain.obj"

    def run_exe(self, exe, *args):
        return subprocess.run([str(exe), *map(str, args)],
                              capture_output=True, text=True, timeout=30)

    def test_mesh_from_simulated_world(self):
        create = self.run_exe(
            self.world_exe, "run", 9, 7, 15, 42, 0.27, 0.4, self.world)
        self.assertEqual(create.returncode, 0, create.stderr)
        mesh = self.run_exe(self.mesh_exe, self.world, self.mesh, 10, 2)
        self.assertEqual(mesh.returncode, 0, mesh.stderr)
        self.assertIn("vertices=63 triangles=96", mesh.stdout)
        content = self.mesh.read_text()
        self.assertEqual(sum(s.startswith("v ") for s in content.splitlines()), 63)
        self.assertEqual(sum(s.startswith("vt ") for s in content.splitlines()), 63)
        self.assertEqual(sum(s.startswith("vn ") for s in content.splitlines()), 63)
        self.assertEqual(sum(s.startswith("f ") for s in content.splitlines()), 96)
        self.assertTrue(content.startswith("# BlackSpore"))
        self.assertRegex(content, r"f 1/1/1 10/10/10 2/2/2")
        repeat = self.run_exe(self.mesh_exe, self.world, self.mesh, 10, 2)
        self.assertEqual(repeat.returncode, 0, repeat.stderr)
        self.assertEqual(self.mesh.read_text(), content)

    def test_reject_bad_snapshot_or_scales(self):
        self.assertEqual(
            self.run_exe(self.mesh_exe, self.world, self.mesh).returncode, 1)
        self.assertFalse(self.mesh.exists())
        create = self.run_exe(
            self.world_exe, "run", 7, 4, 1, 3, 0.2, 0.1, self.world)
        self.assertEqual(create.returncode, 0, create.stderr)
        for x, y in [(0, 1), (1, -1), ("nan", 1)]:
            res = self.run_exe(self.mesh_exe, self.world, self.mesh, x, y)
            self.assertEqual(res.returncode, 2, res.stderr)
            self.assertFalse(self.mesh.exists())


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_terrain_mesh_cli.py <terrain_field> <terrain_mesh>")
    unittest.main(argv=[sys.argv[0]], verbosity=2)
