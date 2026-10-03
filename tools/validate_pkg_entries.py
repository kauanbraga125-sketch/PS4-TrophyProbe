from pathlib import Path
import struct
import sys

pkg_path = Path(sys.argv[1] if len(sys.argv) > 1 else "IV0000-BREW00094_00-TROPHIESEX000000.pkg")
raw = pkg_path.read_bytes()
if raw[:4] != b"\x7fCNT":
    raise SystemExit("not a PS4 CNT package")

entry_count = struct.unpack_from(">I", raw, 0x10)[0]
table_off = struct.unpack_from(">I", raw, 0x18)[0]

wanted = {
    0x402: ("NPTITLE_DAT", 160),
    0x403: ("NPBIND_DAT", 532),
    0x1400: ("TROPHY00_TRP", None),
}
found = {}

for i in range(entry_count):
    off = table_off + i * 0x20
    entry_id, name_off, flags1, flags2, data_off, data_size = struct.unpack_from(">IIIIII", raw, off)
    if entry_id in wanted:
        found[entry_id] = (i, flags1, flags2, data_off, data_size)

lines = []
for entry_id, (name, expected_size) in wanted.items():
    if entry_id not in found:
        raise SystemExit(f"{name} special entry 0x{entry_id:X} missing")
    idx, flags1, flags2, data_off, data_size = found[entry_id]
    if expected_size is not None and data_size != expected_size:
        raise SystemExit(f"{name} size {data_size}, expected {expected_size}")
    if data_size <= 0:
        raise SystemExit(f"{name} is empty")
    line = f"index={idx} id=0x{entry_id:08X} name={name} flags1=0x{flags1:08X} flags2=0x{flags2:08X} offset=0x{data_off:X} size={data_size}"
    print(line)
    lines.append(line)

Path("pkg_entries.txt").write_text("\n".join(lines) + "\n")
print("PASS: complete public OpenOrbis trophy entry set is present in the PKG.")
