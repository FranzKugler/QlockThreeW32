/**
 * Version
 * Single source of truth for the firmware version. It is reported by
 * GET /ota/status, shown in the "Update" tab of the web UI and will be the
 * value compared against the update manifest once the GitHub path exists.
 *
 * scripts/version.py overrides FIRMWARE_VERSION at build time with the output
 * of `git describe --tags`, so a tagged build always reports what it actually
 * is. The value below is the fallback used when the working copy has no tags,
 * when git is unavailable, and by IDE code models (which do not run the build
 * script). Keep it in sync with the version in package.json, which ends up in
 * the filesystem image as /version.json.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
#ifndef VERSION_H
#define VERSION_H

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "2.2.2"
#endif

#endif
