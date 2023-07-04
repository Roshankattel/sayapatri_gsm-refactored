#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define DISABLE_ALL_LIBRARY_WARNINGS

#define DEBUG 1

#define TEST 0 // Enable this to test the flow with dummy data

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

const int WIFI_RECONNECT = 3000;  // Wifi Reconnect time(milliseconds)
const int TIMEOUT = 120;          // seconds to run for wifi connection
const int AMOUNT_LIMIT = 1000000; // Amount limit is 1 lakh
const int NOTIFY_TIME = 10000;

const int DIGIT_ZERO_ASCII_VALUE = 48;

// Server details
const char hostname[] = "rfid.goswivt.com";
const int port = 443;

// JSON configuration file
const String CONFIG_FILE = "/config.json";

/* PN532  PINS */
const uint8_t PN532_IRQ = 13;
const uint8_t PN532_RESET = 25;

const char GPRS_APN[16] = "web";
const char GPRS_USERNAME[16] = "";
const char GPRS_PASSWORD[16] = "";
const char GPRS_SIM_PIN[16] = "";

enum PROCESS
{
    WELCOME,
    AMOUNT,
    SCAN,
    NOTIFY
};

enum BUZZ_MODE
{
    SINGLE,
    SUCCESS,
    ERROR
};

#endif