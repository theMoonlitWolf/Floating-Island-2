/*
 * Floating Island LED Controller
 * A RGB LED lamp
 * 
 * TODO:
 * - git
 * - Clean up IR control
 * - TinyReciever.h?, IRCommandDispatcher.h?, other cleaner uproach?
 * - Implement EEPROM saving and loading
 * - Functions from FastLED
 *    - noise.h?, color.h?, colorpalettes.h?
 */


#include <Arduino.h>
//#include <EEPROM.h>

#include <FastLED.h>
#include <IRremote.h>

// --- Pin Config ---
                      // strip - Color - Uno
  #define PIN_LED   3   //  Data - Green - Pin 3
                      //  GND  - White - GND
                      //  VCC  - Red   - 5V

                      //  IR   - Color - Uno
  #define PIN_IR    2   //  Data - Red   - Pin 2
                      //  GND  - White - GND
                      //  VCC  - Black - 5V

  #define PIN_BUTTON  4
// ---

// --- CONFIG ---
  #define BAUDRATE  115200  // Baudrate for Hardware Serial
  #define NUM_LEDS  14      // Number of LEDs in the strip
  #define IR_CODE_FORGET_TIME_ms 1000L
  #define MINIMUM_FADE_TIME_ms 50
  #define STATUS_FADE_TIME_ms 150  
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

// classes
CRGB leds[NUM_LEDS];

// working variables
bool On = true;
byte mode = 0;
mainLayout Layout = DIAGONAL;

unsigned long FadeStartTime = 0;
long FadeTime = 0;
bool Fading = false;

long StatusFadeStartTime = 0;
unsigned long StatusEndTime = 0;
bool StatusFading = false;

// Light variables
lightData CurrentLight = {0, CHSV(80,255,255), CHSV(120,255,255), CHSV(20,255,255)};
lightData TargetLight = {0, CHSV(80,255,255), CHSV(120,255,255), CHSV(20,255,255)};
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


//TEMPORARY
void receive();
void handleIRResults(byte button);


void setup() {
  Serial.begin(BAUDRATE);
  Serial.println(F("START " __FILE__ " from " __DATE__));
  Serial.println(F("Setting up..."));

  FastLED.setBrightness(150);
  status(250);
  statusUpdate(0);

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  IrReceiver.begin(PIN_IR, ENABLE_LED_FEEDBACK);
  IrReceiver.registerReceiveCompleteCallback(recieveCallbackHandler);

  FastLED.addLeds<SK6812, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(CurrentLight.Brightness);
  FastLED.show();

  Serial.println(F("Setup done."));

  fade(100, Preset0);
  TargetLight = Preset0;
  status(90, 500);
}

void loop() {
  fadeUpdate();
  statusUpdate();

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
  if (hue == StatusFadeData[HUE].end && sat == StatusFadeData[SAT].end && val == StatusFadeData[VAL].end)
  {return;} // No change required

  if (duration == 0) {
    StatusEndTime = 0; // Infinite
  } else {
    StatusEndTime = millis() + duration; // Set time for status led to turn off
  }

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
  // Serial.println(F("Status CHSV set."));
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

  receive(); // TEMPORARY
  // Shorter???, TinyReciever.h?, IRCommandDispatcher.h?
}

void receive() {
  static unsigned long nextSwitchTime = 0;
  static byte button = 0;

  if (IrReceiver.decodedIRData.address == 0xEF00) {
      button = IrReceiver.decodedIRData.command + 1;
  }

  // Serial.print("Value: ");
  // Serial.print(IrReceiver.decodedIRData.decodedRawData);
  // Serial.print(" (0x");
  // Serial.print(IrReceiver.decodedIRData.decodedRawData, HEX);
  // Serial.print("); Adress: 0x");
  // Serial.print(IrReceiver.decodedIRData.address, HEX);
  // Serial.print(", Command: ");
  // Serial.print(IrReceiver.decodedIRData.command);
  // Serial.print(" (0x");
  // Serial.print(IrReceiver.decodedIRData.command, HEX);
  // Serial.print(") || Button: ");
  // Serial.println(button);

  if (IrReceiver.decodedIRData.address != 0xEF00) {
      // Serial.println(F("Wrong address!"));
      return;
  } else if (button == 4 && IrReceiver.decodedIRData.decodedRawData == 0) {
      Serial.println(F("On button repeat off!"));
      return;
  }

  handleIRResults(button);

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

  Serial.print("Pressed: ");
  Serial.print(button);
  Serial.print(" in mode: ");
  Serial.println(mode);

  nextSwitchTime = millis() + IR_CODE_FORGET_TIME_ms;
    

  if(nextSwitchTime != 0) {
    if(nextSwitchTime < millis()) {
    nextSwitchTime = 0;
    button = 0;
    Serial.println(F("Forgot last pressed button."));
    }
  }

}

void handleIRResults(byte button) {
    if (button == 4) {
      On=!On;
      fade(500, TargetLight);
      return;
    }

    if (!On) {
        return;
    }
    
    switch (button) {
        case 8:
            mode++;
            if(mode > 2) {
                mode = 0;
            }
            break;
        case 12:
            TargetLight.Brightness += 2;
            break;
        case 16:
            TargetLight.Brightness -= 2;
            break;
    }
    switch (mode) {
        case 0:
            switch (button)
            {
            case 1:
                TargetLight.Main1 = Preset0.Main1;
                TargetLight.Main2 = Preset0.Main2;
                TargetLight.Back = Preset0.Back;
                TargetLight.Brightness = Preset0.Brightness;
                break;
            case 2:
                TargetLight.Main1 = Preset1.Main1;
                TargetLight.Main2 = Preset1.Main2;
                TargetLight.Back = Preset1.Back;
                TargetLight.Brightness = Preset1.Brightness;
                break;
            case 3:
                TargetLight.Main1 = Preset2.Main1;
                TargetLight.Main2 = Preset2.Main2;
                TargetLight.Back = Preset2.Back;
                TargetLight.Brightness = Preset2.Brightness;
                break;
            case 5:
                TargetLight.Main1 = Preset3.Main1;
                TargetLight.Main2 = Preset3.Main2;
                TargetLight.Back = Preset3.Back;
                TargetLight.Brightness = Preset3.Brightness;
                break;
            case 6:
                TargetLight.Main1 = Preset4.Main1;
                TargetLight.Main2 = Preset4.Main2;
                TargetLight.Back = Preset4.Back;
                TargetLight.Brightness = Preset4.Brightness;
                break;
            case 7:
                TargetLight.Main1 = Preset5.Main1;
                TargetLight.Main2 = Preset5.Main2;
                TargetLight.Back = Preset5.Back;
                TargetLight.Brightness = Preset5.Brightness;
                break;
            
            default:
                break;
            }
            break;
        case 1:
            switch (button) {
            case 1:
                TargetLight.Main1.h += 2;
                break;
            case 5:
                TargetLight.Main1.h -= 2;
                break;
            case 2:
                TargetLight.Main1.s += 2;
                break;
            case 6:
                TargetLight.Main1.s -= 2;
                break;
            case 3:
                TargetLight.Main1.v += 2;
                break;
            case 7:
                TargetLight.Main1.v -= 2;
                break;

            case 9:
                TargetLight.Main2.h += 2;
                break;
            case 13:
                TargetLight.Main2.h -= 2;
                break;
            case 10:
                TargetLight.Main2.s += 2;
                break;
            case 14:
                TargetLight.Main2.s -= 2;
                break;
            case 11:
                TargetLight.Main2.v += 2;
                break;
            case 15:
                TargetLight.Main2.v -= 2;
                break;

            case 17:
                TargetLight.Back.h += 2;
                break;
            case 21:
                TargetLight.Back.h -= 2;
                break;
            case 18:
                TargetLight.Back.s += 2;
                break;
            case 22:
                TargetLight.Back.s -= 2;
                break;
            case 19:
                TargetLight.Back.v += 2;
                break;
            case 23:
                TargetLight.Back.v -= 2;
                break;

            default:
                break;
            }
            break;

        // case 2:
        //     switch (button) {
        //     case 1:
        //         Preset1.Main1 = Main1;
        //         Preset1.Main2 = Main2;
        //         Preset1.Back = Back;
        //         Preset1.Brightness = brightness;
        //         DefPresetNum = 1;
        //         Serial.println("Current colors saved to slot 1.");
        //         break;
        //     case 2:
        //         Preset2.Main1 = Main1;
        //         Preset2.Main2 = Main2;
        //         Preset2.Back = Back;
        //         Preset2.Brightness = brightness;
        //         DefPresetNum = 2;
        //         Serial.println("Current colors saved to slot 2.");
        //         break;
        //     case 3:
        //         Preset3.Main1 = Main1;
        //         Preset3.Main2 = Main2;
        //         Preset3.Back = Back;
        //         Preset3.Brightness = brightness;
        //         DefPresetNum = 3;
        //         Serial.println("Current colors saved to slot 3.");
        //         break;
        //     case 5:
        //         Preset4.Main1 = Main1;
        //         Preset4.Main2 = Main2;
        //         Preset4.Back = Back;
        //         Preset4.Brightness = brightness;
        //         DefPresetNum = 4;
        //         Serial.println("Current colors saved to slot 4.");
        //         break;
        //     case 6:
        //         Preset5.Main1 = Main1;
        //         Preset5.Main2 = Main2;
        //         Preset5.Back = Back;
        //         Preset5.Brightness = brightness;
        //         DefPresetNum = 5;
        //         Serial.println("Current colors saved to slot 5.");
        //         break;
        //     case 7:
        //         Preset6.Main1 = Main1;
        //         Preset6.Main2 = Main2;
        //         Preset6.Back = Back;
        //         Preset6.Brightness = brightness;
        //         DefPresetNum = 6;
        //         Serial.println("Current colors saved to slot 6.");
        //         break;
        //     case 22:
        //         EEPROM.wipe();
        //         ESP.restart();
        //         break;
        //     case 23:
        //         loadEEPROM();
        //         break;
        //     case 24:
        //         saveEEPROM();
        //         break;
        //     default:
        //         break;
        //     }
        //     break;
    }
    fade(500, TargetLight);
}




