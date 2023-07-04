#ifndef GSMSETUP_H
#define GSMSETUP_H

// Configure TinyGSM library
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 1030 // Set RX buffer to 1Kb

#include "psram.h"
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <SSLClient.h>

// Layers stack
extern TinyGsm modem;
extern TinyGsmClient client;

#endif