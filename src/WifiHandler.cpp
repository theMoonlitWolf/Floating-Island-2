#include <WifiHandler.h>
#if USE_WiFi == 1

ESP8266WebServer Server(80); // Create a web server on port 80

void wifi_init() {
  WiFi.mode(WIFI_STA);
  #ifdef STATIC_IP
  debugPrintlnf(64, F("Connecting to: " MY_SSID " with a static IP: %s"), STATIC_IP.toString().c_str());
  WiFi.config(STATIC_IP, GATEWAY, SUBNET);
  #else
  debugPrintlnf(64, F("Connecting to: %s\n"), MY_SSID);
  #endif

  WiFi.begin(MY_SSID, MY_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    debugPrint(F("."));
  }
  debugPrintlnf(64, F("\nConnected. Local IP: %s"), WiFi.localIP().toString());

  #if USE_mDNS == 1
  if (MDNS.begin(mDNS_HOSTNAME)) {
    debugPrintln(F("mDNS responder started: http://" mDNS_HOSTNAME ".local/"));
  } else {
    debugPrintln(F("Error setting up mDNS responder!"));
  }
  #endif

  set_callbacks();
  Server.begin();
  MDNS.addService(SERVICE_NAME, "tcp", 80); // Add HTTP service to mDNS
  debugPrintln(F("HTTP server started."));
}

void set_callbacks() {
  Server.on("/", []() {
    Server.send(200, F("text/plain"), F("Hello from ESP8266! I'm hiding behind the FloatingIsland.\n\nUse:\n/on to turn on\n/off to turn off\n/onoff to toggle on/off\n/status to get status\n/custom?b=0-255&h1=0-255&s1=0-255&v1=0-255&h2=0-255&s2=0-255&v2=0-255&hb=0-255&sb=0-255&vb=0-255 to set custom color"));
  });
  Server.on("/on", []() {
    Server.send(200, F("text/plain"), F("Turning on..."));
    On = true;
    fade(500, CurrentLight);
  });
  Server.on("/off", []() {
    Server.send(200, F("text/plain"), F("Turning off..."));
    On = false;
    fade(500, CurrentLight);
  });
  Server.on("/onoff", []() {
    Server.send(200, F("text/plain"), F("Toggling on/off..."));
    On = !On;
    fade(500, CurrentLight);
  });
  Server.on("/status", []() {
    char buffer[128];
    sprintf_P(buffer, (const char *)F("Status: %s\nBrightness: %d\nMain1: %d,%d,%d\nMain2: %d,%d,%d\nBack: %d,%d,%d\n"), (const char *)(On ? "On" : "Off"), CurrentLight.Brightness, CurrentLight.Main1.h, CurrentLight.Main1.s, CurrentLight.Main1.v, CurrentLight.Main2.h, CurrentLight.Main2.s, CurrentLight.Main2.v, CurrentLight.Back.h, CurrentLight.Back.s, CurrentLight.Back.v);
    Server.send(200, F("text/plain"), buffer);
  });
  Server.on("/custom", []() {
    if (Server.hasArg("b") && Server.hasArg("h1") && Server.hasArg("s1") && Server.hasArg("v1") && Server.hasArg("h2") && Server.hasArg("s2") && Server.hasArg("v2") && Server.hasArg("hb") && Server.hasArg("sb") && Server.hasArg("vb")) {
      CurrentLight.Brightness = Server.arg("b").toInt();
      CurrentLight.Main1.h = Server.arg("h1").toInt();
      CurrentLight.Main1.s = Server.arg("s1").toInt();
      CurrentLight.Main1.v = Server.arg("v1").toInt();
      CurrentLight.Main2.h = Server.arg("h2").toInt();
      CurrentLight.Main2.s = Server.arg("s2").toInt();
      CurrentLight.Main2.v = Server.arg("v2").toInt();
      CurrentLight.Back.h = Server.arg("hb").toInt();
      CurrentLight.Back.s = Server.arg("sb").toInt();
      CurrentLight.Back.v = Server.arg("vb").toInt();
      On = true; // Turn on the light
      fade(CurrentLight);
      Server.send(200, F("text/plain"), F("Setting custom color..."));
    } else {
      Server.send(400, F("text/plain"), F("Bad Request: Missing parameters."));
    }
  });
  Server.onNotFound(handle404);
}

void serverUpdate() {
  #if USE_mDNS == 1
  MDNS.update(); // Update mDNS
  #endif
  Server.handleClient(); // Handle incoming client requests
}

void handle404() {
  char buffer[128];
    sprintf_P(buffer, (const char *)F("404 Not Found\n\nURI: %s\nMethod: %s\nArguments: %d\n"), (const char *)Server.uri().c_str(), (const char *)(Server.method()==HTTP_GET?"GET":"POST"), Server.args());
    for (byte i = 0; i < Server.args(); i++) {
        sprintf_P(buffer+strlen(buffer), (const char *)F("  %s: %s\n"), Server.argName(i), Server.arg(i));
    }
    Server.send(404, F("text/plain"), buffer);
    debugPrintln(F("Server client on 404"));
    debugPrintln(buffer);
}

#endif