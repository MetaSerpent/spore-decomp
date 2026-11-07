# File formats (observed; confirm each field against the exe loader before marking verified)

## DBPF (.package) — VERIFIED against exe decompile + corpus (108,918 entries, 70,149 compressed)
Implementation: `src/resource/DBPF.cpp`, each function annotated with its original address.
Exe rules (beyond the basic layout below):
- Header accepted iff magic, major<4, count<0x7FFFFFF, indexOffset<fileSize,
  (u32)(indexOffset+indexSize)<=fileSize, indexMinor(0x3C)!=0. If legacy offset 0x28 is also
  set, the result is instead (0x40==0x28 && 0x44==0).
- If the header fails, the file is scanned for the 16-byte marker
  `80 9D 88 EC 8F 24 03 6C C9 A6 31 56 5B CF 77 20`; the package starts right after it and
  index/record offsets are relative to that position (integrity checks still use absolute ones).
- Index: flags bit 2 is required and the constant third field must be 0. Entries are
  variable-length: the {u16 compression, u8 committed} dword exists only when size has bit 31;
  otherwise compression = (diskSize != memSize) ? 0xFFFF : 0. Any nonzero compression = RefPack.
- Duplicate keys: first entry wins (hash_map insert-unique, hash = instance ^ group).
- Integrity: every entry with diskSize != 0 must lie inside [0, fileSize).
- RefPack header must satisfy (b0 & 0x1F) == 0x10 && b1 == 0xFB (the 0x01 flag is rejected);
  declared size must equal the index memSize. The exe's decoder has no bounds checks.
- Header 0x60 bytes LE: `DBPF`, major@0x04 (=3), minor@0x08 (=0), index count@0x24,
  index size@0x2C, index minor@0x3C (=3), index offset@0x40.
- Index: u32 flags; bits 0..2 mean type/group/unknown is constant, and each constant
  value follows in that order. Entry = [non-constant of type,group,unknown], instance,
  offset, diskSize|0x80000000, memSize, u16 compressed (0xFFFF = RefPack), u16 committed.
- RefPack: `flags, 0xFB, size(3 or 4 BE; 4 if flags&0x80)`, optional compressed-size field
  if flags&0x01. Standard QFS opcodes.

## .prop (type 0x00B1B104) — OBSERVED
- BE u32 property count. Each record: BE u32 id, BE u16 type, BE u16 flags.
- `flags & 0x30` → array: BE u32 count, BE u32 itemSize, then items.
- Payloads are **little-endian**: key = {instance, type, group} LE, vector floats LE.
  Single (non-array) key / vector3 values take 16 bytes (the last 4 are uninitialized padding).
- string8/string16: BIG-ENDIAN u32 character count, then byte text / UTF-16LE characters, no terminator.
- int32/uint32/float scalars and arrays use BIG-ENDIAN values; key and vector fields are LITTLE-ENDIAN.
- Array specifiers include 0x009C (recognized by flags & 0x30). The nominal array item size for variable-length strings is often 16 and does not represent their true byte span.
- A new conservative, bounds-checked subset reader lives in `src/resource/PropertyList.cpp` with synthetic regression tests. Supported: bool, int32, uint32, float, string8/16, key, vector2/3/4, RGB/RGBA, localized texts, bounding boxes, and their documented array forms. Transform (0x38) is not yet supported; unsupported records produce explicit errors rather than guessing record sizes.
- Format cross-check: https://modthesims.info/wiki.php?title=Spore:00B1B104 (community reverse-engineering observations; validate per installed build).
- Observed types: 0x13 string16, 0x20 key, 0x31 vector3.
- Group/instance 0x01C7AC81 = file-extension ↔ type-ID map (string16 arrays).

### Exe cross-references (pre-Ghidra, from raw disassembly)
- DBPF header validation @ 0x008D8DB6: magic=='DBPF', major<=3, count<0x7FFFFFF,
  indexOffset<fileSize, indexOffset+indexSize<=fileSize. (Second magic use @ 0x008D8F84.)
- FNV-1 prime 0x01000193 used @ 0x0050E879 in an /Od-looking hash-table lookup
  (bucket = hash & 0x7FFF, 12-byte buckets). Mixed optimization levels per module.

## .prop inheritance (read-only reconstruction)

`src/resource/PropertyResolver.cpp` resolves the `parent` resource key (ID `0x00B2CCCB`) recursively through mounted package overlays. It normalizes the parent's `.prop` type ID, loads ancestors first and then applies child overrides. It preserves provenance, enforces depth and cycle limits, and leaves the caller's result empty on failure. Synthetic DBPF fixtures test overrides, patch layering and malformed chains. Real retail parity still needs per-build validation.
