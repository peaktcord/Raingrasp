"""Write the SHA-256 digest of one file in a stable text format."""

import argparse
import hashlib
import pathlib


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    digest = hashlib.sha256(pathlib.Path(args.input).read_bytes()).hexdigest()
    pathlib.Path(args.output).write_text(digest + "\n", encoding="ascii", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
