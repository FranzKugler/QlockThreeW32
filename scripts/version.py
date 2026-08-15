"""
version
Derives FIRMWARE_VERSION from the git tag, so an image always reports the
commit it was built from instead of a number someone has to remember to bump.

Runs as a PlatformIO pre-build script (see extra_scripts in platformio.ini).
`git describe --tags --dirty` yields "2.1.0" on a tagged commit and something
like "2.1.0-4-g2deb356-dirty" on the work in progress after it. If the repo has
no tags yet, or git is not available, the define is left alone and the fallback
in src/Version.h applies.

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  2.0
@created  15.8.2026
@updated  15.8.2026
"""

import subprocess

Import("env")  # noqa: F821 - injected by PlatformIO


def git_version():
    """Version from the nearest tag, or None when there is nothing usable."""
    try:
        described = subprocess.check_output(
            ["git", "describe", "--tags", "--dirty"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).strip()
    except Exception:
        # No git, not a repository, or no tag reachable from HEAD.
        return None

    # Tags may be written as "v2.1.0"; the manifest compares bare versions.
    if described.startswith("v"):
        described = described[1:]

    # Guard against a tag that is not a version at all.
    return described if described[:1].isdigit() else None


version = git_version()
if version:
    print("Firmware version from git: %s" % version)
    env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
else:
    print("No usable git tag, keeping the fallback version from src/Version.h")
