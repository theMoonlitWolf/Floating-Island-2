#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FastLED.h>


// --- Pin Config ---
#if defined(ESP8266)
#define PIN_LED   3
#define PIN_IR    0
#else
#define PIN_LED   4
#define PIN_IR    3
#endif

// --- CONFIG ---
#if defined(BOOT_DEBUG) && defined(ESP8266)
#define BAUDRATE          74880  // Baudrate for Hardware Serial (to see bootloader info & exceptions for esp8266 boards)
#else
#define BAUDRATE         115200  // Baudrate for Hardware Serial
#endif

#define USE_EEPROM            1  // Use EEPROM for settings (1/0 = yes/no)
#ifdef USE_EEPROM
#ifdef ESP8266
#define EEPROM_SIZE         128  // Size of EEPROM (in bytes) (only for needed ESP8266, ignored for board with real EEPROM)
#endif
#define EEPROM_PROJECT_ID  0x01
#endif

#define USE_WiFi             1  // Use WiFi in station mode (1/0 = yes/no) (only for ESP8266)
#define USE_mDNS             1  // Use mDNS (1/0 = yes/no) (only for ESP8266)
#if USE_WiFi == 1
#ifdef ESP8266
#define MY_SSID        "O2-Internet-826" // SSID of the wifi network
#define MY_PASS        "bA6RfeFT"        // Password of the wifi network
#define STATIC_IP   IPAddress(10,0,0,58)
#ifdef STATIC_IP
#define GATEWAY     IPAddress(192,168,1,1)
#define SUBNET      IPAddress(255,255,255,0)
#endif
#if USE_mDNS == 1
#define mDNS_HOSTNAME    "FloatingIsland"
#define SERVICE_NAME     "FloatingIsland"
#endif
#else
#error "WiFi not supported on this board!\n Please use ESP8266"
#endif
#endif

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

// --- Class, Enum and Struct definitions ---
#pragma region ClassEnumStructs
enum ledNum{
  MAIN1,
  MAIN2,
  MAIN3,
  MAIN4,
  BG1,
  BG2,
  BG3,
  BG4,
  BG5,
  BG6,
  BG7,
  BG8,
  BG9,
  STATUS1
};
 
enum mainLayout{
  DIAGONAL,
  SIDE_BY_SIDE,
  FRONT_TO_BACK
};
 
enum fadeDataItem{
  BRIGHTNESS,
  MAIN1_H,
  MAIN1_S,
  MAIN1_V,
  MAIN2_H,
  MAIN2_S,
  MAIN2_V,
  BACK_H,
  BACK_S,
  BACK_V
};
 
enum HSVItem{
  HUE,
  SAT,
  VAL
};
  
struct lightData {
  byte Brightness;
  CHSV Main1;
  CHSV Main2;
  CHSV Back;
};
 
struct fadeData {
  byte start;
  byte end;
  byte diff;
  int dir;
};

struct IRAction {
  byte mode; // Mode number (0-2)
  byte IrCommand; // IR command (0-23)
  void (*action)(); // Action function callback
  bool repeatable = true; // If the action can be repeated
  IRAction(byte m, byte cmd, void (*act)(), bool rep = true)
    : mode(m), IrCommand(cmd), action(act), repeatable(rep) {}
};

#pragma endregion

#endif // CONFIG_H