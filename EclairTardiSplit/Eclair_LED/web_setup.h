#ifndef TARDI_WEB_SETUP_H
#define TARDI_WEB_SETUP_H

#include <Arduino.h>

void webSetupBegin(bool enabled, Stream &out);
void webSetupLoop();
bool webSetupIsActive();
const char *webSetupSsid();
const char *webSetupPassword();
String webSetupIpAddress();
uint8_t webSetupClientCount();
void webSetupPrintStatus(Stream &out);

#endif
