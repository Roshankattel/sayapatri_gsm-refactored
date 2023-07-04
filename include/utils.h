#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "buzz.h"
#include "displayFunction.h"

extern uint8_t processState;
extern uint32_t amount;

const String getHash(const String data);
void lcdDisplay(const char *errMsg1, const char *errMsg2);
void notifyTimer(const bool upperBar);

#endif