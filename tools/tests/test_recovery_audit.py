#!/usr/bin/env python3
"""Synthetic-only regression tests for reconstruction source auditing."""
from pathlib import Path
import tempfile
import unittest
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from recovery_audit import audit, parse_record, to_markdown


class AuditTests(unittest.TestCase):
    def test_manifest_formats(self):
        self.assertEqual(
            parse_record("manifest.txt", "src/game/x.cpp ?Func@@YAXXZ 00ab1234 /O2"),
            "00ab1234",
        )
        self.assertEqual(parse_record("nonmatching.txt", "00ABCDEF 12 reason"), "00abcdef")
        self.assertEqual(parse_record("partial.txt", "0x00aabbcc incomplete"), "00aabbcc")
        self.assertIsNone(parse_record("manifest.txt", "# heading"))
        self.assertIsNone(parse_record("manifest.txt", "bad metadata"))

    def test_source_module_totals_and_annotation_duplicates(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            a = root / "src" / "game" / "SPCreature"
            b = root / "src" / "game" / "SPTerrain"
            a.mkdir(parents=True)
            b.mkdir()
            (a / "FakeMigrated.cpp").mkdir()  # Some real migration dirs end with .cpp
            (a / "alpha.cpp").write_text(
                "// @ 0x00abc123\nvoid alpha() {}\n"
                "// @ 0x00abc124\nvoid beta() {}\n",
                encoding="utf-8",
            )
            (b / "terrain.cpp").write_text("// @ 0x00abc123\n", encoding="utf-8")
            (a / "manifest.txt").write_text(
                "# source symbol va flags\nsrc/game/a.cpp symbol 00abc123 /O2\n",
                encoding="utf-8",
            )
            (a / "nonmatching.txt").write_text("00abc124 5 equivalent\n", encoding="utf-8")
            (a / "partial.txt").write_text("00abc999 partial\n", encoding="utf-8")
            (b / "manifest.txt").write_text("src/game/b.cpp symbol 00abc125 /O2\n", encoding="utf-8")
            result = audit(root)
            totals = result["summary"]
            self.assertEqual(totals["modules"], 2)
            self.assertEqual(totals["cpp_files"], 2)
            self.assertEqual(totals["address_markers"], 3)
            self.assertEqual(totals["distinct_annotated_addresses"], 2)
            self.assertEqual(totals["repeated_annotated_addresses"], 1)
            self.assertEqual(totals["known_status_addresses"], 4)
            self.assertEqual(totals["byte_exact_records"], 2)
            self.assertIn("SPCreature", to_markdown(result))
            self.assertEqual([m["module"] for m in result["modules"]],
                             ["SPCreature", "SPTerrain"])


if __name__ == "__main__":
    unittest.main()
