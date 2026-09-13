"""Create a deterministic identity report for verified private JARs."""

import argparse
import hashlib
import json
import pathlib
import zipfile


def parse_manifest(data: bytes) -> dict[str, str]:
    unfolded: list[str] = []
    for line in data.decode("utf-8", errors="replace").splitlines():
        if line.startswith(" ") and unfolded:
            unfolded[-1] += line[1:]
        else:
            unfolded.append(line)

    fields: dict[str, str] = {}
    for line in unfolded:
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        fields[key] = value.lstrip()
    return fields


def content_tree_digest(archive: zipfile.ZipFile) -> str:
    tree = hashlib.sha256()
    entries = sorted(
        (entry for entry in archive.infolist() if not entry.is_dir()),
        key=lambda entry: entry.filename,
    )
    for entry in entries:
        payload_digest = hashlib.sha256(archive.read(entry)).digest()
        tree.update(entry.filename.encode("utf-8"))
        tree.update(b"\0")
        tree.update(entry.file_size.to_bytes(8, byteorder="big"))
        tree.update(payload_digest)
    return tree.hexdigest()


def inspect_jar(path: pathlib.Path) -> dict[str, object]:
    archive_bytes = path.read_bytes()
    with zipfile.ZipFile(path) as archive:
        files = [entry for entry in archive.infolist() if not entry.is_dir()]
        manifest = parse_manifest(archive.read("META-INF/MANIFEST.MF"))
        midlet_parts = [part.strip() for part in manifest.get("MIDlet-1", "").split(",")]
        main_class = midlet_parts[-1] if midlet_parts else ""
        return {
            "archive_sha256": hashlib.sha256(archive_bytes).hexdigest(),
            "archive_size": len(archive_bytes),
            "canonical_name": path.name,
            "content_tree_sha256": content_tree_digest(archive),
            "entry_count": len(files),
            "midlet": {
                "configuration": manifest.get("MicroEdition-Configuration", ""),
                "main_class": main_class,
                "name": manifest.get("MIDlet-Name", ""),
                "profile": manifest.get("MicroEdition-Profile", ""),
                "vendor": manifest.get("MIDlet-Vendor", ""),
                "version": manifest.get("MIDlet-Version", ""),
            },
        }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", action="append", default=[])
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if not args.input:
        parser.error("at least one --input JAR is required")

    report = {
        "format_version": 1,
        "jars": [inspect_jar(pathlib.Path(path)) for path in sorted(args.input)],
    }
    payload = json.dumps(report, indent=2, sort_keys=True) + "\n"
    pathlib.Path(args.output).write_text(payload, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
