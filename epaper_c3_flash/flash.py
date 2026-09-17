#!/usr/bin/env python3
"""Flash an ESP32-C3 Super Mini from the command line, bypassing the Arduino IDE.

    python flash.py --erase                 # uploader sketch, LittleFS wiped
    python flash.py                         # same, filesystem kept
    python flash.py epaper_clean --erase    # one of the diagnostic sketches
    python flash.py --port COM7 --compile-only

Full arduino-cli output always lands in flash.log next to this script; only the
tail is echoed.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
DEFAULT_CLI = Path(
    r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
)

FQBN_TEMPLATE = (
    "esp32:esp32:esp32c3:CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,"
    "FlashSize=4M,PartitionScheme=default,UploadSpeed=921600,EraseFlash={erase}"
)


def resolve_sketch(name):
    """Accept a bare name, a repo-relative path, or an absolute path."""
    candidates = [Path(name), ROOT / name, REPO / name]
    for path in candidates:
        if path.is_dir() and any(path.glob("*.ino")):
            return path.resolve()
    raise SystemExit(f"No sketch folder with a .ino found for '{name}'")


def resolve_cli(explicit):
    if explicit:
        path = Path(explicit)
        if not path.exists():
            raise SystemExit(f"arduino-cli not found at {path}")
        return path
    if DEFAULT_CLI.exists():
        return DEFAULT_CLI
    found = shutil.which("arduino-cli")
    if found:
        return Path(found)
    raise SystemExit(
        f"arduino-cli not found at {DEFAULT_CLI} and not on PATH; pass --cli"
    )


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("sketch", nargs="?", default="epaper_web_uploader")
    parser.add_argument("--port", default="COM11")
    parser.add_argument(
        "--erase",
        action="store_true",
        help="wipe LittleFS along with the flash; clears a corrupt /screen.bin",
    )
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--cli", help="path to arduino-cli.exe")
    parser.add_argument("--tail", type=int, default=30)
    args = parser.parse_args()

    cli = resolve_cli(args.cli)
    sketch = resolve_sketch(args.sketch)
    log = ROOT / "flash.log"

    # Without a fixed cache path arduino-cli recompiles the whole ESP32 core
    # (~10 min) on every invocation; the IDE keeps its own warm cache we cannot
    # reach, which is why the same build is seconds there.
    cache = ROOT / ".build-cache"
    build = ROOT / ".build"
    cache.mkdir(exist_ok=True)
    build.mkdir(exist_ok=True)

    erase = "all" if args.erase else "none"
    cmd = [
        str(cli),
        "compile",
        "--fqbn",
        FQBN_TEMPLATE.format(erase=erase),
        "--build-cache-path",
        str(cache),
        "--build-path",
        str(build),
        "--log-level",
        "warn",
        str(sketch),
    ]
    if not args.compile_only:
        cmd += ["--upload", "--port", args.port, "--verify"]

    print(f"sketch     : {sketch}")
    print(f"port       : {args.port}")
    print(f"erase flash: {erase}")
    print(f"log        : {log}")
    print("\nCompiling (the first run populates the cache, ~10 minutes)...", flush=True)

    with log.open("w", encoding="utf-8", errors="replace") as handle:
        code = subprocess.call(cmd, stdout=handle, stderr=subprocess.STDOUT)

    tail = log.read_text(encoding="utf-8", errors="replace").splitlines()[-args.tail :]
    print("\n".join(tail))

    if code != 0:
        print(f"\narduino-cli exited with {code} - see {log}", file=sys.stderr)
        return code

    if args.compile_only:
        print("\nCompiled. Nothing was uploaded.")
    else:
        print("\nFlashed and verified. Serial monitor at 115200 to watch the boot.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
