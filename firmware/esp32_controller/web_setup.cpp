#include "web_setup.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "led_direct_output.h"
#include "led_settings.h"
#include "web_setup_page_gzip.h"

static const char *WEB_SETUP_SSID = "TARDI-LED";
static const char *WEB_SETUP_PASSWORD = "tardigrade";
static const byte WEB_SETUP_DNS_PORT = 53;

static DNSServer webSetupDnsServer;
static WebServer webSetupServer(80);
static bool webSetupActive = false;
static bool webSetupDnsActive = false;

static String webSetupLiveOutputLabel() {
  if (!ledDirectOutputAllowed()) {
    return "SIM ONLY";
  }

  if (!ledDirectOutputLinkOnline()) {
    return "ON / ECLAIR OFFLINE";
  }

  return ledSettingsAmbientIsCompletelyDark() ? "ON / SETTINGS DARK" : "ON";
}

static bool webSetupParseByteValue(const String &text, uint8_t &value) {
  if (text.length() == 0) {
    return false;
  }

  for (uint16_t i = 0; i < text.length(); i++) {
    if (!isDigit(text[i])) {
      return false;
    }
  }

  long parsed = text.toInt();

  if (parsed < 0 || parsed > 255) {
    return false;
  }

  value = static_cast<uint8_t>(parsed);
  return true;
}

static bool webSetupParseBoundedByteValue(const String &text, uint8_t minValue, uint8_t maxValue, uint8_t &value) {
  if (!webSetupParseByteValue(text, value)) {
    return false;
  }

  return value >= minValue && value <= maxValue;
}

static int8_t webSetupApplyByteArg(const char *argName, uint8_t &setting) {
  if (!webSetupServer.hasArg(argName)) {
    return 0;
  }

  uint8_t value = 0;
  if (!webSetupParseByteValue(webSetupServer.arg(argName), value)) {
    webSetupServer.send(400, "text/plain", String("Invalid ") + argName + ". Use 0..255.");
    return -1;
  }

  setting = value;
  return 1;
}

static uint8_t webSetupByteToPercent(uint8_t value) {
  return static_cast<uint8_t>(
    (static_cast<uint16_t>(value) * 100u + 127u) / 255u
  );
}

static uint8_t webSetupPercentToByte(uint8_t percent) {
  return static_cast<uint8_t>(
    (static_cast<uint16_t>(percent) * 255u + 50u) / 100u
  );
}

static void webSetupAppendLookJson(String &json, const LedLookSettings &look) {
  json += "{\"brightnessPercent\":";
  json += String(webSetupByteToPercent(look.brightness));
  json += ",\"saturationPercent\":";
  json += String(webSetupByteToPercent(look.saturation));
  json += ",\"speed\":";
  json += String(look.speedPercent);
  json += ",\"palette\":\"";
  json += ledSettingsPaletteName(look.paletteMode);
  json += "\",\"behavior\":\"";
  json += ledSettingsBehaviorName(look.behaviorMode);
  json += "\"}";
}

static void webSetupAppendLookGroupJson(String &json, LedLookKind lookKind) {
  json += "{\"whole\":";
  webSetupAppendLookJson(json, ledSettingsGlobalLook(lookKind));
  json += ",\"zones\":[";

  for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
    if (zone > 0) {
      json += ",";
    }

    webSetupAppendLookJson(json, ledSettingsZoneLook(lookKind, zone));
  }

  json += "]}";
}

static String webSetupJsonStatus() {
  const LedSettings &settings = ledSettingsGet();
  String json;
  json.reserve(3000);

  json += "{";
  json += "\"setupMode\":";
  json += webSetupActive ? "true" : "false";
  json += ",\"ssid\":\"";
  json += WEB_SETUP_SSID;
  json += "\",\"ip\":\"";
  json += webSetupIpAddress();
  json += "\",\"clients\":";
  json += String(webSetupClientCount());
  json += ",\"liveOutput\":\"";
  json += webSetupLiveOutputLabel();
  json += "\",\"realOutputAllowed\":";
  json += ledDirectOutputAllowed() ? "true" : "false";
  json += ",\"firstShowAttempted\":";
  json += ledDirectOutputFirstShowAttempted() ? "true" : "false";
  json += ",\"eclairLinkOnline\":";
  json += ledDirectOutputLinkOnline() ? "true" : "false";
  json += ",\"savedAmbientDark\":";
  json += ledSettingsAmbientIsCompletelyDark() ? "true" : "false";
  json += ",\"mode\":\"";
  json += ledDirectOutputModeName();
  json += "\",\"ledBackend\":\"Eclair7 UART / FastLED RMT4\"";
  json += ",\"ledPins\":\"Eclair:4,5,6,7,8,9,10\"";
  json += ",\"settings\":{\"brightness\":";
  json += String(settings.masterBrightness);
  json += ",\"brightnessPercent\":";
  json += String(webSetupByteToPercent(settings.masterBrightness));
  json += ",\"saturation\":";
  json += String(settings.saturationScale);
  json += ",\"saturationPercent\":";
  json += String(webSetupByteToPercent(settings.saturationScale));
  json += ",\"ambient\":";
  json += String(settings.ambientLevel);
  json += ",\"ambientPercent\":";
  json += String(webSetupByteToPercent(settings.ambientLevel));
  json += ",\"active\":";
  json += String(settings.activeLevel);
  json += ",\"activePercent\":";
  json += String(webSetupByteToPercent(settings.activeLevel));
  json += ",\"speed\":";
  json += String(settings.speedPercent);
  json += ",\"animationDurationSeconds\":";
  json += String(settings.animationDurationSeconds);
  json += ",\"palette\":\"";
  json += ledSettingsPaletteName(settings.paletteMode);
  json += "\",\"behavior\":\"";
  json += ledSettingsBehaviorName(settings.behaviorMode);
  json += "\"";
  json += ",\"looks\":{\"ambient\":";
  webSetupAppendLookGroupJson(json, LED_LOOK_AMBIENT);
  json += ",\"animation\":";
  webSetupAppendLookGroupJson(json, LED_LOOK_ANIMATION);
  json += "}";
  json += ",\"saved\":";
  json += ledSettingsLoadedFromSaved() ? "true" : "false";
  json += ",\"version\":";
  json += String(ledSettingsVersion());
  json += ",\"zones\":[";

  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    if (i > 0) {
      json += ",";
    }

    json += String(settings.zoneBrightness[i]);
  }

  json += "]}}";
  return json;
}

static void webSetupHandleRoot() {
  webSetupServer.sendHeader("Content-Encoding", "gzip");
  webSetupServer.sendHeader("Cache-Control", "no-store");
  webSetupServer.send_P(
    200,
    "text/html",
    reinterpret_cast<const char *>(WEB_SETUP_PAGE_GZIP),
    WEB_SETUP_PAGE_GZIP_SIZE
  );
}

static void webSetupHandleCaptivePortal() {
  webSetupHandleRoot();
}

static void webSetupHandleStatus() {
  webSetupServer.send(200, "application/json", webSetupJsonStatus());
}

static void webSetupHandleSettings() {
  LedSettings &settings = ledSettingsMutable();
  LedLookSettings *targetLook = nullptr;
  bool handled = false;
  int8_t result = 0;

  if (webSetupServer.hasArg("look") || webSetupServer.hasArg("area")) {
    if (!webSetupServer.hasArg("look") || !webSetupServer.hasArg("area")) {
      webSetupServer.send(400, "text/plain", "Use: look=ambient|animation&area=whole|0..6");
      return;
    }

    LedLookKind lookKind = LED_LOOK_AMBIENT;
    if (!ledSettingsParseLookKindName(webSetupServer.arg("look"), lookKind)) {
      webSetupServer.send(400, "text/plain", "Invalid look. Use ambient or animation.");
      return;
    }

    String area = webSetupServer.arg("area");
    area.trim();
    area.toLowerCase();

    if (area == "whole") {
      targetLook = &ledSettingsMutableGlobalLook(lookKind);
    } else if (
      area.length() == 1
      && area[0] >= '0'
      && area[0] < static_cast<char>('0' + LED_LOGICAL_ZONE_COUNT)
    ) {
      targetLook = &ledSettingsMutableZoneLook(lookKind, static_cast<uint8_t>(area[0] - '0'));
    } else {
      webSetupServer.send(400, "text/plain", "Invalid area. Use whole or 0..6.");
      return;
    }
  }

  if (webSetupServer.hasArg("brightnessPercent")) {
    uint8_t percent = 0;

    if (!webSetupParseBoundedByteValue(webSetupServer.arg("brightnessPercent"), 0, 100, percent)) {
      webSetupServer.send(400, "text/plain", "Invalid brightnessPercent. Use 0..100.");
      return;
    }

    if (targetLook != nullptr) {
      targetLook->brightness = webSetupPercentToByte(percent);
    } else {
      settings.masterBrightness = webSetupPercentToByte(percent);
    }
    handled = true;
  }

  if (webSetupServer.hasArg("saturationPercent")) {
    uint8_t percent = 0;

    if (!webSetupParseBoundedByteValue(webSetupServer.arg("saturationPercent"), 0, 100, percent)) {
      webSetupServer.send(400, "text/plain", "Invalid saturationPercent. Use 0..100.");
      return;
    }

    if (targetLook != nullptr) {
      targetLook->saturation = webSetupPercentToByte(percent);
    } else {
      settings.saturationScale = webSetupPercentToByte(percent);
    }
    handled = true;
  }

  if (webSetupServer.hasArg("ambientPercent")) {
    uint8_t percent = 0;

    if (!webSetupParseBoundedByteValue(webSetupServer.arg("ambientPercent"), 0, 100, percent)) {
      webSetupServer.send(400, "text/plain", "Invalid ambientPercent. Use 0..100.");
      return;
    }

    settings.ambientLevel = webSetupPercentToByte(percent);
    handled = true;
  }

  if (webSetupServer.hasArg("activePercent")) {
    uint8_t percent = 0;

    if (!webSetupParseBoundedByteValue(webSetupServer.arg("activePercent"), 0, 100, percent)) {
      webSetupServer.send(400, "text/plain", "Invalid activePercent. Use 0..100.");
      return;
    }

    settings.activeLevel = webSetupPercentToByte(percent);
    handled = true;
  }

  if (webSetupServer.hasArg("speed")) {
    uint8_t speed = 0;

    if (!webSetupParseBoundedByteValue(webSetupServer.arg("speed"), 0, 200, speed)) {
      webSetupServer.send(400, "text/plain", "Invalid speed. Use 0..200.");
      return;
    }

    if (targetLook != nullptr) {
      targetLook->speedPercent = speed;
    } else {
      settings.speedPercent = speed;
    }
    handled = true;
  }

  if (webSetupServer.hasArg("animationDurationSeconds")) {
    uint8_t seconds = 0;

    if (!webSetupParseBoundedByteValue(webSetupServer.arg("animationDurationSeconds"), 1, 60, seconds)) {
      webSetupServer.send(400, "text/plain", "Invalid animationDurationSeconds. Use 1..60.");
      return;
    }

    settings.animationDurationSeconds = seconds;
    handled = true;
  }

  if (webSetupServer.hasArg("palette")) {
    LedPaletteMode paletteMode = LED_PALETTE_DEFAULT;

    if (!ledSettingsParsePaletteName(webSetupServer.arg("palette"), paletteMode)) {
      webSetupServer.send(400, "text/plain", "Invalid palette.");
      return;
    }

    if (targetLook != nullptr) {
      targetLook->paletteMode = paletteMode;
    } else {
      settings.paletteMode = paletteMode;
    }
    handled = true;
  }

  if (webSetupServer.hasArg("behavior")) {
    LedBehaviorMode behaviorMode = LED_BEHAVIOR_NORMAL;

    if (!ledSettingsParseBehaviorName(webSetupServer.arg("behavior"), behaviorMode)) {
      webSetupServer.send(400, "text/plain", "Invalid behavior.");
      return;
    }

    if (targetLook != nullptr) {
      targetLook->behaviorMode = behaviorMode;
    } else {
      settings.behaviorMode = behaviorMode;
    }
    handled = true;
  }

  result = webSetupApplyByteArg("brightness", settings.masterBrightness);
  if (result < 0) {
    return;
  }
  handled = result > 0 || handled;

  result = webSetupApplyByteArg("saturation", settings.saturationScale);
  if (result < 0) {
    return;
  }
  handled = result > 0 || handled;

  result = webSetupApplyByteArg("ambient", settings.ambientLevel);
  if (result < 0) {
    return;
  }
  handled = result > 0 || handled;

  result = webSetupApplyByteArg("active", settings.activeLevel);
  if (result < 0) {
    return;
  }
  handled = result > 0 || handled;

  if (webSetupServer.hasArg("zone") || webSetupServer.hasArg("value")) {
    if (!webSetupServer.hasArg("zone") || !webSetupServer.hasArg("value")) {
      webSetupServer.send(400, "text/plain", "Use: zone=0..6&value=0..255");
      return;
    }

    String zoneArg = webSetupServer.arg("zone");
    uint8_t value = 0;

    if (
      zoneArg.length() != 1
      || zoneArg[0] < '0'
      || zoneArg[0] >= static_cast<char>('0' + LED_LOGICAL_ZONE_COUNT)
      || !webSetupParseByteValue(webSetupServer.arg("value"), value)
    ) {
      webSetupServer.send(400, "text/plain", "Use: zone=0..6&value=0..255");
      return;
    }

    settings.zoneBrightness[static_cast<uint8_t>(zoneArg[0] - '0')] = value;
    handled = true;
  }

  if (!handled) {
    webSetupServer.send(400, "text/plain", "No setting provided");
    return;
  }

  webSetupServer.send(200, "text/plain", "OK");
}

static void webSetupHandleSave() {
  bool saved = ledSettingsSave();
  webSetupServer.send(
    saved ? 200 : 500,
    "text/plain",
    saved ? "Saved" : "Save failed"
  );
}

static void webSetupHandleResetDefaults() {
  ledSettingsResetToDefaults();
  webSetupServer.send(200, "text/plain", "Defaults loaded in RAM");
}

static void webSetupHandleMode() {
  if (!webSetupServer.hasArg("value")) {
    webSetupServer.send(400, "text/plain", "Use: value=off|animation|solid");
    return;
  }

  String value = webSetupServer.arg("value");
  value.toLowerCase();

  bool ok = false;

  if (value == "off") {
    ok = ledDirectOutputSetMode(LED_OUTPUT_OFF, Serial);
  } else if (value == "animation") {
    ok = ledDirectOutputSetMode(LED_OUTPUT_ANIMATION, Serial);
  } else if (value == "solid") {
    ok = ledDirectOutputSetMode(LED_OUTPUT_VALIDATE_SOLID, Serial);
  } else {
    webSetupServer.send(400, "text/plain", "Use: value=off|animation|solid");
    return;
  }

  webSetupServer.send(ok ? 200 : 409, "text/plain", ok ? "OK" : "Blocked");
}

static void webSetupHandleNotFound() {
  webSetupHandleCaptivePortal();
}

void webSetupBegin(bool enabled, Stream &out) {
  if (!enabled) {
    webSetupActive = false;
    webSetupDnsActive = false;
    webSetupDnsServer.stop();
    WiFi.mode(WIFI_OFF);
    out.println("Tardi web controller: OFF");
    return;
  }

  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP(WEB_SETUP_SSID, WEB_SETUP_PASSWORD);

  if (!apStarted) {
    webSetupActive = false;
    out.println("Tardi web controller failed to start.");
    return;
  }

  webSetupDnsActive = webSetupDnsServer.start(
    WEB_SETUP_DNS_PORT,
    "*",
    WiFi.softAPIP()
  );

  webSetupServer.on("/", HTTP_GET, webSetupHandleRoot);
  webSetupServer.on("/generate_204", HTTP_GET, webSetupHandleCaptivePortal);
  webSetupServer.on("/gen_204", HTTP_GET, webSetupHandleCaptivePortal);
  webSetupServer.on("/hotspot-detect.html", HTTP_GET, webSetupHandleCaptivePortal);
  webSetupServer.on("/library/test/success.html", HTTP_GET, webSetupHandleCaptivePortal);
  webSetupServer.on("/ncsi.txt", HTTP_GET, webSetupHandleCaptivePortal);
  webSetupServer.on("/connecttest.txt", HTTP_GET, webSetupHandleCaptivePortal);
  webSetupServer.on("/fwlink", HTTP_GET, webSetupHandleCaptivePortal);
  webSetupServer.on("/api/status", HTTP_GET, webSetupHandleStatus);
  webSetupServer.on("/api/settings", HTTP_POST, webSetupHandleSettings);
  webSetupServer.on("/api/save", HTTP_POST, webSetupHandleSave);
  webSetupServer.on("/api/reset-defaults", HTTP_POST, webSetupHandleResetDefaults);
  webSetupServer.on("/api/mode", HTTP_POST, webSetupHandleMode);
  webSetupServer.onNotFound(webSetupHandleNotFound);
  webSetupServer.begin();

  webSetupActive = true;
  out.println("Tardi web controller: ON");
  out.println("WiFi AP and captive portal are available while powered.");
  webSetupPrintStatus(out);
}

void webSetupLoop() {
  if (!webSetupActive) {
    return;
  }

  if (webSetupDnsActive) {
    webSetupDnsServer.processNextRequest();
  }

  webSetupServer.handleClient();
}

bool webSetupIsActive() {
  return webSetupActive;
}

const char *webSetupSsid() {
  return WEB_SETUP_SSID;
}

const char *webSetupPassword() {
  return WEB_SETUP_PASSWORD;
}

String webSetupIpAddress() {
  if (!webSetupActive) {
    return "0.0.0.0";
  }

  return WiFi.softAPIP().toString();
}

uint8_t webSetupClientCount() {
  if (!webSetupActive) {
    return 0;
  }

  return WiFi.softAPgetStationNum();
}

void webSetupPrintStatus(Stream &out) {
  out.print("WIFI CONTROLLER active=");
  out.print(webSetupActive ? 1 : 0);
  out.print(" captive=");
  out.print(webSetupActive ? 1 : 0);
  out.print(" dns=");
  out.print(webSetupDnsActive ? 1 : 0);
  out.print(" ssid=");
  out.print(WEB_SETUP_SSID);
  out.print(" password=");
  out.print(WEB_SETUP_PASSWORD);
  out.print(" ip=");
  out.print(webSetupIpAddress());
  out.print(" clients=");
  out.println(webSetupClientCount());
}
