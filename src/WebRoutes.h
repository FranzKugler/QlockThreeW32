/**
 * WebRoutes
 * The clock's HTTP interface. See WebRoutes.cpp for what lives there and why
 * the network switch counts as part of it.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  17.8.2026
 * @updated  17.8.2026
 */
#ifndef WEBROUTES_H
#define WEBROUTES_H

#include "Arduino.h"

namespace Web
{
    /**
     * Hangs every handler on the server. Call from setup() before
     * server.begin(); the /ota routes are registered separately by Ota::begin().
     */
    void begin();

    /**
     * Drives the network switch requested through POST /wifi. Call from
     * loop() - the switch takes seconds and must not block a request.
     */
    void poll();

    /**
     * True while a switch is in flight. loop() leaves the connection alone
     * meanwhile, or its reconnect and the switch would fight over it.
     */
    bool switchingNetwork();

    /**
     * Sends one file out of LittleFS by path, or answers false if there is
     * none. Shared with Portal.cpp, which serves the same SPA build out of
     * the same filesystem while the clock has no network yet - so the
     * extension-to-MIME table exists once rather than twice.
     */
    bool serveFromFilesystem(String path);
}

#endif
