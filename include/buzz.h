#ifndef BUZZ_H
#define BUZZ_H

#include <Arduino.h>
#include "config.h"

const uint8_t BUZZER_PIN = 12;

extern bool buzzStatus;
extern uint8_t buzzMode;
extern unsigned long turnOnTime;

void playBuzz(uint8_t mode);
void checkBuzzerStatus();

#endif