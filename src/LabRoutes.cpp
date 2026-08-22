/**
 * LabRoutes
 * The /lab endpoints: mode, leds, sensor, sweep.
 *
 * See LabRoutes.h for why this exists and what shape it takes from that. What
 * is worth knowing here is the addressing, which is the one thing in this file
 * that could be quietly wrong.
 *
 * A pixel can be named two ways, and both are offered on purpose:
 *
 *     {"i": 0..113}        where it sits on the strip
 *     {"cell": [row, col]} where it sits on the face, row 0 at the top
 *
 * The second goes through LedDriverWS2812FastLED::physicalFor(), which is the
 * only place the wiring is written down. Having both is not convenience: it is
 * how the mapping gets checked, because lighting cell (9,10) and lighting
 * pixel 0 must be the same lamp, and this project has had that wrong before.
 *
 * The strip, as the owner describes it and as the driver has always driven it:
 * index 0 is the bottom right letter, the run meanders left and up, index 109
 * is the top right letter, and 110..113 are the corners in the order bottom
 * right, top right, top left, bottom left.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "LabRoutes.h"
#include "Expert.h"
#include "LightSensor.h"
#include "LedDriverWS2812FastLED.h"
#include "DisplayModes.h"
#include "LogBuffer.h"

extern WebServer server;
extern LedDriverWS2812FastLED ledDriver;
extern AmbientLight ambientLight;
extern byte mode;
extern bool needsUpdateFromRtc;

#define LAB_PIXELS 114

namespace
{
    bool labOn = false;

    // What the mode was before the lab took over, so leaving puts the face
    // back where it was rather than on a default nobody chose.
    byte modeBefore = STD_MODE_NORMAL;

    void sendError(int code, const char *what)
    {
        String out = "{\"error\":\"";
        out += what;
        out += "\"}";
        server.send(code, "application/json", out);
    }

    /** Reads the body, or answers and returns false. */
    bool body(JsonDocument &doc)
    {
        if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) return true;
        sendError(400, "labBody");
        return false;
    }

    /**
     * One entry of a `set` list onto the strip.
     *
     * Colour defaults to white at full scale, which is what a coupling
     * measurement wants: the brightest, most repeatable thing the lamp can do.
     */
    bool applyOne(JsonObjectConst item)
    {
        int index = -1;

        if (item["i"].is<int>())
        {
            index = item["i"].as<int>();
        }
        else if (item["cell"].is<JsonArrayConst>())
        {
            JsonArrayConst cell = item["cell"];
            if (cell.size() != 2) return false;
            byte physical = LedDriverWS2812FastLED::physicalFor(cell[0].as<int>(),
                                                                cell[1].as<int>());
            if (physical == 255) return false;
            index = physical;
        }
        else return false;

        if (index < 0 || index >= LAB_PIXELS) return false;

        byte r = 255, g = 255, b = 255;
        if (item["rgb"].is<JsonArrayConst>())
        {
            JsonArrayConst rgb = item["rgb"];
            if (rgb.size() != 3) return false;
            r = (byte)rgb[0].as<int>();
            g = (byte)rgb[1].as<int>();
            b = (byte)rgb[2].as<int>();
        }

        ledDriver.setPixelRaw((byte)index, CRGB(r, g, b));
        return true;
    }

    /** Applies `clear` and `set` from a request or a sweep frame. */
    bool applyFrame(JsonVariantConst frame, uint16_t &lit)
    {
        if (frame["clear"] | true) ledDriver.clearRaw();

        lit = 0;
        if (frame["set"].is<JsonArrayConst>())
        {
            for (JsonObjectConst item : frame["set"].as<JsonArrayConst>())
            {
                if (!applyOne(item)) return false;
                lit++;
            }
        }
        return true;
    }

    /** One measurement into a JSON object: lux and, where there are any, counts. */
    void measureInto(JsonObject out)
    {
        float lux = ambientLight.readNow();
        out["lux"] = lux;

        LightSensor *sensor = ambientLight.device();
        if (sensor == nullptr) return;

        uint16_t full = 0, ir = 0;
        if (sensor->readChannels(full, ir))
        {
            // The counts, not just the lux. The infrared channel is the whole
            // reason they are here: the LEDs put out almost none, so a channel
            // that does not move with the display is an ambient reading the
            // clock cannot pollute.
            out["ch0"] = full;
            out["ch1"] = ir;
        }
        out["rung"] = sensor->rung();
        out["ms"]   = sensor->integrationMs();
    }
}

// ------ mode ------

/**
 * Takes the strip, or gives it back.
 *
 * Taking it also holds the sampling task off the bus, so a scan is not
 * interleaved with the regulator's own reads and a pinned rung stays pinned.
 * Nothing here is stored: the mode is a global, not a setting, and a restart
 * ends the session whatever a script left behind.
 */
static void setMode()
{
    if (!Expert::guard()) return;

    JsonDocument doc;
    if (!body(doc)) return;

    bool wanted = doc["on"] | false;

    if (wanted && !labOn)
    {
        modeBefore = mode;
        mode = EXT_MODE_LAB;
        labOn = true;
        ambientLight.hold(true);
        ledDriver.clearRaw();
        ledDriver.showRaw();
        debugW("Lab: took the strip, mode %d parked", modeBefore);
    }
    else if (!wanted && labOn)
    {
        labOn = false;
        ambientLight.hold(false);
        // Auto-ranging again, or the regulator would keep whichever rung the
        // last sweep happened to pin.
        LightSensor *sensor = ambientLight.device();
        if (sensor) sensor->pinRung(-1);
        mode = modeBefore;
        needsUpdateFromRtc = true;
        debugW("Lab: gave the strip back, mode %d", mode);
    }

    JsonDocument answer;
    answer["on"]   = labOn;
    answer["mode"] = mode;

    String out;
    serializeJson(answer, out);
    server.send(200, "application/json", out);
}

/** What the lab can do on this clock, and what it is doing. */
static void sendState()
{
    if (!Expert::guard()) return;

    JsonDocument doc;
    doc["on"]      = labOn;
    doc["mode"]    = mode;
    doc["pixels"]  = LAB_PIXELS;
    doc["rows"]    = 10;
    doc["columns"] = 11;
    // The corners are not cells and have no (row, col); they are addressed by
    // their place on the strip, in the order they are wired.
    JsonObject corners = doc["corners"].to<JsonObject>();
    corners["bottomRight"] = 110;
    corners["topRight"]    = 111;
    corners["topLeft"]     = 112;
    corners["bottomLeft"]  = 113;

    doc["maxFrames"] = LAB_MAX_FRAMES;
    doc["drawMw"]    = ledDriver.estimatedDrawMilliwatts();
    doc["maxDrawMw"] = LAB_MAX_DRAW_MW;

    JsonObject sensor = doc["sensor"].to<JsonObject>();
    sensor["name"]    = ambientLight.name();
    sensor["present"] = ambientLight.present();
    LightSensor *device = ambientLight.device();
    if (device)
    {
        uint16_t full = 0, ir = 0;
        sensor["channels"] = device->readChannels(full, ir);
        sensor["rungs"]    = device->rungCount();
        sensor["rung"]     = device->rung();
        sensor["ms"]       = device->integrationMs();
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ------ leds ------

static void setLeds()
{
    if (!Expert::guard()) return;
    if (!labOn) { sendError(409, "labNotActive"); return; }

    JsonDocument doc;
    if (!body(doc)) return;

    uint16_t lit = 0;
    if (!applyFrame(doc.as<JsonVariantConst>(), lit)) { sendError(400, "labPixel"); return; }

    // Checked before it is shown, so an over-budget frame never reaches the
    // strip at all. See LAB_MAX_DRAW_MW: refusing is the point.
    uint32_t draw = ledDriver.estimatedDrawMilliwatts();
    if (draw > LAB_MAX_DRAW_MW)
    {
        ledDriver.clearRaw();
        ledDriver.showRaw();
        String out = "{\"error\":\"labTooBright\",\"errorDetail\":\"";
        out += draw;
        out += " mW\"}";
        server.send(413, "application/json", out);
        return;
    }

    if (doc["show"] | true) ledDriver.showRaw();

    JsonDocument answer;
    answer["lit"]    = lit;
    answer["drawMw"] = draw;

    String out;
    serializeJson(answer, out);
    server.send(200, "application/json", out);
}

/** Every pixel as it stands, so a client can check what it actually set. */
static void sendLeds()
{
    if (!Expert::guard()) return;

    JsonDocument doc;
    JsonArray list = doc["leds"].to<JsonArray>();
    for (byte i = 0; i < LAB_PIXELS; i++)
    {
        CRGB colour = ledDriver.getPixelRaw(i);
        JsonArray one = list.add<JsonArray>();
        one.add(colour.r);
        one.add(colour.g);
        one.add(colour.b);
    }
    doc["drawMw"] = ledDriver.estimatedDrawMilliwatts();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ------ sensor ------

static void sendSensor()
{
    if (!Expert::guard()) return;
    if (!ambientLight.present()) { sendError(404, "labNoSensor"); return; }

    LightSensor *sensor = ambientLight.device();
    if (server.hasArg("rung") && sensor)
    {
        if (!sensor->pinRung((int8_t)server.arg("rung").toInt()))
        {
            sendError(400, "labRung");
            return;
        }
    }

    JsonDocument doc;
    JsonObject reading = doc.to<JsonObject>();
    measureInto(reading);
    reading["sensor"] = ambientLight.name();
    // The regulator's own view, for comparison: it is the same chip, smoothed
    // over thirty seconds, and the difference between the two is exactly what
    // makes a scan need its own path.
    reading["smoothed"] = ambientLight.lux();
    reading["held"]     = ambientLight.isHeld();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ------ sweep ------

/**
 * A whole measurement series in one request.
 *
 * Each frame is set, given `settleMs` to be seen, and measured; with
 * `dark: true` an all-off reading is taken first and reported beside it, which
 * is what makes a slow scan survive the room changing underneath it - the
 * difference is the frame's own contribution and the drift cancels.
 *
 * The clock answers nothing else while this runs. That is stated rather than
 * worked around: the alternative is one HTTP round trip per frame, and then
 * network jitter sits inside the measurement.
 */
static void sweep()
{
    if (!Expert::guard()) return;
    if (!labOn) { sendError(409, "labNotActive"); return; }
    if (!ambientLight.present()) { sendError(404, "labNoSensor"); return; }

    JsonDocument request;
    if (!body(request)) return;

    JsonArrayConst frames = request["frames"];
    if (frames.isNull() || frames.size() == 0) { sendError(400, "labNoFrames"); return; }
    if (frames.size() > LAB_MAX_FRAMES)        { sendError(400, "labTooManyFrames"); return; }

    uint32_t settle = request["settleMs"] | 60;
    if (settle > LAB_MAX_SETTLE_MS) settle = LAB_MAX_SETTLE_MS;
    bool wantDark = request["dark"] | false;

    LightSensor *sensor = ambientLight.device();
    if (request["rung"].is<int>() && sensor)
    {
        if (!sensor->pinRung((int8_t)request["rung"].as<int>()))
        {
            sendError(400, "labRung");
            return;
        }
    }

    JsonDocument answer;
    JsonArray out = answer["frames"].to<JsonArray>();
    uint32_t startedAt = millis();

    for (JsonVariantConst frame : frames)
    {
        JsonObject row = out.add<JsonObject>();

        if (wantDark)
        {
            ledDriver.clearRaw();
            ledDriver.showRaw();
            delay(settle);
            measureInto(row["dark"].to<JsonObject>());
        }

        uint16_t lit = 0;
        if (!applyFrame(frame, lit))
        {
            ledDriver.clearRaw();
            ledDriver.showRaw();
            sendError(400, "labPixel");
            return;
        }
        uint32_t draw = ledDriver.estimatedDrawMilliwatts();
        if (draw > LAB_MAX_DRAW_MW)
        {
            ledDriver.clearRaw();
            ledDriver.showRaw();
            sendError(413, "labTooBright");
            return;
        }

        ledDriver.showRaw();
        delay(settle);

        JsonObject reading = row["lit"].to<JsonObject>();
        measureInto(reading);
        row["n"]      = lit;
        row["drawMw"] = ledDriver.estimatedDrawMilliwatts();

        // A sweep that outstays its welcome is stopped rather than left to run
        // the clock out of a client's timeout with nothing to show for it.
        if (millis() - startedAt > LAB_MAX_SWEEP_MS)
        {
            answer["truncated"] = true;
            break;
        }
    }

    ledDriver.clearRaw();
    ledDriver.showRaw();

    answer["ms"]   = millis() - startedAt;
    answer["rung"] = sensor ? sensor->rung() : 0;

    String result;
    serializeJson(answer, result);
    debugI("Lab: swept %d frames in %lu ms, %d bytes", (int)out.size(),
           (unsigned long)(millis() - startedAt), result.length());
    server.send(200, "application/json", result);
}

// ------ the interface the rest of the program sees ------

bool Lab::active()
{
    return labOn;
}

void Lab::begin()
{
    server.on("/lab/state",  HTTP_GET,  sendState);
    server.on("/lab/mode",   HTTP_POST, setMode);
    server.on("/lab/leds",   HTTP_GET,  sendLeds);
    server.on("/lab/leds",   HTTP_POST, setLeds);
    server.on("/lab/sensor", HTTP_GET,  sendSensor);
    server.on("/lab/sweep",  HTTP_POST, sweep);
}
