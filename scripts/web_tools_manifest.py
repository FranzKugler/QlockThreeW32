"""
web_tools_manifest
Writes the manifest ESP Web Tools reads to flash a blank chip from a browser -
see docs/index.html, the "first flashing" page this feeds. A completely
different document from manifest.py's: that one is polled by a clock that
already runs this firmware and only ever touches the two OTA partitions; this
one is read once, by a browser that has never met the chip, and has to name
every part a **factory-fresh** XIAO ESP32-S3 needs - the bootloader and the
partition table themselves included, which nothing else here ever writes.

Called by .github/workflows/release.yml, but a plain script so it can be run
by hand:

    python scripts/web_tools_manifest.py --version 2.3.4 \\
        --base-url https://example/download \\
        --out esp-web-tools-manifest.json \\
        .pio/build/seeed_xiao_esp32s3

Three of the five offsets are not in partitions.csv at all - the bootloader,
the partition table and boot_app0 (which seeds otadata to boot app0 first) are
placed by the Arduino build tooling itself, not by this project's table, and
are the same on every XIAO ESP32-S3 build: see
framework-arduinoespressif32/tools/pioarduino-build.py's FLASH_EXTRA_IMAGES for
where 0x0/0x8000/0xe000 come from. The other two - app0 and spiffs - are
parsed out of partitions.csv rather than repeated here, so a changed table
cannot silently drift out of step with what this writes.

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  1.0
@created  30.8.2026
@updated  30.8.2026
"""

import argparse
import csv
import json
import os
import sys

# Fixed by the Arduino build tooling for every ESP32-S3 target, not by this
# project's partition table - see the module docstring.
BOOTLOADER_OFFSET = 0x0
PARTITION_TABLE_OFFSET = 0x8000
BOOT_APP0_OFFSET = 0xE000

PARTITIONS_CSV = os.path.join(os.path.dirname(__file__), "..", "partitions.csv")


def table_offsets():
    """{name: offset} out of partitions.csv, ints rather than the hex strings
    the file spells them as."""
    offsets = {}
    with open(PARTITIONS_CSV, newline="") as csv_file:
        for row in csv.reader(csv_file):
            row = [cell.strip() for cell in row]
            if not row or not row[0] or row[0].startswith("#"):
                continue
            name, offset = row[0], row[3]
            offsets[name] = int(offset, 16)
    return offsets


def main():
    parser = argparse.ArgumentParser(
        description="Write the ESP Web Tools manifest for flashing a blank chip."
    )
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-url", required=True, help="where the images will be served from")
    parser.add_argument("--out", default="esp-web-tools-manifest.json")
    parser.add_argument("--boot-app0", required=True, help="path to boot_app0.bin from the framework package")
    parser.add_argument("build_dir", help="e.g. .pio/build/seeed_xiao_esp32s3")
    args = parser.parse_args()

    offsets = table_offsets()
    for needed in ("app0", "spiffs"):
        if needed not in offsets:
            sys.exit("partitions.csv has no %r partition - is this still the XIAO table?" % needed)

    parts = [
        (BOOTLOADER_OFFSET, os.path.join(args.build_dir, "bootloader.bin")),
        (PARTITION_TABLE_OFFSET, os.path.join(args.build_dir, "partitions.bin")),
        (BOOT_APP0_OFFSET, args.boot_app0),
        (offsets["app0"], os.path.join(args.build_dir, "firmware.bin")),
        (offsets["spiffs"], os.path.join(args.build_dir, "littlefs.bin")),
    ]
    for _, path in parts:
        if not os.path.isfile(path):
            sys.exit("no such image: %s" % path)

    base = args.base_url.rstrip("/")
    manifest = {
        "name": "QlockThreeW32",
        "version": args.version,
        # A browser here has never met this chip - offering the erase
        # checkbox is right every time, not just the first.
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": [
                    {"path": "%s/%s" % (base, os.path.basename(path)), "offset": offset}
                    for offset, path in parts
                ],
            }
        ],
    }

    with open(args.out, "w") as out:
        json.dump(manifest, out, indent=2)
        out.write("\n")

    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
