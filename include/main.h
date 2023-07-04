#ifndef MAIN_H
#define MAIN_H

#include <Wire.h>

#include "config.h"
#include "displayFunction.h"
#include "utils.h"
#include "buzz.h"
#include "welcome.h"
#include "amount.h"
#include "scan.h"
#include "gsmSetup.h"
#include "notify.h"

const byte ROWS = 4;
const byte COLS = 3;

char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

byte rowPins[ROWS] = {34, 36, 39, 35}; // connect to the row pinouts of the kpd
byte colPins[COLS] = {32, 14, 33};     // connect to the column pinouts of the kpd

Keypad kpd = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

const int DELAY_BETWEEN_CARDS = 500;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;

unsigned long previousMillis = 0;
unsigned long turnOnTime = 0;

int httpCode;

uint8_t processState = 0;

uint32_t amount = 0;

bool rechargeRequest; // for recharge and payment
bool buzzStatus = false;
uint8_t buzzMode = -1;

String userName;

TaskHandle_t display_handle = NULL;

/* TTGO T-Call pins */
const uint8_t MODEM_RST = 5;
const uint8_t MODEM_PWRKEY = 4;
const uint8_t MODEM_POWER_ON = 23;
const uint8_t MODEM_TX = 27;
const uint8_t MODEM_RX = 26;

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

/*Merchant Details*/
String MERCHANT_EMAIL = "ajay@gmail.com";
String MERCHANT_PASSWORD = "Hello123@";

// Layers stack
TinyGsm modem(SerialAT);
TinyGsmClient gsm_transpor_layer(modem);
SSLClient sslClient(&gsm_transpor_layer);
HttpClient http = HttpClient(sslClient, hostname, port);

#endif