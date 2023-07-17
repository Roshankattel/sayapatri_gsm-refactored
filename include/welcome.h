#ifndef WELCOME_H
#define WELCOME_H

#include <Arduino.h>
#include <Keypad.h>

extern bool rechargeRequest; // for recharge and payment
extern Keypad kpd;

bool welcomeProcess(void);

#endif