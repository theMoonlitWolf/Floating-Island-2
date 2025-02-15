/*
 * Floating Island LED Controller
 * A RGB LED lamp
 * 
 * TODO:
 * - debug messages
 * - Leds not working (all white) when debug - wrong timing? ("runs 100 times slower when using ram breakpoints") - use falsh breakpoints - new bootloader!!!
 * - TinyReciever.h?, IRCommandDispatcher.h?, other cleaner uproach?
 * - Functions from FastLED
 *    - noise.h?, color.h?, colorpalettes.h?
 */


#include <Arduino.h>
#include <EEPROM.h>

#include <FastLED.h>
#include <IRremote.hpp>

#ifdef DEBUG_ESP8266
#include <GDBStub.h>
#endif

#ifdef DEBUG_ATMEGA328P
#include "avr8-stub.h"
// #include "app_api.h" // only needed for breakponts in flash
#endif

// --- Pin Config ---
  #ifdef BOARD_UNO
  #define PIN_LED   4 
  #define PIN_IR    3
  // pin 2 reserved for debugger interrupt (interrupts are 2 and 3)
  #endif
  #ifdef BOARD_ESP8266
  #define PIN_LED   3
  #define PIN_IR    0
  #endif
// ---

// --- CONFIG ---
  #define BAUDRATE  115200  // Baudrate for Hardware Serial
  #define NUM_LEDS  14      // Number of LEDs in the strip
  #define IR_CODE_FORGET_TIME_ms 1000L
  #define IR_REPEAT_IGNORE_TIME_ms 100
  #define MINIMUM_FADE_TIME_ms 50
  #define STATUS_FADE_TIME_ms 150 
  #define IR_REMOTE_STEP 10
  #define EEPROM_PROJECT_ID 0x01
// ---

// --- Define variables, classes and functions ---
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

enum IRFunctionItem{
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

// classes
CRGB leds[NUM_LEDS];

// working variables
bool On = true;
byte mode = 0;
mainLayout Layout = DIAGONAL;

unsigned long LastIRCodeTime = 0;

unsigned long FadeStartTime = 0;
long FadeTime = 0;
bool Fading = false;

long StatusFadeStartTime = 0;
unsigned long StatusEndTime = 0;
bool StatusFading = false;

#ifdef DEBUG_ATMEGA328P
unsigned long DebugTimer = 0;
#endif

// Light variables
lightData CurrentLight = {127, CHSV(80,255,255), CHSV(120,255,255), CHSV(20,255,255)};
fadeData FadeData[10];
fadeData StatusFadeData[3];

lightData Preset0 = {127, CHSV(80,255,255), CHSV(120,255,255), CHSV(20,255,255)};
lightData Preset1 = {5, CHSV(180,255,255), CHSV(200,255,255), CHSV(20,255,255)};
lightData Preset2 = {127, CHSV(110,255,255), CHSV(130,255,255), CHSV(170,255,255)};
lightData Preset3 = {127, CHSV(80,255,255), CHSV(90,255,255), CHSV(50,255,255)};
lightData Preset4 = {127, CHSV(200,255,255), CHSV(230,255,255), CHSV(170,255,255)};
lightData Preset5 = {127, CHSV(100,255,255), CHSV(110,255,255), CHSV(50,255,255)};

// functions

// Fade to specified target light data over specified time. In light data use CHSV struct values.
void fade(long time, lightData targetLight);

// Fade to specified target light data over a short time. In HSV values use 0-255 for data.
// Same as fade(50, targetLight);
void fade(lightData targetLight);

// Call every loop to update fading. Returns progress in percent.
byte fadeUpdate();

// Set status light to specified color over specified time.
// In HSV values use 0-255 for data or -1 (or anything else) for no change.
// Duration is in milliseconds, how long will the status light be on. 0 for infinite.
void status(int hue, uint16_t duration = 0, int val = 150, int sat = 255);

// Call every loop to update status fading. You can specify to skip the fade and just set the color instantly.
void statusUpdate(bool skipFade = false);

// Callback for every received IR signal. Handles the IR signals.
void recieveCallbackHandler();

// EEPROM functions
void saveEEPROM();
void loadEEPROM();

void reboot() {
  asm volatile ("jmp 0"); // Reset (jump to bootloader)
}

// IR signal action callbacks
void onOffAction() {On=!On;}
void modeAction() {mode = (mode + 1) % 3;}
void brightnessUpAction() {if (CurrentLight.Brightness < 255) {CurrentLight.Brightness+=IR_REMOTE_STEP;}}
void brightnessDownAction() {if (CurrentLight.Brightness > 0) {CurrentLight.Brightness-=IR_REMOTE_STEP;}}
void usePreset0Action() {fade(500, Preset0);}
void usePreset1Action() {fade(500, Preset1);};
void usePreset2Action() {fade(500, Preset2);};
void usePreset3Action() {fade(500, Preset3);};
void usePreset4Action() {fade(500, Preset4);};
void usePreset5Action() {fade(500, Preset5);};
void main1HueUpAction() {CurrentLight.Main1.h+=IR_REMOTE_STEP;}
void main1HueDownAction() {CurrentLight.Main1.h-=IR_REMOTE_STEP;}
void main1SatUpAction() {if (CurrentLight.Main1.s < 255) {CurrentLight.Main1.s+=IR_REMOTE_STEP;}}
void main1SatDownAction() {if (CurrentLight.Main1.s > 0) {CurrentLight.Main1.s-=IR_REMOTE_STEP;}}
void main1ValUpAction() {if (CurrentLight.Main1.v < 255) {CurrentLight.Main1.v+=IR_REMOTE_STEP;}}
void main1ValDownAction() {if (CurrentLight.Main1.v > 0) {CurrentLight.Main1.v-=IR_REMOTE_STEP;}}
void main2HueUpAction() {CurrentLight.Main2.h+=IR_REMOTE_STEP;}
void main2HueDownAction() {CurrentLight.Main2.h-=IR_REMOTE_STEP;}
void main2SatUpAction() {if (CurrentLight.Main2.s < 255) {CurrentLight.Main2.s+=IR_REMOTE_STEP;}}
void main2SatDownAction() {if (CurrentLight.Main2.s > 0) {CurrentLight.Main2.s-=IR_REMOTE_STEP;}}
void main2ValUpAction() {if (CurrentLight.Main2.v < 255) {CurrentLight.Main2.v+=IR_REMOTE_STEP;}}
void main2ValDownAction() {if (CurrentLight.Main2.v > 0) {CurrentLight.Main2.v-=IR_REMOTE_STEP;}}
void backHueUpAction() {CurrentLight.Back.h+=IR_REMOTE_STEP;}
void backHueDownAction() {CurrentLight.Back.h-=IR_REMOTE_STEP;}
void backSatUpAction() {if (CurrentLight.Back.s < 255) {CurrentLight.Back.s+=IR_REMOTE_STEP;}}
void backSatDownAction() {if (CurrentLight.Back.s > 0) {CurrentLight.Back.s-=IR_REMOTE_STEP;}}
void backValUpAction() {if (CurrentLight.Back.v < 255) {CurrentLight.Back.v+=IR_REMOTE_STEP;}}
void backValDownAction() {if (CurrentLight.Back.v > 0) {CurrentLight.Back.v-=IR_REMOTE_STEP;}}
void setPreset0Action() {Preset0 = CurrentLight;}
void setPreset1Action() {Preset1 = CurrentLight;}
void setPreset2Action() {Preset2 = CurrentLight;}
void setPreset3Action() {Preset3 = CurrentLight;}
void setPreset4Action() {Preset4 = CurrentLight;}
void setPreset5Action() {Preset5 = CurrentLight;}
void loadEEPROMAction() {loadEEPROM();}
void saveEEPROMAction() {saveEEPROM();}
void wipeEEPROMAction() {EEPROM.update(0, 0); reboot();}

#ifndef WOKWI
const IRAction IRActions[] = {
  {0,  3, onOffAction, false}, // ON_OFF
  {1,  3, onOffAction, false}, // ON_OFF
  {2,  3, onOffAction, false}, // ON_OFF
  {0,  7, modeAction, false}, // MODE
  {1,  7, modeAction, false}, // MODE
  {2,  7, modeAction, false}, // MODE
  {0, 11, brightnessUpAction}, // BRIGHTNESS_UP
  {1, 11, brightnessUpAction}, // BRIGHTNESS_UP
  {2, 11, brightnessUpAction}, // BRIGHTNESS_UP
  {0, 15, brightnessDownAction}, // BRIGHTNESS_DOWN
  {1, 15, brightnessDownAction}, // BRIGHTNESS_DOWN
  {2, 15, brightnessDownAction}, // BRIGHTNESS_DOWN
  {0,  0, usePreset0Action}, // USE_PRESET0
  {0,  1, usePreset1Action}, // USE_PRESET1
  {0,  2, usePreset2Action}, // USE_PRESET2
  {0,  4, usePreset3Action}, // USE_PRESET3
  {0,  5, usePreset4Action}, // USE_PRESET4
  {0,  6, usePreset5Action}, // USE_PRESET5
  {1,  0, main1HueUpAction}, // MAIN1_HUE_UP
  {1,  4, main1HueDownAction}, // MAIN1_HUE_DOWN
  {1,  1, main1SatUpAction}, // MAIN1_SAT_UP
  {1,  5, main1SatDownAction}, // MAIN1_SAT_DOWN
  {1,  2, main1ValUpAction}, // MAIN1_VAL_UP
  {1,  6, main1ValDownAction}, // MAIN1_VAL_DOWN
  {1,  8, main2HueUpAction}, // MAIN2_HUE_UP
  {1, 12, main2HueDownAction}, // MAIN2_HUE_DOWN
  {1,  9, main2SatUpAction}, // MAIN2_SAT_UP
  {1, 13, main2SatDownAction}, // MAIN2_SAT_DOWN
  {1, 10, main2ValUpAction}, // MAIN2_VAL_UP
  {1, 14, main2ValDownAction}, // MAIN2_VAL_DOWN
  {1, 16, backHueUpAction}, // BACK_HUE_UP
  {1, 20, backHueDownAction}, // BACK_HUE_DOWN
  {1, 17, backSatUpAction}, // BACK_SAT_UP
  {1, 21, backSatDownAction}, // BACK_SAT_DOWN
  {1, 18, backValUpAction}, // BACK_VAL_UP
  {1, 22, backValDownAction}, // BACK_VAL_DOWN
  {2,  0, setPreset0Action}, // SET_PRESET0
  {2,  1, setPreset1Action}, // SET_PRESET1
  {2,  2, setPreset2Action}, // SET_PRESET2
  {2,  4, setPreset3Action}, // SET_PRESET3
  {2,  5, setPreset4Action}, // SET_PRESET4
  {2,  6, setPreset5Action}, // SET_PRESET5
  {2, 21, wipeEEPROMAction}, // WIPE_EEPROM
  {2, 22, loadEEPROMAction}, // LOAD_EEPROM
  {2, 23, saveEEPROMAction} // SAVE_EEPROM
};
#elif defined(WOKWI)
const IRAction IRActions[] = {
  {0, 162, onOffAction, false}, // ON_OFF
  {1, 162, onOffAction, false}, // ON_OFF
  {2, 162, onOffAction, false}, // ON_OFF
  {0, 226, modeAction, false}, // MODE
  {1, 226, modeAction, false}, // MODE
  {2, 226, modeAction, false}, // MODE
  {0,  34, brightnessUpAction}, // BRIGHTNESS_UP
  {2,  34, brightnessUpAction}, // BRIGHTNESS_UP
  {0, 224, brightnessDownAction}, // BRIGHTNESS_DOWN
  {2, 224, brightnessDownAction}, // BRIGHTNESS_DOWN
  {0,  48, usePreset0Action}, // USE_PRESET0
  {0,  24, usePreset1Action}, // USE_PRESET1
  {0, 122, usePreset2Action}, // USE_PRESET2
  {0,  16, usePreset3Action}, // USE_PRESET3
  {0,  56, usePreset4Action}, // USE_PRESET4
  {0,  90, usePreset5Action}, // USE_PRESET5
  {1,  34, main1HueUpAction}, // MAIN1_HUE_UP
  {1, 224, main1HueDownAction}, // MAIN1_HUE_DOWN
  {1,  2, main1SatUpAction}, // MAIN1_SAT_UP
  {1, 168, main1SatDownAction}, // MAIN1_SAT_DOWN
  {1, 194, main1ValUpAction}, // MAIN1_VAL_UP
  {1, 144, main1ValDownAction}, // MAIN1_VAL_DOWN
  {1, 104, main2HueUpAction}, // MAIN2_HUE_UP
  {1,  48, main2HueDownAction}, // MAIN2_HUE_DOWN
  {1, 152, main2SatUpAction}, // MAIN2_SAT_UP
  {1,  24, main2SatDownAction}, // MAIN2_SAT_DOWN
  {1, 176, main2ValUpAction}, // MAIN2_VAL_UP
  {1, 122, main2ValDownAction}, // MAIN2_VAL_DOWN
  {1,  16, backHueUpAction}, // BACK_HUE_UP
  {1,  66, backHueDownAction}, // BACK_HUE_DOWN
  {1,  56, backSatUpAction}, // BACK_SAT_UP
  {1,  74, backSatDownAction}, // BACK_SAT_DOWN
  {1,  90, backValUpAction}, // BACK_VAL_UP
  {1,  82, backValDownAction}, // BACK_VAL_DOWN
  {2,  48, setPreset0Action}, // SET_PRESET0
  {2,  24, setPreset1Action}, // SET_PRESET1
  {2, 122, setPreset2Action}, // SET_PRESET2
  {2,  16, setPreset3Action}, // SET_PRESET3
  {2,  56, setPreset4Action}, // SET_PRESET4
  {2,  90, setPreset5Action}, // SET_PRESET5
};
#endif


void setup() {
  #ifdef DEBUG_ATMEGA328P
  debug_init(); // Needs to be called as first thing in setup
  delay(1000); // Wait for serial monitor to connect
  DebugTimer = millis();
  debug_message("START " __FILE__ " from " __DATE__ " in debug mode");
  debug_message("Setting up...");
  #endif

  #ifndef DEBUG_ATMEGA328P
  Serial.begin(BAUDRATE);
  #endif
  
  #ifdef DEBUG_ESP8266
  gdbstub_init();
  #endif

  #ifndef DEBUG_ATMEGA328P
  Serial.println(F("START " __FILE__ " from " __DATE__));
  Serial.println(F("Setting up..."));
  #endif


  FastLED.addLeds<SK6812, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(150);
  FastLED.show();

  status(250);
  statusUpdate(0);

  if (EEPROM.read(0) == EEPROM_PROJECT_ID) {
    loadEEPROM();
  } else {
    saveEEPROM();
    fade(100, CurrentLight);
  }

  IrReceiver.begin(PIN_IR, ENABLE_LED_FEEDBACK);
  IrReceiver.registerReceiveCompleteCallback(recieveCallbackHandler);

  #ifndef DEBUG_ATMEGA328P
  Serial.println(F("Setup done."));
  #endif

  #ifdef DEBUG_ATMEGA328P
  debug_message("Setup done.");
  char msg[20];
  sprintf(msg, "Took %lu ms", millis() - DebugTimer);
  debug_message(msg);
  #endif

  status(90, 500);
}

void loop() {
  fadeUpdate();
  statusUpdate();

  if (millis() > 4294960000UL) { //4294967295
    reboot();
  } 

  delay(10);
}

void fade(long time, lightData targetLight) {
  if (time < MINIMUM_FADE_TIME_ms) {time = MINIMUM_FADE_TIME_ms;} // Minimum fade time

  if (!On) {targetLight.Brightness = 0;} // If light is off, set brightness to 0

  if (targetLight.Brightness == FastLED.getBrightness() && targetLight.Main1 == CurrentLight.Main1 && targetLight.Main2 == CurrentLight.Main2 && targetLight.Back == CurrentLight.Back)
  {return;} // No change required

  FadeStartTime = millis();
  FadeTime = time;

  // Fill fade data list
  FadeData[BRIGHTNESS].start = FastLED.getBrightness(); // If off (!On), CurrentLight.Brightness stores original brightness, actual brightness might be different
  FadeData[BRIGHTNESS].end = targetLight.Brightness;
   
  FadeData[MAIN1_H].start = CurrentLight.Main1.h;
  FadeData[MAIN1_H].end = targetLight.Main1.h;

  FadeData[MAIN1_S].start = CurrentLight.Main1.s;
  FadeData[MAIN1_S].end = targetLight.Main1.s;

  FadeData[MAIN1_V].start = CurrentLight.Main1.v;
  FadeData[MAIN1_V].end = targetLight.Main1.v;

  FadeData[MAIN2_H].start = CurrentLight.Main2.h;
  FadeData[MAIN2_H].end = targetLight.Main2.h;

  FadeData[MAIN2_S].start = CurrentLight.Main2.s;
  FadeData[MAIN2_S].end = targetLight.Main2.s;

  FadeData[MAIN2_V].start = CurrentLight.Main2.v;
  FadeData[MAIN2_V].end = targetLight.Main2.v;

  FadeData[BACK_H].start = CurrentLight.Back.h;
  FadeData[BACK_H].end = targetLight.Back.h;

  FadeData[BACK_S].start = CurrentLight.Back.s;
  FadeData[BACK_S].end = targetLight.Back.s;

  FadeData[BACK_V].start = CurrentLight.Back.v;
  FadeData[BACK_V].end = targetLight.Back.v;

  // Calculate differences and directions of fades for each value
  for (byte i = 0; i < 10; i++) {
    FadeData[i].diff = abs(FadeData[i].end - FadeData[i].start);
    if (i == MAIN1_H || i == MAIN2_H || i == BACK_H) {  
      if (FadeData[i].diff > 128) {
        FadeData[i].diff = 256-FadeData[i].diff;
        FadeData[i].dir = (FadeData[i].start < FadeData[i].end) ? -1 : 1;
      } else {
        FadeData[i].dir = (FadeData[i].start < FadeData[i].end) ? 1 : -1;
      }
    } else {
      FadeData[i].dir = (FadeData[i].start < FadeData[i].end) ? 1 : -1;
    }
  }
  
  Fading = true; // Set fading flag
}

void fade(lightData targetLight) {
  fade(50, targetLight);
}

void status(int hue, uint16_t duration, int val, int sat) {
  // Set status duration even if no change in color
  if (duration == 0) {
    StatusEndTime = 0; // Infinite
  } else {
    StatusEndTime = millis() + duration; // Set time for status led to turn off
  }
  
  if (hue == StatusFadeData[HUE].end && sat == StatusFadeData[SAT].end && val == StatusFadeData[VAL].end)
  {return;} // No change required

  StatusFadeStartTime = millis();

  // Fill fade data list. If not in byte range, do not change.
  StatusFadeData[HUE].start = StatusFadeData[HUE].end;
  if (hue >= 0 && hue <= 255) {
    StatusFadeData[HUE].end = hue;
  }

  StatusFadeData[SAT].start = StatusFadeData[SAT].end;
  if (sat >= 0 && sat <= 255) {
    StatusFadeData[SAT].end = sat;
  }

  StatusFadeData[VAL].start = StatusFadeData[VAL].end;
  if (val >= 0 && val <= 255) {
    StatusFadeData[VAL].end = val;
  }

  // Calculate differences and directions of fades for each value
  for (byte i = 0; i < 3; i++) {
    StatusFadeData[i].diff = abs(StatusFadeData[i].end - StatusFadeData[i].start);
    if (i == HUE) {  
      if (StatusFadeData[i].diff > 128) {
        StatusFadeData[i].diff = 256-StatusFadeData[i].diff;
        StatusFadeData[i].dir = (StatusFadeData[i].start < StatusFadeData[i].end) ? -1 : 1;
      } else {
        StatusFadeData[i].dir = (StatusFadeData[i].start < StatusFadeData[i].end) ? 1 : -1;
      }
    } else {
      StatusFadeData[i].dir = (StatusFadeData[i].start < StatusFadeData[i].end) ? 1 : -1;
    }
  }

  StatusFading = true; // Set fading flag
}

byte fadeUpdate() {
  if (!Fading) // If not fading, return 255 (-> -1)
  {return 255;} 

  // Calculate current progress
  byte progress = (millis()-FadeStartTime)*100/FadeTime;
  if (progress > 100)
  {progress = 100;}

  // Calculate current brightness
  FastLED.setBrightness(FadeData[BRIGHTNESS].start + (FadeData[BRIGHTNESS].diff * progress / 100) * FadeData[BRIGHTNESS].dir);
  
  if (On) { // Do not update CurrentLight.Brightness if light is off, it will be used to store original brightness for turning it back on
    CurrentLight.Brightness = FadeData[BRIGHTNESS].start + (FadeData[BRIGHTNESS].diff * progress / 100) * FadeData[BRIGHTNESS].dir;
  }
  
  // Calculate current light values
  CurrentLight.Main1.h = FadeData[MAIN1_H].start + (FadeData[MAIN1_H].diff * progress / 100) * FadeData[MAIN1_H].dir;
  CurrentLight.Main1.s = FadeData[MAIN1_S].start + (FadeData[MAIN1_S].diff * progress / 100) * FadeData[MAIN1_S].dir;
  CurrentLight.Main1.v = FadeData[MAIN1_V].start + (FadeData[MAIN1_V].diff * progress / 100) * FadeData[MAIN1_V].dir;
  CurrentLight.Main2.h = FadeData[MAIN2_H].start + (FadeData[MAIN2_H].diff * progress / 100) * FadeData[MAIN2_H].dir;
  CurrentLight.Main2.s = FadeData[MAIN2_S].start + (FadeData[MAIN2_S].diff * progress / 100) * FadeData[MAIN2_S].dir;
  CurrentLight.Main2.v = FadeData[MAIN2_V].start + (FadeData[MAIN2_V].diff * progress / 100) * FadeData[MAIN2_V].dir;
  CurrentLight.Back.h = FadeData[BACK_H].start + (FadeData[BACK_H].diff * progress / 100) * FadeData[BACK_H].dir;
  CurrentLight.Back.s = FadeData[BACK_S].start + (FadeData[BACK_S].diff * progress / 100) * FadeData[BACK_S].dir;
  CurrentLight.Back.v = FadeData[BACK_V].start + (FadeData[BACK_V].diff * progress / 100) * FadeData[BACK_V].dir;

  // Set current light values to LEDs
  switch (Layout) {
    case DIAGONAL:
      leds[MAIN1] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      leds[MAIN2] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      leds[MAIN3] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      leds[MAIN4] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      break;
    case SIDE_BY_SIDE:
      leds[MAIN1] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      leds[MAIN2] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      leds[MAIN3] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      leds[MAIN4] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      break;
    case FRONT_TO_BACK:
      leds[MAIN1] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      leds[MAIN2] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      leds[MAIN3] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      leds[MAIN4] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      break;
    default:
    leds[MAIN1] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      leds[MAIN2] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      leds[MAIN3] = CHSV(CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v);
      leds[MAIN4] = CHSV(CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v);
      break;
  }
  fill_solid(&leds[BG1], 9, CHSV(CurrentLight.Back.h, CurrentLight.Back.s, CurrentLight.Back.v));

  FastLED.show(); // Update LEDs


  if (progress == 100) {
    Fading = false; // Fading done
  }
  return progress;
}

void statusUpdate(bool skipFade) {
  if (StatusEndTime > 0 && millis() > StatusEndTime) { // If status led timer has expired
    status(-1, -1, 0); // Turn off status led
  }

  if (!StatusFading)
  {return;}

  // Calculate current progress
  byte progress;
  if (skipFade == true) {
    progress = 100;
  } else {
    progress = (millis()-StatusFadeStartTime)*100/STATUS_FADE_TIME_ms;
    if (progress > 100)
    {progress = 100;}
  }

  // Calculate current light values and set them to the LED
  leds[STATUS1] = CHSV(StatusFadeData[HUE].start + (StatusFadeData[HUE].diff * progress / 100) * StatusFadeData[HUE].dir,
                       StatusFadeData[SAT].start + (StatusFadeData[SAT].diff * progress / 100) * StatusFadeData[SAT].dir,
                       StatusFadeData[VAL].start + (StatusFadeData[VAL].diff * progress / 100) * StatusFadeData[VAL].dir);
  
    if (progress == 100) {
    StatusFading = false; // Fading done
  }

  FastLED.show(); // Update LEDs
  }

void recieveCallbackHandler() {
  IrReceiver.decode(); // fill IrReceiver.decodedIRData
  IrReceiver.resume(); // enable receiving the next value

  #ifdef DEBUG_ATMEGA328P
  char msg[50];
  debug_message("IR received");
  sprintf(msg, "Address: 0x%04X", IrReceiver.decodedIRData.address);
  debug_message(msg);
  sprintf(msg, "Command: 0x%02X", IrReceiver.decodedIRData.command);
  debug_message(msg);
  #endif

  if (IrReceiver.decodedIRData.address != 0xEF00) {
    #ifdef DEBUG_ATMEGA328P
    debug_message("Invalid IR address");
    #endif
  
    #ifndef WOKWI
    return;
    #endif
  }
  if (IrReceiver.decodedIRData.flags == IRDATA_FLAGS_IS_REPEAT) {
    if (LastIRCodeTime + IR_REPEAT_IGNORE_TIME_ms > millis() || LastIRCodeTime +IR_CODE_FORGET_TIME_ms < millis()) {
      #ifdef DEBUG_ATMEGA328P
      debug_message("Ignoring repeat");
      #endif
      return;
    }
  }

  LastIRCodeTime = millis();

  // Search for the action in the IRActions list
  for (const auto& action : IRActions) {
    // Check if the mode matches first (faster)
    if (action.mode == mode) {
      // Check if the IR command matches
      if (action.IrCommand == IrReceiver.decodedIRData.command) {
        // Check if the action is repeatable and if the command is a repeat
        if (!action.repeatable && IrReceiver.decodedIRData.flags == IRDATA_FLAGS_IS_REPEAT) {
          return;
        }
        // Execute the action
        action.action();
        break;
      }
    }
  }

  // Set status led according to the current mode
  switch (mode)
  {
  case 0:
    status(50, IR_CODE_FORGET_TIME_ms);
    break;
  case 1:
    status(180, IR_CODE_FORGET_TIME_ms);
    break;
  case 2:
    status(250, IR_CODE_FORGET_TIME_ms);
    break;
  
  default:
    break;
  }
  fade(500, CurrentLight);
}

void saveEEPROM() {
  #ifdef DEBUG_ATMEGA328P
    char msg[100];
    debug_message("Saving to EEPROM...");
    DebugTimer = millis();
  #endif
  EEPROM.update(0, EEPROM_PROJECT_ID);
  EEPROM.put(1, On);
  EEPROM.put(2, Layout);
  EEPROM.put(3, CurrentLight); // Startup color
  EEPROM.put(3 + sizeof(lightData), Preset0);
  EEPROM.put(3 + sizeof(lightData)*2, Preset1);
  EEPROM.put(3 + sizeof(lightData)*3, Preset2);
  EEPROM.put(3 + sizeof(lightData)*4, Preset3);
  EEPROM.put(3 + sizeof(lightData)*5, Preset4);
  EEPROM.put(3 + sizeof(lightData)*6, Preset5);
  #ifdef DEBUG_ATMEGA328P
    debug_message("Saved data:");
    sprintf(msg, "Project ID: 0x%02X, reads: 0x%02X", EEPROM_PROJECT_ID, EEPROM.read(0));
    debug_message(msg);
    sprintf(msg, "On: %s, reads: %s", On ? "true" : "false", EEPROM.read(1) ? "true" : "false");
    debug_message(msg);
    sprintf(msg, "Layout: %d, reads: %d", Layout, EEPROM.read(2));
    debug_message(msg);

    // sprintf(msg, "CurrentLight: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   CurrentLight.Brightness, CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v, CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v, CurrentLight.Back.h, CurrentLight.Back.s, CurrentLight.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset0: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset0.Brightness, Preset0.Main1.h, Preset0.Main1.s, Preset0.Main1.v, Preset0.Main2.h, Preset0.Main2.s, Preset0.Main2.v, Preset0.Back.h, Preset0.Back.s, Preset0.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset1: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset1.Brightness, Preset1.Main1.h, Preset1.Main1.s, Preset1.Main1.v, Preset1.Main2.h, Preset1.Main2.s, Preset1.Main2.v, Preset1.Back.h, Preset1.Back.s, Preset1.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset2: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset2.Brightness, Preset2.Main1.h, Preset2.Main1.s, Preset2.Main1.v, Preset2.Main2.h, Preset2.Main2.s, Preset2.Main2.v, Preset2.Back.h, Preset2.Back.s, Preset2.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset3: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset3.Brightness, Preset3.Main1.h, Preset3.Main1.s, Preset3.Main1.v, Preset3.Main2.h, Preset3.Main2.s, Preset3.Main2.v, Preset3.Back.h, Preset3.Back.s, Preset3.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset4: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset4.Brightness, Preset4.Main1.h, Preset4.Main1.s, Preset4.Main1.v, Preset4.Main2.h, Preset4.Main2.s, Preset4.Main2.v, Preset4.Back.h, Preset4.Back.s, Preset4.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset5: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset5.Brightness, Preset5.Main1.h, Preset5.Main1.s, Preset5.Main1.v, Preset5.Main2.h, Preset5.Main2.s, Preset5.Main2.v, Preset5.Back.h, Preset5.Back.s, Preset5.Back.v);
    // debug_message(msg);

    sprintf(msg, "Took %lu ms", millis() - DebugTimer);
    debug_message(msg);
  #endif
}

void loadEEPROM() {
  #ifdef DEBUG_ATMEGA328P
    char msg[100];
    debug_message("Loading from EEPROM...");
    DebugTimer = millis();
  #endif
  EEPROM.get(1, On);
  EEPROM.get(2, Layout);
  EEPROM.get(3, CurrentLight); // Startup color
  EEPROM.get(3 + sizeof(lightData), Preset0);
  EEPROM.get(3 + sizeof(lightData)*2, Preset1);
  EEPROM.get(3 + sizeof(lightData)*3, Preset2);
  EEPROM.get(3 + sizeof(lightData)*4, Preset3);
  EEPROM.get(3 + sizeof(lightData)*5, Preset4);
  EEPROM.get(3 + sizeof(lightData)*6, Preset5);

  fade(100, CurrentLight);

  #ifdef DEBUG_ATMEGA328P
    debug_message("Loaded data:");
    sprintf(msg, "On: %s", On ? "true" : "false");
    debug_message(msg);
    sprintf(msg, "Layout: %d", Layout);
    debug_message(msg);

    // sprintf(msg, "CurrentLight: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   CurrentLight.Brightness, CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v, CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v, CurrentLight.Back.h, CurrentLight.Back.s, CurrentLight.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset0: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset0.Brightness, Preset0.Main1.h, Preset0.Main1.s, Preset0.Main1.v, Preset0.Main2.h, Preset0.Main2.s, Preset0.Main2.v, Preset0.Back.h, Preset0.Back.s, Preset0.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset1: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset1.Brightness, Preset1.Main1.h, Preset1.Main1.s, Preset1.Main1.v, Preset1.Main2.h, Preset1.Main2.s, Preset1.Main2.v, Preset1.Back.h, Preset1.Back.s, Preset1.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset2: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset2.Brightness, Preset2.Main1.h, Preset2.Main1.s, Preset2.Main1.v, Preset2.Main2.h, Preset2.Main2.s, Preset2.Main2.v, Preset2.Back.h, Preset2.Back.s, Preset2.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset3: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset3.Brightness, Preset3.Main1.h, Preset3.Main1.s, Preset3.Main1.v, Preset3.Main2.h, Preset3.Main2.s, Preset3.Main2.v, Preset3.Back.h, Preset3.Back.s, Preset3.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset4: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset4.Brightness, Preset4.Main1.h, Preset4.Main1.s, Preset4.Main1.v, Preset4.Main2.h, Preset4.Main2.s, Preset4.Main2.v, Preset4.Back.h, Preset4.Back.s, Preset4.Back.v);
    // debug_message(msg);
    // sprintf(msg, "Preset5: Brightness: %d Main1: H: %d S: %d V: %d, Main2: H: %d S: %d V: %d, Back: H: %d S: %d V: %d,",
    //   Preset5.Brightness, Preset5.Main1.h, Preset5.Main1.s, Preset5.Main1.v, Preset5.Main2.h, Preset5.Main2.s, Preset5.Main2.v, Preset5.Back.h, Preset5.Back.s, Preset5.Back.v);
    // debug_message(msg);

    debug_message("Set colors to LEDs");

    sprintf(msg, "Took %lu ms", millis() - DebugTimer);
    debug_message(msg);
  #endif
}
