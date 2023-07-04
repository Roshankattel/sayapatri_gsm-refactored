#ifndef AMOUNT_H
#define AMOUNT_H

#include <Arduino.h>
#include <Keypad.h>

extern Keypad kpd;
extern uint32_t amount;
bool amountProcess();

#endif