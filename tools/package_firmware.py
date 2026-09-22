"""Package existing build outputs; never connect to or change a device."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--version", default="local")
    parser.add_argument("--provenance", default="Locally built; not necessarily hardware validated.")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9._-]+", args.version):
        parser.error("Version must be a plain filename-safe label")
    sources = {"bootloader.bin": "bootloader/bootloader.bin",
               "partition-table.bin": "partition_table/partition-table.bin",
               "rlcd_hardware_test.bin": "rlcd_hardware_test.bin"}
    for relative in sources.values():
        if not (args.build_dir / relative).is_file():
            parser.error(f"Missing build artifact: {relative}")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    for name, relative in sources.items():
        shutil.copy2(args.build_dir / relative, output / name)
    merged_name = f"rlcd-hardware-test-{args.version}-merged.bin"
    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin",
               "-o", merged_name, "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "16MB",
               "0x0", "bootloader.bin", "0x8000", "partition-table.bin", "0x10000", "rlcd_hardware_test.bin"]
    subprocess.run(command, cwd=output, check=True)
    hashes = {name: hashlib.sha256((output / name).read_bytes()).hexdigest()
              for name in (*sources.keys(), merged_name)}
    (output / "SHA256SUMS").write_text("".join(f"{digest}  {name}\n" for name, digest in hashes.items()), encoding="utf-8")
    metadata = {
        "version": args.version, "chip": "esp32s3", "flash": "16MB DIO 40MHz",
        "psram": "8MB Octal 40MHz", "esp_idf": "v5.5.5",
        "provenance": args.provenance,
        "upstream": "https://github.com/LinIT-L/ESP32-S3-RLCD-BBK",
        "upstream_commit": "1c0549206f128c311103f374ee46c38b95229c91",
        "offsets": {"bootloader.bin": "0x0", "partition-table.bin": "0x8000",
                    "rlcd_hardware_test.bin": "0x10000", merged_name: "0x0"},
        "merged_warning": "Merged image overwrites NVS/PHY padding; prefer split images for upgrades.",
        "sha256": hashes,
    }
    (output / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Packaged {args.version}: {output}")


if __name__ == "__main__":
    main()
