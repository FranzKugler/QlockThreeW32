/**
 * Secrets.example
 * Template for src/Secrets.h, which holds the credentials of one particular
 * clock and is therefore not in version control.
 *
 * Copy this file to Secrets.h and fill in your own values. The build works
 * without it - main .cpp checks with __has_include and simply leaves the
 * corresponding feature unprotected - so a fresh clone compiles as it is.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
#ifndef SECRETS_H
#define SECRETS_H

/*
 * Password for flashing over the network with espota, i.e.
 * `pio run -t upload --upload-port <address>`. Leave this undefined and
 * ArduinoOTA accepts anyone on the network without asking.
 *
 * Note that this does not cover the upload through the web UI (POST
 * /ota/upload), which is deliberately unauthenticated - see CLAUDE.md.
 */
#define OTA_PASSWORD "change-me"

#endif
