"""Run a private-data tool and capture a platform-stable text artifact."""

import argparse
import pathlib
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", required=True)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--arg", action="append", default=[])
    parser.add_argument("--tool-writes-output", action="store_true")
    args = parser.parse_args()

    command = [args.tool, *args.arg, args.input]
    if args.tool_writes_output:
        command.extend(["--write", args.output])
    result = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode:
        sys.stderr.buffer.write(result.stderr)
        return result.returncode

    payload = pathlib.Path(args.output).read_bytes() if args.tool_writes_output else result.stdout
    normalized = payload.replace(b"\r\n", b"\n")
    if not args.tool_writes_output:
        normalized = b"\n".join(line.rstrip(b" \t") for line in normalized.split(b"\n"))
    normalized = normalized.rstrip(b"\n") + b"\n"
    pathlib.Path(args.output).write_bytes(normalized)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
