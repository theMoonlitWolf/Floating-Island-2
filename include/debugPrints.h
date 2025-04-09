#ifndef DEBUG_PRINTS_H
#define DEBUG_PRINTS_H

#include <Arduino.h>
#include <Config.h>

#if defined(DEBUG) && defined(__AVR_ATmega328P__)
#define debugPrint(message) debug_message((const char*)message)
#define debugPrintln(message) debug_message((const char*)message)
#else
#define debugPrint(message) Serial.print(message)
#define debugPrintln(message) Serial.println(message)
#endif

void debugPrintlnf(int len, const char* format, ...);

void debugPrintlnf(int len, const __FlashStringHelper* format, ...);

#endif