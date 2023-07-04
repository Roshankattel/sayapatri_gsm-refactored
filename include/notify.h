
#ifndef NOTIFY_H
#define NOTIFY_H
#include <Arduino.h>

extern bool rechargeRequest;
extern int httpCode;
extern String userName;
extern uint32_t amount;

void notifyProcess();

#endif