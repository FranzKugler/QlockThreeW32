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
     * The stylesheet injected into WiFiManager's setup portal, which is the
     * only face the clock has before it is on a network. Mirrors the SPA's
     * colours, so change it when those change.
     */
    const char *portalStyle();
}

#endif
