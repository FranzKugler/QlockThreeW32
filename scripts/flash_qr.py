"""
flash_qr
Generates the two QR codes on the flashing page (docs/index.html): one to
join the clock's own setup access point, one to open the clock once it is on
the real network.

No service, no key, no subscription - a QR code is just a Reed-Solomon-coded
bitmap of the text it carries, and the `qrcode` package computes that offline
from a fixed string. Both codes here point at *this project's own* constants
(the AP name and password WiFiManager starts with, see setup() in
main .cpp, and the hostname every unconfigured clock answers to before
someone renames it), so generating them once and committing the result is the
same call as zones.json or the panel letters: nothing here changes unless the
project's own defaults do.

Plain black on white, deliberately not the page's accent blue - colour inside
a QR code only narrows the contrast a camera has to work with, and this one
gets scanned once, on a phone that is mid-setup and not yet on any network.

    pip install qrcode pillow
    python scripts/flash_qr.py

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  1.0
@created  30.8.2026
@updated  30.8.2026
"""

import os
import sys

try:
    import qrcode
except ImportError:
    sys.exit("qrcode is missing - run: pip install qrcode pillow")

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS_DIR = os.path.join(PROJECT_DIR, "docs")

# The AP WiFiManager opens in setup() when no network is stored - see
# wifiManager.autoConnect(settings.getHostname()) in main .cpp. A single
# argument to autoConnect() means no password; "QlockThreeW32" is
# Settings::Hostname's default, which is what an unconfigured clock is still
# running at exactly the point this code is meant to be scanned.
AP_SSID = "QlockThreeW32"

# mDNS name for that same default hostname, once the clock has joined a real
# network - see "The clock's name" in CLAUDE.md.
CLOCK_URL = "http://qlockthreew32.local"

# The WiFi QR payload format phones' own camera apps recognise (iOS 11+,
# Android 10+) - no app to install, which is the whole point of this page.
WIFI_PAYLOAD = "WIFI:T:nopass;S:%s;;" % AP_SSID

OUTPUTS = [
    ("qr-ap.png", WIFI_PAYLOAD),
    ("qr-clock.png", CLOCK_URL),
]


def make(payload):
    return qrcode.make(payload, border=2).convert("RGB")


def main():
    os.makedirs(DOCS_DIR, exist_ok=True)
    for name, payload in OUTPUTS:
        path = os.path.join(DOCS_DIR, name)
        make(payload).save(path, optimize=True)
        print("%-14s %-40s %6d bytes" % (name, payload, os.path.getsize(path)))


if __name__ == "__main__":
    main()
