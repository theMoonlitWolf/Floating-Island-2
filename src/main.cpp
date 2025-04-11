/*
 * Floating Island LED Controller
 * A RGB LED lamp
 * 
 * TODO:
 * - TinyReciever.h?, IRCommandDispatcher.h?, other cleaner uproach?
 * - Functions from FastLED
 *    - noise.h?, color.h?, colorpalettes.h?
 * - Mode timeout - switch to mode 0
*/

#include <Arduino.h>
#include <Config.h>
#include <debugPrints.h>
#if USE_WiFi == 1
#include <WifiHandler.h>
#endif
#include <FastLED.h>
#include <IRremote.hpp>

// --- Define variables, classes and functions ---

#pragma region VariablesClassInstances
CRGB leds[NUM_LEDS];

// Working variables
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

#pragma endregion

#pragma region Functions
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
  #if defined(ESP8266)
  ESP.restart(); // Restart ESP8266
  #elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  asm volatile ("jmp 0"); // Reset (jump to bootloader)
  #endif
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
void wipeEEPROMAction() {
  EEPROM.put(0, byte(0));
  #if defined(ESP8266)
  EEPROM.commit(); // Commit changes to EEPROM
  #endif
  reboot();
}

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
  {2, 23, saveEEPROMAction}, // SAVE_EEPROM
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

#pragma endregion
// ---

#pragma region Setup
void setup() {
  #if defined(DEBUG) && defined(__AVR_ATmega328P__)
  debug_init(); // Needs to be called as first thing in setup
  #endif

  #if not (defined(DEBUG) && defined(__AVR_ATmega328P__))
  Serial.begin(BAUDRATE);
  delay(100);
  #endif

  #if defined(DEBUG) && defined(ESP8266)
  gdbstub_init();
  #endif

  debugPrintln(F("START " __FILE__ " from " __DATE__));
  debugPrintln(F("Setting up..."));

  debugPrintln(F("Setting up FastLED..."));
  FastLED.addLeds<SK6812, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(150);
  fill_solid(leds, NUM_LEDS, CRGB::Black); // Clear all LEDs
  FastLED.show();
  
  status(250);
  statusUpdate(0);

  #if USE_EEPROM == 1 && defined(ESP8266)
  EEPROM.begin(EEPROM_SIZE);
  #endif

  if (EEPROM.read(0) == EEPROM_PROJECT_ID) {
    loadEEPROM();
  } else {
    saveEEPROM();
    fade(100, CurrentLight);
  }

  debugPrintln(F("Setting up IR receiver..."));
  IrReceiver.begin(PIN_IR, DISABLE_LED_FEEDBACK);
  IrReceiver.registerReceiveCompleteCallback(recieveCallbackHandler);

  #if USE_WiFi == 1
  wifi_init();
  #endif

  debugPrintln(F("Setup complete."));
  status(90, 500);
}
#pragma endregion

#pragma region Loop
void loop() {
  fadeUpdate();
  statusUpdate();

  #if USE_WiFi == 1
  serverUpdate(); // Update the web server
  #endif

  if (millis() > 4294960000UL) { //4294967295
    reboot();
  }

  delay(10);
}
#pragma endregion

// --- Functions ---
#pragma region Functions
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
  debug_message("IR received");
  debug_message("Address: ");
  debug_message((char*)IrReceiver.decodedIRData.address);
  debug_message("Command: ");
  debug_message((char*)IrReceiver.decodedIRData.command);
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
  Serial.println(F("Saving to EEPROM..."));
  EEPROM.put(0, byte(EEPROM_PROJECT_ID));
  EEPROM.put(1, On);
  EEPROM.put(2, Layout);
  EEPROM.put(3, CurrentLight); // Startup color
  EEPROM.put(3 + sizeof(lightData), Preset0);
  EEPROM.put(3 + sizeof(lightData)*2, Preset1);
  EEPROM.put(3 + sizeof(lightData)*3, Preset2);
  EEPROM.put(3 + sizeof(lightData)*4, Preset3);
  EEPROM.put(3 + sizeof(lightData)*5, Preset4);
  EEPROM.put(3 + sizeof(lightData)*6, Preset5);

  #if defined(ESP8266)
  EEPROM.commit(); // Commit changes to EEPROM
  #endif
}

void loadEEPROM() {
  Serial.println(F("Loading from EEPROM..."));
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
}
#pragma endregion
// ---
