#include <debugPrints.h>

void debugPrintlnf(int len, const char* format, ...) {
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
  }
  
  void debugPrintlnf(int len, const __FlashStringHelper* format, ...) {
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
  }