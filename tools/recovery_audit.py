#!/usr/bin/env python3
"""Offline audit of integrated Spore reconstruction, no retail files required.

Inventories tracked src/game modules, address annotations and the existing
matching ledgers. The report measures source organization, NOT game playability
or independently linked code. Results are deterministic and CI-friendly.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re

VA = re.compile(r"(?<![0-9a-fA-F])(?:0x)?([0-9a-fA-F]{8})(?![0-9a-fA-F])")
MARK = re.compile(r"//\s*@\s*0x([0-9a-fA-F]{8})")
KINDS = {"manifest.txt": "byte_exact", "nonmatching.txt": "equivalent",
         "partial.txt": "partial"}


def parse_record(filename: str, line: str) -> str | None:
    if not line.strip() or line.lstrip().startswith("#"):
        return None
    fields = line.split()
    if filename == "manifest.txt":
        if len(fields) < 3:
            return None
        candidate = fields[2]
    else:
        candidate = fields[0]
    result = VA.fullmatch(candidate)
    return result.group(1).lower() if result else None


def audit(root: Path) -> dict:
    game = root / "src" / "game"
    if not game.is_dir():
        raise FileNotFoundError(f"Not a reconstruction tree: {game}")
    modules = defaultdict(lambda: {
        "cpp_files": 0, "source_bytes": 0, "address_markers": 0,
        "byte_exact": set(), "equivalent": set(), "partial": set(),
        "malformed_records": 0,
    })
    global_annotations = Counter()
    for file in sorted(game.rglob("*.cpp")):
        key = file.relative_to(game).parts[0]
        info = modules[key]
        info["cpp_files"] += 1
        info["source_bytes"] += file.stat().st_size
        # Iteration over lines avoids loading every very large TU at once.
        with file.open("r", encoding="utf-8", errors="replace") as f:
            for line in f:
                for match in MARK.finditer(line):
                    info["address_markers"] += 1
                    global_annotations[match.group(1).lower()] += 1
    for file in sorted(game.rglob("*.txt")):
        kind = KINDS.get(file.name)
        if kind is None:
            continue
        key = file.relative_to(game).parts[0]
        info = modules[key]
        with file.open("r", encoding="utf-8", errors="replace") as f:
            for line in f:
                value = parse_record(file.name, line)
                if value is None:
                    if line.strip() and not line.lstrip().startswith("#"):
                        info["malformed_records"] += 1
                    continue
                info[kind].add(value)
    result = []
    for name, info in sorted(modules.items()):
        row = {"module": name}
        for field in ("cpp_files", "source_bytes", "address_markers",
                      "malformed_records"):
            row[field] = info[field]
        for field in KINDS.values():
            row[field] = len(info[field])
        row["known_addresses"] = len(
            info["byte_exact"] | info["equivalent"] | info["partial"]
        )
        result.append(row)
    summary = {
        "modules": len(result),
        "cpp_files": sum(m["cpp_files"] for m in result),
        "source_bytes": sum(m["source_bytes"] for m in result),
        "address_markers": sum(m["address_markers"] for m in result),
        "distinct_annotated_addresses": len(global_annotations),
        "repeated_annotated_addresses": sum(
            1 for count in global_annotations.values() if count > 1
        ),
        "known_status_addresses": sum(m["known_addresses"] for m in result),
        "byte_exact_records": sum(m["byte_exact"] for m in result),
        "equivalent_records": sum(m["equivalent"] for m in result),
        "partial_records": sum(m["partial"] for m in result),
        "malformed_records": sum(m["malformed_records"] for m in result),
    }
    return {"schema": 1, "root": "src/game", "summary": summary, "modules": result}


def to_markdown(report: dict) -> str:
    summary = report["summary"]
    lines = [
        "# Reconstructed Spore source inventory",
        "",
        "Generated from tracked source and matching manifests only. "
        "It is not a linked-game completeness estimate.",
        "",
        f"- C++ units: **{summary['cpp_files']:,}**",
        f"- Source volume: **{summary['source_bytes'] / 1048576:.2f} MiB**",
        f"- Address markers: **{summary['address_markers']:,}**",
        f"- Unique addresses with known matching status (sum by module): "
        f"**{summary['known_status_addresses']:,}**",
        f"- Malformed status records: **{summary['malformed_records']:,}**",
        "",
        "| Module | C++ files | Exact | Equivalent | Partial | Annotated |",
        "|:--|--:|--:|--:|--:|--:|",
    ]
    for row in sorted(report["modules"], key=lambda m: -m["source_bytes"]):
        lines.append(
            f"| {row['module']} | {row['cpp_files']} | {row['byte_exact']} "
            f"| {row['equivalent']} | {row['partial']} | {row['address_markers']} |"
        )
    lines.extend(["", "Do not infer runnable engine progress from these counts.", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--json", type=Path, help="Write machine-readable inventory")
    parser.add_argument("--markdown", type=Path, help="Write human-readable report")
    parser.add_argument("--check", action="store_true",
                        help="Fail if no integrated C++ units or no annotated addresses")
    args = parser.parse_args()
    report = audit(args.root)
    payload = json.dumps(report, indent=2) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(payload, encoding="utf-8")
    else:
        print(json.dumps(report["summary"], indent=2))
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(to_markdown(report), encoding="utf-8")
    if args.check and (report["summary"]["cpp_files"] == 0 or
                       report["summary"]["address_markers"] == 0):
        raise SystemExit("No recovered C++ source units or annotated functions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
