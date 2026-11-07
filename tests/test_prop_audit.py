#!/usr/bin/env python3
"""Black-box corpus probes for the read-only Spore property inventory.

All fixture bytes are synthetic and independent of installed retail assets.
"""
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest


PROP = 0x00B1B104
GROUP = 0x44


def record(name, kind, payload, flags=0):
    return struct.pack(">IHH", name, kind, flags) + payload


def prop(*entries):
    return struct.pack(">I", len(entries)) + b"".join(entries)


def key(group, instance):
    return struct.pack("<IIII", instance, 0, group, 0)


def write_package(path, records, huge_key=None):
    body = bytearray(0x60)
    body[:4] = b"DBPF"
    struct.pack_into("<I", body, 4, 3)
    struct.pack_into("<I", body, 0x24, len(records))
    struct.pack_into("<I", body, 0x3C, 3)
    index = bytearray(struct.pack("<II", 4, 0))
    for type_id, group, instance, payload in records:
        off = len(body)
        body.extend(payload)
        size = len(payload)
        mem_size = ((16 << 20) + 1) if huge_key == (group, instance) else size
        index.extend(struct.pack(
            "<IIIIII", type_id, group, instance, off, size, mem_size))
    struct.pack_into("<I", body, 0x40, len(body))
    struct.pack_into("<I", body, 0x2C, len(index))
    body.extend(index)
    path.write_bytes(body)


class BulkPropertyAudits(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = pathlib.Path(sys.argv[1]).resolve()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = pathlib.Path(self.tmp.name)
        self.base = root / "base.package"
        self.patch = root / "patch.package"
        self.broken = root / "broken.package"
        self.orphan = root / "orphan.package"
        self.huge = root / "huge.package"

        parent = prop(
            record(1, 1, b"\x01"),
            record(2, 9, struct.pack(">i", 25)),
        )
        child = prop(
            record(0x00B2CCCB, 0x20, key(GROUP, 1)),
            record(3, 0x0D, struct.pack(">f", 1.5)),
        )
        patched = prop(
            record(0x00B2CCCB, 0x20, key(GROUP, 1)),
            record(3, 0x0D, struct.pack(">f", 2.5)),
        )
        write_package(self.base, [
            (PROP, GROUP, 1, parent),
            (PROP, GROUP, 2, child),
            (0xF00F, GROUP, 3, b"not-a-property"),
        ])
        write_package(self.patch, [(PROP, GROUP, 2, patched)])
        write_package(self.broken, [
            (PROP, GROUP, 4, prop(record(17, 0xFFFF, b""))),
        ])
        write_package(self.orphan, [
            (PROP, GROUP, 5, prop(
                record(0x00B2CCCB, 0x20, key(GROUP, 0xBAD)))),
        ])
        write_package(self.huge, [
            (PROP, GROUP, 6, prop(record(17, 1, b"\x01"))),
        ], huge_key=(GROUP, 6))

    def invoke(self, *args):
        return subprocess.run(
            [str(self.exe), *map(str, args)],
            capture_output=True, text=True, timeout=20)

    def test_full_inventory_and_repeatability(self):
        result = self.invoke("--strict", "--verify-parents", self.base)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data["packages"], 1)
        self.assertEqual(data["effective_resources"], 3)
        self.assertEqual(data["property_resources"], 2)
        self.assertEqual(data["decoded"], 2)
        self.assertEqual(data["failed"], 0)
        self.assertEqual(data["property_records"], 4)
        self.assertEqual(data["parent_chains_passed"], 2)
        self.assertEqual(data["parent_chains_failed"], 0)
        self.assertEqual(data["errors"], [])
        types = {x["id"]: x["count"] for x in data["types"]}
        self.assertEqual(types, {
            "0x00000001": 1,
            "0x00000009": 1,
            "0x0000000D": 1,
            "0x00000020": 1,
        })
        self.assertEqual(self.invoke("--strict", "--verify-parents", self.base).stdout,
                         result.stdout, "inventory should be reproducible")

    def test_patch_stacks_and_unsupported_types(self):
        result = self.invoke("--strict", self.base, self.patch)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data["packages"], 2)
        self.assertEqual(data["effective_resources"], 3)
        self.assertEqual(data["property_records"], 4)
        self.assertNotIn("parent_chains_failed", data)
        result = self.invoke(self.base, self.broken)
        self.assertEqual(result.returncode, 0, "non-strict scan reports errors")
        data = json.loads(result.stdout)
        self.assertEqual(data["decoded"], 2)
        self.assertEqual(data["failed"], 1)
        self.assertEqual(data["property_resources"], 3)
        self.assertEqual(data["errors"][0]["instance"], "0x00000004")
        self.assertIn("unsupported", data["errors"][0]["reason"])
        self.assertEqual(self.invoke("--strict", self.base, self.broken).returncode, 3)

    def test_missing_parent_reported_without_unbounded_recursion(self):
        result = self.invoke("--verify-parents", "--strict", self.orphan)
        self.assertEqual(result.returncode, 3)
        data = json.loads(result.stdout)
        self.assertEqual(data["decoded"], 1)
        self.assertEqual(data["failed"], 0)
        self.assertEqual(data["parent_chains_passed"], 0)
        self.assertEqual(data["parent_chains_failed"], 1)
        self.assertIn("inheritance:", data["errors"][0]["reason"])

    def test_oversized_entry_is_skipped_before_decompression(self):
        result = self.invoke("--strict", self.huge)
        self.assertEqual(result.returncode, 3)
        data = json.loads(result.stdout)
        self.assertEqual(data["oversized"], 1)
        self.assertEqual(data["decoded"], 0)
        self.assertIn("16 MiB", data["errors"][0]["reason"])

    def test_bad_usage_and_missing_files(self):
        self.assertEqual(self.invoke().returncode, 2)
        self.assertEqual(self.invoke("--nonsense", self.base).returncode, 2)
        self.assertEqual(self.invoke("/no/such/package").returncode, 1)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_prop_audit.py <path to spore_prop_audit executable>")
    unittest.main(argv=[sys.argv[0]], verbosity=2)
