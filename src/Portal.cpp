/**
 * Portal
 * See Portal.h for why this exists rather than WiFiManager.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  1.0
 * @created  30.8.2026
 * @updated  30.8.2026
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>    // storing the credentials only once they have worked
#include <ArduinoJson.h>

#include "Portal.h"
#include "WebRoutes.h"   // Web::serveFromFilesystem(), so the MIME table exists once
#include "Settings.h"
#include "LogBuffer.h"

extern WebServer server;
extern Settings settings;

namespace
{
    DNSServer dns;

    bool started = false;
    bool retryStored = false;
    unsigned long startedAt = 0;

    IPAddress apIp;
    String apName;

    /** 'idle' | 'connecting' | 'connected' | 'failed', as the browser sees it. */
    enum ConnectState { IDLE, CONNECTING, CONNECTED, FAILED };
    ConnectState state = IDLE;

    String pendingSsid, pendingPass;
    unsigned long connectDeadline = 0;
    // A code, never a sentence: the portal page translates it the same way the
    // rest of the UI does - see errors.js.
    String lastError;
    String lastErrorDetail;
    // Set once the connection is up; the page shows it, then the clock goes.
    String obtainedIp;
    unsigned long restartAt = 0;

    /**
     * Whether this request is aimed at us by address.
     *
     * Every name resolves to our address while the portal is up, so a phone
     * checking for internet access arrives here asking for
     * connectivitycheck.gstatic.com. Redirecting that - rather than answering
     * it - is what makes the phone put up its "sign in to network" sheet,
     * which is the whole reason a captive portal is captive. Requests from the
     * portal page itself carry the address in Host and must be served, or the
     * page would redirect its own stylesheet.
     */
    bool addressedToUs()
    {
        String host = server.hostHeader();
        if (host.length() == 0) return true;
        // A client is free to send the port along, and a few do. Comparing the
        // whole header against the bare address would then send the portal's
        // own bundle into the redirect below - and the page would load as
        // unstyled HTML, which is the one outcome this whole module exists to
        // avoid.
        int colon = host.indexOf(':');
        if (colon >= 0) host.remove(colon);
        return host == apIp.toString();
    }

    /**
     * Writes the credentials that have just worked into the WiFi driver's own
     * store, which is where WiFi.begin() reads them from on the next boot.
     *
     * The attempt itself deliberately does not write them - see begin(), which
     * puts the driver in RAM-only mode. Without that, one typo would overwrite
     * a network the clock had been reaching perfectly well until somebody
     * opened the portal, and the retry in poll() would then restart into the
     * typo. So nothing is stored until a connection has actually come up, and
     * this is the moment it has.
     *
     * Rewriting the config the driver already holds is what commits it: with
     * storage set back to flash, esp_wifi_set_config() puts it there. Called
     * immediately before the restart, so the disconnect it may cause costs
     * nothing.
     */
    void persistCredentials()
    {
        wifi_config_t conf;
        if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK)
        {
            debugE("Portal: cannot read back the credentials to store them");
            return;
        }
        esp_wifi_set_storage(WIFI_STORAGE_FLASH);
        if (esp_wifi_set_config(WIFI_IF_STA, &conf) != ESP_OK)
        {
            debugE("Portal: storing the credentials failed");
            return;
        }
        debugA("Portal: credentials for %s stored", (const char *)conf.sta.ssid);
    }

    void redirectToPortal()
    {
        server.sendHeader("Location", String("http://") + apIp.toString() + "/", true);
        // 302 and an empty body: some captive-portal detectors follow the
        // redirect, others only look at the status, and both are satisfied.
        server.send(302, "text/plain", "");
    }

    /** What the portal page polls. Carries no secret - there is nobody to hide it from yet. */
    void sendStatus()
    {
        JsonDocument doc;
        doc["portal"]      = true;
        doc["apName"]      = apName;
        doc["hostname"]    = settings.getHostname();
        doc["state"]       = state == CONNECTING ? "connecting"
                           : state == CONNECTED  ? "connected"
                           : state == FAILED     ? "failed"
                                                 : "idle";
        doc["ssid"]        = pendingSsid;
        doc["ip"]          = obtainedIp;
        doc["error"]       = lastError;
        doc["errorDetail"] = lastErrorDetail;

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    /**
     * Async scan, in exactly the shape GET /wifi/scan answers with - the two
     * are drawn by the same Svelte component, and a second shape here would
     * mean a second component to keep in step.
     *
     * Scanning from AP+STA mode makes the access point stutter for a second or
     * two, which is why the page polls rather than blocking on one long call:
     * a phone that loses the portal mid-request shows an error, and it was
     * only ever a scan.
     */
    void sendScan()
    {
        JsonDocument doc;
        int found = WiFi.scanComplete();

        if (found == WIFI_SCAN_RUNNING)
        {
            doc["scanning"] = true;
        }
        else if (found == WIFI_SCAN_FAILED)
        {
            WiFi.scanNetworks(true);
            doc["scanning"] = true;
        }
        else
        {
            doc["scanning"] = false;
            JsonArray networks = doc["networks"].to<JsonArray>();
            for (int i = 0; i < found && i < 20; i++)
            {
                JsonObject net = networks.add<JsonObject>();
                net["ssid"]   = WiFi.SSID(i);
                net["rssi"]   = WiFi.RSSI(i);
                net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            }
            WiFi.scanDelete();
        }

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    /**
     * Takes the credentials and answers straight away.
     *
     * The attempt itself runs from poll(), for the same reason the network
     * switch in WebRoutes does: it takes seconds, and a request that blocks
     * for seconds is a request the phone gives up on - after which the page
     * has no way of learning how it went.
     */
    void startConnect()
    {
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));

        String ssid = doc["ssid"] | "";
        if (ssid.length() == 0)
        {
            server.send(400, "application/json", "{\"error\":\"wifiNoSsid\"}");
            return;
        }
        if (state == CONNECTING)
        {
            server.send(409, "application/json", "{\"error\":\"portalBusy\"}");
            return;
        }

        pendingSsid = ssid;
        pendingPass = doc["password"] | "";
        lastError = "";
        lastErrorDetail = "";
        obtainedIp = "";
        state = CONNECTING;

        // Never the password, here or anywhere else: this line goes into the
        // ring, which the debug tab hands out to anyone who is unlocked.
        debugI("Portal: trying %s", pendingSsid.c_str());
        sendStatus();
    }

    /** Steps the attempt along, and restarts once one has worked. */
    void driveConnect()
    {
        if (state == CONNECTED)
        {
            if (restartAt && (long)(millis() - restartAt) >= 0)
            {
                persistCredentials();
                debugA("Portal: restarting into normal operation");
                ESP.restart();
            }
            return;
        }

        if (state != CONNECTING) return;

        if (connectDeadline == 0)
        {
            // AP_STA rather than STA: the phone is on our access point and has
            // to stay there long enough to be told how it went. Switching to
            // plain STA here drops the connection carrying the answer.
            WiFi.mode(WIFI_AP_STA);
            WiFi.begin(pendingSsid.c_str(), pendingPass.c_str());
            connectDeadline = millis() + PORTAL_CONNECT_TIMEOUT;
            return;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            obtainedIp = WiFi.localIP().toString();
            state = CONNECTED;
            connectDeadline = 0;
            // Long enough for the page to poll once more and show the address
            // it ended up with - which is the one thing somebody needs before
            // the access point disappears from under them.
            restartAt = millis() + 4000;
            debugA("Portal: connected to %s as %s", pendingSsid.c_str(), obtainedIp.c_str());
            return;
        }

        if ((long)(millis() - connectDeadline) >= 0)
        {
            state = FAILED;
            connectDeadline = 0;
            lastError = "wifiConnect";
            lastErrorDetail = pendingSsid;
            // Back to the access point alone: left in AP_STA the driver would
            // keep retrying the wrong password in the background for as long
            // as the portal is up, and every retry costs the access point the
            // phone is sitting on. Nothing to erase - the attempt was never
            // written to flash; see persistCredentials().
            WiFi.disconnect();
            WiFi.mode(WIFI_AP);
            debugW("Portal: could not join %s", pendingSsid.c_str());
        }
    }
}

void Portal::begin(bool hadCredentials)
{
    retryStored = hadCredentials;
    startedAt = millis();

    apName = settings.getHostname();

    // Nothing a visitor types is written to flash until it has worked. The
    // driver stores credentials in its own NVS namespace on every
    // WiFi.begin(ssid, pass), which is right in the WLAN tab - the clock is
    // on a network there and a failed switch falls back - and wrong here: this
    // runs on a clock that could not reach its network, and one typo would
    // replace the credentials it would have retried. RAM only until
    // persistCredentials() says otherwise.
    WiFi.persistent(false);

    // Open on purpose. A WPA2 access point needs a password, and the only
    // places to put one are the label on the housing or the manual - which is
    // a shared secret published with the product. What is reachable here is
    // the portal and nothing else (see the header), and it is up for as long
    // as it takes to type a WiFi password.
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str());
    apIp = WiFi.softAPIP();

    // Every name, our address. NoError rather than a refusal, so a client that
    // asks for something we do not serve still comes to us and gets the
    // redirect instead of a resolver failure.
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", apIp);

    server.on("/portal/status", HTTP_GET, sendStatus);
    server.on("/portal/scan", HTTP_GET, sendScan);
    server.on("/portal/connect", HTTP_POST, startConnect);

    // The portal is a page of its own in the same build, so "/" has to be
    // pointed at it explicitly - index.html is the configuration SPA, and that
    // one has nothing to say to a clock with no network.
    server.on("/", HTTP_GET, []()
    {
        if (!Web::serveFromFilesystem("/portal.html"))
        {
            // No filesystem image flashed yet. Worth saying plainly rather
            // than answering 404: the clock is working, the images are half
            // installed, and `pio run -t uploadfs` is the fix.
            server.send(200, "text/html",
                        "<!doctype html><meta charset=utf-8>"
                        "<title>QlockThreeW32</title>"
                        "<p>No web interface in flash. Upload the filesystem "
                        "image: <code>pio run -t uploadfs</code>");
        }
    });

    server.onNotFound([]()
    {
        // A phone probing for internet access, or anything else that arrived
        // by a name rather than by our address: redirect, which is what puts
        // the sign-in sheet on screen.
        if (!addressedToUs()) { redirectToPortal(); return; }

        // Our own address: the page's assets live in the filesystem.
        if (Web::serveFromFilesystem(server.uri())) return;

        // Addressed to us but nothing there - a stale bookmark of the SPA, or
        // /favicon.ico. Send them to the portal rather than to a 404: while
        // this mode is on there is exactly one page worth being on.
        redirectToPortal();
    });

    server.begin();
    started = true;

    debugA("Setup portal up: SSID \"%s\", http://%s/", apName.c_str(), apIp.toString().c_str());
}

bool Portal::active()
{
    return started;
}

void Portal::poll()
{
    if (!started) return;

    dns.processNextRequest();
    driveConnect();

    // Only when there was something to go back to. With no stored network the
    // portal is the clock's only state, and restarting out of it would put it
    // straight back here having lost whatever was half typed.
    if (retryStored && state == IDLE && (millis() - startedAt) > PORTAL_RETRY_AFTER)
    {
        debugA("Portal: nothing entered, restarting to retry the stored network");
        ESP.restart();
    }
}
