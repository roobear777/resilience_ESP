#include "web_setup.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "led_expander_output.h"
#include "led_settings.h"

static const char *WEB_SETUP_SSID = "TARDI-LED";
static const char *WEB_SETUP_PASSWORD = "tardigrade";
static const byte WEB_SETUP_DNS_PORT = 53;

static DNSServer webSetupDnsServer;
static WebServer webSetupServer(80);
static bool webSetupActive = false;
static bool webSetupDnsActive = false;

static String webSetupLiveOutputLabel() {
  return ledExpanderOutputRealOutputAllowed() ? "ON" : "SIM ONLY";
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

static const char *webSetupZoneName(uint8_t zoneIndex) {
  switch (zoneIndex) {
    case 0:
      return "Mouth";
    case 1:
      return "Shoulder";
    case 2:
      return "Midbody";
    case 3:
      return "Rear";
    case 4:
      return "Front Legs";
    case 5:
      return "Back Legs";
    case 6:
      return "Digestive";
    case 7:
      return "Stations";
    default:
      return "Zone";
  }
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
  json += ledExpanderOutputRealOutputAllowed() ? "true" : "false";
  json += ",\"realOutputStarted\":";
  json += ledExpanderOutputRealOutputStarted() ? "true" : "false";
  json += ",\"mode\":\"";
  json += ledExpanderOutputModeName();
  json += "\",\"tx\":";
  json += String(ledExpanderOutputPlannedTxPin());
  json += ",\"baud\":";
  json += String(ledExpanderOutputPlannedBaudRate());
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

static String webSetupHtmlPage() {
  const LedLookSettings &initialLook = ledSettingsGlobalLook(LED_LOOK_AMBIENT);
  String html;
  html.reserve(10000);

  html += F("<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>Tardi Controller</title><style>");
  html += F("body{margin:0;background:#090d10;color:#eef7f6;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;font-size:17px;}");
  html += F("main{max-width:560px;margin:0 auto;padding:20px 16px 32px;}h1{font-size:30px;margin:0 0 8px;line-height:1.05;letter-spacing:.04em;}h2{font-size:21px;margin:28px 0 10px;color:#4dd6d0;letter-spacing:.05em;text-transform:uppercase;}");
  html += F(".sub{color:#9db2b7;margin:8px 0 12px;line-height:1.35}.hint{color:#4dd6d0;font-weight:800;letter-spacing:.04em;text-transform:uppercase}.small{font-size:14px;color:#9db2b7;line-height:1.35}");
  html += F(".card{border:1px solid #21444a;border-radius:8px;padding:17px;margin:14px 0;background:#121a1f;box-shadow:0 0 0 1px #0b1519 inset,0 8px 26px rgba(0,0,0,.28);}");
  html += F(".head{background:#0f1b21;border-color:#2c6268}.chips{display:flex;flex-wrap:wrap;gap:9px;margin-top:14px}.chip{border:1px solid #2c6268;background:#081114;color:#d8ffff;border-radius:999px;padding:7px 10px;font-size:13px;font-weight:800;letter-spacing:.03em}");
  html += F(".row{display:flex;justify-content:space-between;align-items:center;gap:16px;border-bottom:1px solid #223038;padding:10px 0}.row:last-child{border-bottom:0}");
  html += F(".k{color:#9db2b7}.v{text-align:right;font-weight:750}.cmd{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;background:#070b0d;padding:4px 7px;border-radius:5px;}");
  html += F(".ctrl{margin:20px 0 24px}.ctrl label{display:flex;justify-content:space-between;align-items:flex-end;gap:12px;margin-bottom:10px;font-size:18px}.ctrl strong{font-size:21px;color:#4dd6d0}.ctrl input{width:100%;height:34px;accent-color:#4dd6d0}");
  html += F("select{width:100%;min-height:48px;border:1px solid #2c6268;border-radius:8px;background:#081114;color:#eef7f6;font-size:18px;font-weight:750;padding:10px;margin-top:8px}");
  html += F(".buttons{display:grid;grid-template-columns:1fr;gap:12px}.buttons button,.wide{border:0;border-radius:8px;min-height:50px;padding:15px;background:#1f3037;color:#eef7f6;font-weight:800;font-size:16px}.primary{background:#4dd6d0!important;color:#081114!important}.danger{background:#63333d!important}.status{min-height:24px;color:#a7ff83;margin-top:14px;font-weight:700}");
  html += F(".target{display:grid;grid-template-columns:1fr;gap:14px;margin-bottom:18px}.target label{display:block;color:#9db2b7;font-size:14px;font-weight:800;letter-spacing:.04em;text-transform:uppercase}");
  html += F(".savebar{border-top:1px solid #223038;margin-top:12px;padding-top:18px}");
  html += F("@media(min-width:430px){.buttons{grid-template-columns:1fr 1fr}.buttons button{min-height:54px}}");
  html += F("</style></head><body><main>");
  html += F("<h1>Tardi Controller</h1>");
  html += F("<p class=\"sub\">You are connected to Tardi Controller. If this window disappears, open Safari/Chrome and go to http://192.168.4.1</p>");

  html += F("<h2>Global</h2><section class=\"card\">");
  html += F("<div class=\"target\"><label>Editing<select id=\"lookKind\" onchange=\"selectLookTarget()\"><option value=\"ambient\">Ambient</option><option value=\"animation\">Animation</option></select></label>");
  html += F("<label>Area<select id=\"lookArea\" onchange=\"selectLookTarget()\"><option value=\"whole\">Whole Sculpture</option>");
  for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
    html += F("<option value=\"");
    html += String(zone);
    html += F("\">");
    html += webSetupZoneName(zone);
    html += F("</option>");
  }
  html += F("</select></label></div>");
  html += F("<div class=\"ctrl\"><label><span>Brightness</span><strong id=\"brightnessPercentValue\">");
  html += String(webSetupByteToPercent(initialLook.brightness));
  html += F("%</strong></label><input id=\"brightnessPercent\" type=\"range\" min=\"0\" max=\"100\" value=\"");
  html += String(webSetupByteToPercent(initialLook.brightness));
  html += F("\" oninput=\"queueSetting('brightnessPercent',this.value)\" onchange=\"sendSetting('brightnessPercent',this.value)\"></div>");
  html += F("<div class=\"ctrl\"><label><span>Colour Intensity</span><strong id=\"saturationPercentValue\">");
  html += String(webSetupByteToPercent(initialLook.saturation));
  html += F("%</strong></label><input id=\"saturationPercent\" type=\"range\" min=\"0\" max=\"100\" value=\"");
  html += String(webSetupByteToPercent(initialLook.saturation));
  html += F("\" oninput=\"queueSetting('saturationPercent',this.value)\" onchange=\"sendSetting('saturationPercent',this.value)\"></div>");
  html += F("<div class=\"ctrl\"><label><span>Speed</span><strong id=\"speedValue\">");
  html += String(initialLook.speedPercent);
  html += F("%</strong></label><input id=\"speed\" type=\"range\" min=\"0\" max=\"200\" value=\"");
  html += String(initialLook.speedPercent);
  html += F("\" oninput=\"queueSetting('speed',this.value)\" onchange=\"sendSetting('speed',this.value)\"></div>");
  html += F("<div class=\"ctrl\"><label><span>Animation duration</span><strong id=\"animationDurationSecondsValue\">");
  html += String(ledSettingsGet().animationDurationSeconds);
  html += F(" seconds</strong></label><input id=\"animationDurationSeconds\" type=\"range\" min=\"1\" max=\"60\" value=\"");
  html += String(ledSettingsGet().animationDurationSeconds);
  html += F("\" oninput=\"queueGlobalSetting('animationDurationSeconds',this.value)\" onchange=\"sendGlobalSetting('animationDurationSeconds',this.value)\"></div>");
  html += F("<div class=\"ctrl\"><label><span>Colour Palette</span><strong id=\"paletteValue\">");
  html += ledSettingsPaletteName(initialLook.paletteMode);
  html += F("</strong></label><select id=\"palette\" onchange=\"sendSetting('palette',this.value)\">");
  html += F("<option value=\"default\">Default</option><option value=\"warm\">Warm</option><option value=\"cool\">Cool</option><option value=\"ember\">Ember</option><option value=\"ocean\">Ocean</option><option value=\"rainbow\">Rainbow</option></select></div>");
  html += F("<div class=\"ctrl\"><label><span>Behaviour</span><strong id=\"behaviorValue\">");
  html += ledSettingsBehaviorName(initialLook.behaviorMode);
  html += F("</strong></label><select id=\"behavior\" onchange=\"sendSetting('behavior',this.value)\">");
  html += F("<option value=\"normal\">Default</option><option value=\"calm\">Calm</option><option value=\"energetic\">Energetic</option><option value=\"sparkle\">Sparkle</option></select></div>");
  html += F("<div class=\"savebar\"><button onclick=\"saveSettings()\" class=\"wide primary\">SAVE</button>");
  html += F("<div class=\"status\" id=\"message\"></div></div></section>");

  html += F("<h2>Reset</h2><section class=\"card\">");
  html += F("<button onclick=\"resetDefaults()\" class=\"wide\">RESET</button>");
  html += F("<p class=\"small\">Reset is temporary until you save.</p></section>");

  html += F("<h2>Connection</h2><section class=\"card head\">");
  html += F("<div class=\"hint\">Local sculpture link</div>");
  html += F("<div class=\"sub\">Controller hotspot active</div>");
  html += F("<div class=\"chips\"><span class=\"chip\" id=\"liveOutput\">");
  html += webSetupLiveOutputLabel();
  html += F("</span><span class=\"chip\">Mode: <span id=\"mode\">");
  html += ledExpanderOutputModeName();
  html += F("</span></span><span class=\"chip\">");
  html += WEB_SETUP_SSID;
  html += F("</span><span class=\"chip\">");
  html += webSetupIpAddress();
  html += F("</span></div><div class=\"row\"><span class=\"k\">Started</span><span class=\"v\" id=\"started\">");
  html += ledExpanderOutputRealOutputStarted() ? "YES" : "NO";
  html += F("</span></div><div class=\"row\"><span class=\"k\">LED UART</span><span class=\"v\">GPIO");
  html += String(ledExpanderOutputPlannedTxPin());
  html += F(" / ");
  html += String(ledExpanderOutputPlannedBaudRate());
  html += F("</span></div><div class=\"row\"><span class=\"k\">Clients</span><span class=\"v\">");
  html += String(webSetupClientCount());
  html += F("</span></div></section>");

  html += F("<h2>Serial Commands</h2><section class=\"card\">");
  html += F("<p><span class=\"cmd\">led settings</span></p>");
  html += F("<p><span class=\"cmd\">led set brightness N</span></p>");
  html += F("<p><span class=\"cmd\">led save</span></p>");
  html += F("<p><span class=\"cmd\">led defaults save</span></p>");
  html += F("</section><script>");
  html += F("let timers={},lastStatus=null;function msg(t){document.getElementById('message').textContent=t||'';}");
  html += F("function label(k,v){if(k=='animationDurationSeconds')return v+' seconds';if(k.endsWith('Percent')||k=='speed')return v+'%';return v;}function val(id,v){let e=document.getElementById(id+'Value');if(e)e.textContent=label(id,v);}");
  html += F("function post(u){return fetch(u,{method:'POST'}).then(r=>r.text()).then(t=>{msg(t);refresh();return t;}).catch(e=>msg('Request failed'));}");
  html += F("function targetQuery(){return 'look='+encodeURIComponent(document.getElementById('lookKind').value)+'&area='+encodeURIComponent(document.getElementById('lookArea').value);}");
  html += F("function selectedLook(s){let k=document.getElementById('lookKind').value,a=document.getElementById('lookArea').value,g=s.settings.looks[k];return a=='whole'?g.whole:g.zones[Number(a)];}");
  html += F("function applyLookControls(look){['brightnessPercent','saturationPercent','speed','palette','behavior'].forEach(k=>{document.getElementById(k).value=look[k];val(k,look[k]);});}");
  html += F("function selectLookTarget(){if(lastStatus)applyLookControls(selectedLook(lastStatus));}");
  html += F("function queueSetting(k,v){val(k,v);let q=targetQuery();clearTimeout(timers[k]);timers[k]=setTimeout(()=>sendSetting(k,v,q),250);}");
  html += F("function sendSetting(k,v,q){val(k,v);return post('/api/settings?'+(q||targetQuery())+'&'+encodeURIComponent(k)+'='+encodeURIComponent(v));}");
  html += F("function queueGlobalSetting(k,v){val(k,v);clearTimeout(timers[k]);timers[k]=setTimeout(()=>sendGlobalSetting(k,v),250);}");
  html += F("function sendGlobalSetting(k,v){val(k,v);return post('/api/settings?'+encodeURIComponent(k)+'='+encodeURIComponent(v));}");
  html += F("function saveSettings(){return fetch('/api/save',{method:'POST'}).then(r=>r.text()).then(t=>{msg(t=='Saved'?'Saved':t);refresh();return t;}).catch(e=>msg('Request failed'));}");
  html += F("function resetDefaults(){return post('/api/reset-defaults').then(refresh);}");
  html += F("function applyStatus(s){lastStatus=s;document.getElementById('liveOutput').textContent=s.liveOutput;document.getElementById('mode').textContent=s.mode;document.getElementById('started').textContent=s.realOutputStarted?'YES':'NO';document.getElementById('animationDurationSeconds').value=s.settings.animationDurationSeconds;val('animationDurationSeconds',s.settings.animationDurationSeconds);applyLookControls(selectedLook(s));}");
  html += F("function refresh(){return fetch('/api/status').then(r=>r.json()).then(applyStatus).catch(e=>{});}refresh();setInterval(refresh,5000);");
  html += F("</script>");
  html += F("</main></body></html>");
  return html;
}

static void webSetupHandleRoot() {
  webSetupServer.send(200, "text/html", webSetupHtmlPage());
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
      webSetupServer.send(400, "text/plain", "Use: look=ambient|animation&area=whole|0..7");
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
    } else if (area.length() == 1 && area[0] >= '0' && area[0] <= '7') {
      targetLook = &ledSettingsMutableZoneLook(lookKind, static_cast<uint8_t>(area[0] - '0'));
    } else {
      webSetupServer.send(400, "text/plain", "Invalid area. Use whole or 0..7.");
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
      webSetupServer.send(400, "text/plain", "Use: zone=0..7&value=0..255");
      return;
    }

    String zoneArg = webSetupServer.arg("zone");
    uint8_t value = 0;

    if (
      zoneArg.length() != 1
      || zoneArg[0] < '0'
      || zoneArg[0] > '7'
      || !webSetupParseByteValue(webSetupServer.arg("value"), value)
    ) {
      webSetupServer.send(400, "text/plain", "Use: zone=0..7&value=0..255");
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
    ok = ledExpanderOutputSetMode(LED_OUTPUT_OFF, Serial);
  } else if (value == "animation") {
    ok = ledExpanderOutputSetMode(LED_OUTPUT_ANIMATION, Serial);
  } else if (value == "solid") {
    ok = ledExpanderOutputSetMode(LED_OUTPUT_VALIDATE_SOLID, Serial);
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
