"""
version
Derives FIRMWARE_VERSION from the git tag, so an image always reports the
commit it was built from instead of a number someone has to remember to bump.

Runs as a PlatformIO pre-build script (see extra_scripts in platformio.ini).
`git describe --tags --dirty` yields "2.0.0" on a tagged commit and something
like "2.0.0-4-g2deb356-dirty" on the work in progress after it. If the repo has
no tags yet, or git is not available, the fallback from src/Version.h applies.

Whatever it settles on is written to .pio/version.txt as well. The release
workflow reads that file rather than working the version out a second time -
two implementations of the same rule would eventually disagree, and the
manifest has to name exactly what is in the image.

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  2.0
@created  15.8.2026
@updated  15.8.2026
"""

import os
import re
import subprocess

Import("env")  # noqa: F821 - injected by PlatformIO

PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
VERSION_HEADER = os.path.join(PROJECT_DIR, "src", "Version.h")
VERSION_STAMP = os.path.join(PROJECT_DIR, ".pio", "version.txt")


def git_version():
    """Version from the nearest release tag, or None when there is nothing usable.

    Only tags named like a version count. The release workflow keeps the edge
    build under a fixed tag `edge`, recreated on the commit of every build - so
    without --match it is always the nearest tag, `git describe` answers "edge",
    and every build from then on falls back to the version in Version.h.
    """
    try:
        described = subprocess.check_output(
            ["git", "describe", "--tags", "--dirty", "--match", "v[0-9]*"],
            cwd=PROJECT_DIR,
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).strip()
    except Exception:
        # No git, not a repository, or no tag reachable from HEAD.
        return None

    # Tags may be written as "v2.0.0"; the manifest compares bare versions.
    if described.startswith("v"):
        described = described[1:]

    # Guard against a tag that is not a version at all.
    return described if described[:1].isdigit() else None


def fallback_version():
    """The value compiled in when no tag applies, straight from Version.h."""
    try:
        with open(VERSION_HEADER, "r") as header:
            match = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', header.read())
            if match:
                return match.group(1)
    except Exception:
        pass
    return "0.0.0"


version = git_version()

if version:
    print("Firmware version from git: %s" % version)
    env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
else:
    version = fallback_version()
    print("No usable git tag, keeping the fallback version %s from src/Version.h" % version)

try:
    os.makedirs(os.path.dirname(VERSION_STAMP), exist_ok=True)
    with open(VERSION_STAMP, "w") as stamp:
        stamp.write(version + "\n")
except Exception as error:
    # Only the release workflow needs this; a local build carries on regardless.
    print("Could not write %s: %s" % (VERSION_STAMP, error))
