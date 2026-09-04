/**
 * Portal
 * The setup portal: the only face the clock has before it is on a network.
 *
 * **Why this is not WiFiManager.** WiFiManager works and is three lines to
 * use, and what it serves is its own HTML - a page that can be given the
 * project's colours through a stylesheet and no more than that. The setup
 * portal is the first thing anybody ever sees of this clock, and "same
 * colours as the app" is not the same thing as "the app". So the portal here
 * is a second page out of the same Svelte build: same app.css, same cards,
 * same network list component the WLAN tab uses. There is no second design
 * to keep in step, because there is no second design.
 *
 * The cost is this file, and it is the smaller half of the bargain: an
 * access point, a DNS server that answers every name with our own address,
 * three endpoints and a state machine for the connection attempt.
 *
 * **Nothing else is registered while the portal is up.** main .cpp calls
 * either Portal::begin() or Web::begin() + Files::begin() + Nvs::begin() +
 * Lab::begin() + Ota::begin(), never both. An open access point that anyone
 * within radio range can join is not somewhere to expose a filesystem
 * explorer or a firmware upload, and the expert lock is no help here: a
 * clock that has never been on a network has never been enrolled either.
 *
 * How it ends: the credentials that work are stored by the WiFi driver
 * itself (esp_wifi keeps them in its own NVS namespace, which is also where
 * WiFi.begin() with no arguments reads them from on the next boot), and the
 * clock restarts into normal operation. A restart rather than switching
 * modes in place, because everything that reads the network once - mDNS,
 * espota, the SNTP client - would otherwise have to be taught to start late.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  1.0
 * @created  30.8.2026
 * @updated  30.8.2026
 */
#ifndef PORTAL_H
#define PORTAL_H

#include <Arduino.h>

// How long a connection attempt from the portal is given before it counts as
// failed. Longer than the 20 s a switch gets in WebRoutes, because somebody
// is standing there watching this one and a router that is slow to hand out
// a lease is not a wrong password.
#define PORTAL_CONNECT_TIMEOUT 25000

// How long the portal stays up when the clock *had* credentials and merely
// could not reach the network - after that it restarts and tries them again.
// A router that was rebooting must not leave the clock sitting in setup mode
// for good. With no stored credentials there is nothing to go back to, so
// the portal stays up indefinitely; see Portal::poll().
#define PORTAL_RETRY_AFTER (10 * 60 * 1000UL)

namespace Portal
{
    /**
     * Brings up the access point, the DNS catch-all and the portal's routes,
     * and starts the server. Call instead of Web::begin() and friends.
     *
     * @param hadCredentials  whether the clock had a stored network it simply
     *                        could not reach. Decides whether the portal ever
     *                        gives up and retries - see PORTAL_RETRY_AFTER.
     */
    void begin(bool hadCredentials);

    /** True once begin() has run. The rest of the firmware asks before acting. */
    bool active();

    /** DNS and the connection attempt. Call from loop() while active(). */
    void poll();
}

#endif
