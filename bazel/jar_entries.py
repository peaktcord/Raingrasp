"""Generate the native ZIP reader's expected JAR-entry report independently."""

import argparse
import pathlib
import zipfile


# Match the small historical checksum in inflate_jar_dump.cpp. Its offset is
# intentionally retained for compatibility even though it differs from the
# published FNV-1a offset basis.
FNV_OFFSET_BASIS = 1469598103934665603
FNV_PRIME = 0x100000001B3


def fnv1a64(payload: bytes) -> int:
    digest = FNV_OFFSET_BASIS
    for byte in payload:
        digest ^= byte
        digest = (digest * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return digest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", action="append", default=[])
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if not args.input:
        parser.error("at least one --input JAR is required")

    lines: list[str] = []
    for jar_path_text in sorted(args.input):
        jar_path = pathlib.Path(jar_path_text)
        with zipfile.ZipFile(jar_path) as archive:
            for entry in archive.infolist():
                if entry.is_dir():
                    continue
                payload = archive.read(entry)
                lines.append(
                    f"{jar_path.name}\t{entry.filename}\t{len(payload)}\t{fnv1a64(payload):016x}"
                )

    pathlib.Path(args.output).write_text(
        "\n".join(sorted(lines)) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
