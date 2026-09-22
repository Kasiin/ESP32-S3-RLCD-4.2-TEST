"""Validate a release and flash split images without a full-chip erase."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
IMAGES = (("0x0", "bootloader.bin"), ("0x8000", "partition-table.bin"),
          ("0x10000", "rlcd_hardware_test.bin"))


def verify(directory):
    expected = {}
    for line in (directory / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        digest, name = line.split(maxsplit=1)
        name = name.lstrip("*")
        if Path(name).name != name or name in expected:
            raise ValueError("Invalid or duplicate checksum filename")
        expected[name] = digest.lower()
    for _, name in IMAGES:
        if name not in expected:
            raise ValueError(f"Missing checksum: {name}")
    for name, digest in expected.items():
        actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        if actual != digest:
            raise ValueError(f"SHA256 mismatch: {name}")
        print(f"SHA256 OK: {name}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="ESP32 USB port, e.g. COM15")
    parser.add_argument("--baud", type=int, default=460800)
    parser.add_argument("--firmware-dir", type=Path, default=ROOT / "firmware/v0.2.0")
    parser.add_argument("--dry-run", action="store_true", help="Verify only; do not access USB")
    args = parser.parse_args()
    directory = args.firmware_dir.resolve()
    verify(directory)
    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port,
               "--baud", str(args.baud), "--before", "default_reset", "--after", "hard_reset",
               "write_flash", "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "16MB"]
    for address, name in IMAGES:
        command.extend((address, str(directory / name)))
    print(subprocess.list2cmdline(command), flush=True)
    if not args.dry_run:
        subprocess.run(command, check=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"Flash aborted: {exc}", file=sys.stderr)
        sys.exit(1)
