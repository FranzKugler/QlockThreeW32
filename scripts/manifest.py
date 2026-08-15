"""
manifest
Writes the update manifest the clocks poll: a few hundred bytes naming the
version of a channel and where its two images are, instead of the ~30 KB the
GitHub releases API would answer with.

Called by .github/workflows/release.yml, but deliberately a plain script so it
can be run by hand to see what a release would look like:

    python scripts/manifest.py --channel edge --version 2.0.0-4-gabc123 \\
        --base-url https://example/download --notes "..." \\
        --out manifest.json \\
        .pio/build/seeed_xiao_esp32s3/firmware.bin \\
        .pio/build/seeed_xiao_esp32s3/littlefs.bin

The SHA-256 is what the clock checks the download against. It travels over the
same connection as the image, so it guards against a truncated or corrupted
transfer, not against someone who can rewrite both - see CLAUDE.md.

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  2.0
@created  15.8.2026
@updated  15.8.2026
"""

import argparse
import datetime
import hashlib
import json
import os
import sys


def describe(path, base_url):
    """Name, size, digest and download URL of one image."""
    digest = hashlib.sha256()
    with open(path, "rb") as image:
        for block in iter(lambda: image.read(65536), b""):
            digest.update(block)

    name = os.path.basename(path)
    return {
        "url": "%s/%s" % (base_url.rstrip("/"), name),
        "size": os.path.getsize(path),
        "sha256": digest.hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description="Write the OTA update manifest.")
    parser.add_argument("--channel", required=True, choices=["stable", "edge"])
    parser.add_argument("--version", required=True)
    parser.add_argument("--notes", default="")
    parser.add_argument("--base-url", required=True, help="where the images will be served from")
    parser.add_argument("--out", default="manifest.json")
    parser.add_argument("firmware", help="path to firmware.bin")
    parser.add_argument("filesystem", help="path to littlefs.bin")
    args = parser.parse_args()

    for path in (args.firmware, args.filesystem):
        if not os.path.isfile(path):
            sys.exit("no such image: %s" % path)

    manifest = {
        "channel": args.channel,
        "version": args.version,
        "notes": args.notes,
        "built": datetime.datetime.now(datetime.timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z"),
        "firmware": describe(args.firmware, args.base_url),
        "filesystem": describe(args.filesystem, args.base_url),
    }

    with open(args.out, "w") as out:
        json.dump(manifest, out, indent=2)
        out.write("\n")

    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
