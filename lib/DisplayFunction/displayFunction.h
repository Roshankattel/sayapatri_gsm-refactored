#ifndef DISPLAYFUNCTION_H
#define DISPLAYFUNCTION_H

#include <TFT_eSPI.h>
#include <PNGdec.h>

#include "images/startAnimation.h"
#include "images/logo.h"
#include "images/battery.h"
#include "images/cellular.h"
#include "images/charging.h"
#include "images/bluetooth.h"
#include "images/diamond.h"

// diplay params
#define GFXFF 4
#define MAX_IMAGE_WDITH 320
// #define FF18 &FreeSans12pt7b
// #define CF_OL24 &Orbitron_Light_24

/*Images Cordinates*/
#define CHARGE_X 300
#define CHARGE_Y 2
#define BATTERY_X 270
#define BATTERY_Y 0
#define WIFI_X 240
#define WIFI_Y 0
#define BTH_X 215
#define BTH_Y 2
#define PAIR_X 219
#define PAIR_Y 15
#define SAYAPATRI_X 52
#define SAYAPATRI_Y 35

void tftInit();
void pngDraw(PNGDRAW *pDraw);
void clearText();
void clearLCD();
void clearScreen();
void showImage(int x, int y, uint8_t *image, int size);
void chargingDisplay();
void showPair();
void clearPair();
void homePage();
void startScreen();
void welcomePage(bool temp);
void showProcessing(void);
void amountPage(bool rechargeReq);
void gsmDisplay(bool status);
void showConnection(String ssid);
void scanPage(bool rechargeReq, int amount);
void showHotspot(uint32_t chipId);
void writeString(String msg, int X, int Y);

#endif