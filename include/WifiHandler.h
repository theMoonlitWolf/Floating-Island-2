#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>
#include <Config.h>
#include <debugPrints.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#if USE_mDNS == 1
#include <ESP8266mDNS.h>
#endif

extern ESP8266WebServer Server; // Create a web server on port 80

extern bool On; // Light status
extern lightData CurrentLight; // Current light data
extern void fade(long time, lightData targetLight); // Fade function
extern void fade(lightData targetLight); // Fade function with default time

void wifi_init();
void set_callbacks();

void serverUpdate();

void handle404();

#endif