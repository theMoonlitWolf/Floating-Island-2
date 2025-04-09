#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


// --- Pin Config ---
#if defined(ESP8266)
#define PIN_LED   3
#define PIN_IR    0
#else
#define PIN_LED   4
#define PIN_IR    3
#endif

// --- CONFIG ---
#if defined(BOOT_DEBUG) && defined(CHIP_ESP8266)
#define BAUDRATE          74880  // Baudrate for Hardware Serial (to see bootloader info & exceptions for esp8266 boards)
#else
#define BAUDRATE         115200  // Baudrate for Hardware Serial
#endif
#define USE_EEPROM            1  // Use EEPROM for settings (1/0 = yes/no)
#define EEPROM_SIZE         128  // Size of EEPROM (in bytes) (only for needed ESP8266, ignored for board with real EEPROM)
#define EEPROM_PROJECT_ID  0x01

#define NUM_LEDS                  14  // Number of LEDs in the strip
#define IR_CODE_FORGET_TIME_ms 1000L
#define IR_REPEAT_IGNORE_TIME_ms 100
#define MINIMUM_FADE_TIME_ms      50
#define STATUS_FADE_TIME_ms      150 
#define IR_REMOTE_STEP            10

// --- Board specific ---
#pragma region BoardSpecificDefines
#if not defined(ESP8266) && not defined(__AVR_ATmega328P__) && not defined(__AVR_ATmega32U4__)
#error "Unsupported platform!\n Please use ESP8266 or ATmega328P (Arduino Uno) or ATmega32U4 (Arduino Pro Micro)"
#endif

#ifdef DEBUG
  #ifdef ESP8266
    #include <GDBStub.h>
  #endif
  #ifdef __AVR_ATmega328P__
    #include "avr8-stub.h"
    // #include "app_api.h" // only needed for breakponts in flash
  #endif
#endif

#if USE_EEPROM == 1
#if defined(ESP8266)
#include <ESP_EEPROM.h>
#else
#include <EEPROM.h>
#endif
#endif

#pragma endregion
// ---

// --- Custom debug print functions ---
#pragma region DebugPrintDefines
#if defined(DEBUG) && defined(__AVR_ATmega328P__)
#define debugPrint(message) debug_message((const char*)message)
#define debugPrintln(message) debug_message((const char*)message)
#else
#define debugPrint(message) Serial.print(message)
#define debugPrintln(message) Serial.println(message)
#endif

void debugPrintlnf(int len, const char* format, ...) {
  #ifdef ESP32
  Serial.printf(format, ...);
  #else
  char buffer[len];

  va_list args;
  va_start(args, format);
  vsnprintf(buffer, len, format, args);
  va_end(args);

  #if defined(DEBUG) && defined(__AVR_ATmega328P__)
  debug_message(buffer);
  #else
  Serial.println(buffer);
  #endif
  #endif
}

void debugPrintlnf(int len, const __FlashStringHelper* format, ...) {
  #ifdef ESP32
  Serial.printf(format, ...);
  #else
  char buffer[len];

  va_list args;
  va_start(args, format);
  vsnprintf(buffer, len, (const char*)format, args);
  va_end(args);

  #if defined(DEBUG) && defined(__AVR_ATmega328P__)
  debug_message(buffer);
  #else
  Serial.println(buffer);
  #endif
  #endif
}
#pragma endregion
// ---

#endif // CONFIG_H