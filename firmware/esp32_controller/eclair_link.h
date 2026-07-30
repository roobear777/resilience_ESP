#ifndef TARDI_ECLAIR_LINK_H
#define TARDI_ECLAIR_LINK_H
#include <Arduino.h>
void eclairLinkBegin();
void eclairLinkUpdate(uint32_t nowMs);
bool eclairLinkOnline();
bool eclairLinkFirstShowAttempted();
void eclairLinkPrintStatus(Stream &out);
#endif
