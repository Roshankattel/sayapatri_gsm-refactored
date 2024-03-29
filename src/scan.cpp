#include "scan.h"
#include "buzz.h"
#include "utils.h"
#include "gsmSetup.h"

void startListeningToNFC(void)
{
    // Reset our IRQ indicators
    irqPrev = irqCurr = HIGH;

    Serial.println("Waiting for an ISO14443A Card ...");
    nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

String handleCardDetected(void)
{
    uint8_t success = false;
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
    uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    // read the NFC tag's info
    success = nfc.readDetectedPassiveTargetID(uid, &uidLength);
    debugln(success ? "Read successful" : "Read failed (not a card?)");
    String unhashTag;
    if (success)
    {
        // Display some basic information about the card
        debugln("Found an ISO14443A card");
        debug("  UID Length: ");
        Serial.print(uidLength, DEC);
        debugln(" bytes");
        debug("  UID Value: ");
        nfc.PrintHex(uid, uidLength);

        for (int i = 6; i > -1; i--)
        {
            unhashTag += int(uid[i]);
        }
        debugln(unhashTag);
        return (unhashTag);
    }
    return ("");
}

bool scanProcess(void)
{

#if TEST == 1
    debugln("Running Test Case");
    lcdDisplay("Running Test Case!", "");
    const String accessToken = merchantLogin();
    if (accessToken != "")
    {
        const String tag = "ff3e3a07fee14c0562c6dba74ae7a483a8ea5a7d80ffadba5b5116971d0af47d";
        userName = transactionRequest(accessToken, tag);
    }

    return 1;
#else
    if (buzzMode == ERROR)
    {
        notifyTimer(true);
        return false;
    }

    irqCurr = digitalRead(PN532_IRQ);

    if (irqCurr == HIGH || irqPrev == LOW)
    {
        irqPrev = irqCurr;
        return false;
    }

    clearText();
    lcdDisplay("Card Scan Sucessful !", "");

    debugln("Got NFC IRQ");

    String tag = getHash(handleCardDetected());

    if (!modem.isGprsConnected())
    {
        debugln("GPRS not connected !");
        return false;
    }

    String accessToken = merchantLogin();

    if (httpCode == 200)
    {
        lcdDisplay("P R O C E S S I N G", "");
        httpCode = 0;
        userName = transactionRequest(accessToken, tag);
        return 1;
    }
    else if (httpCode == 404)
    {
        lcdDisplay("Merchant Not Found !", "");
        playBuzz(ERROR);
    }
    else if (httpCode == 400)
    {
        lcdDisplay("Invalid Credentials!", "");
        playBuzz(ERROR);
    }
    else if (httpCode == -1)
    {
        clearLCD();
        lcdDisplay("No Internet", "");
        delay(1000);
        welcomePage(true);
        playBuzz(ERROR);
    }
    else
    {
        lcdDisplay("Merchant Login Failed", "");
    }
    if (httpCode != 200)
    {
        playBuzz(ERROR);
    }

    tag = "";

    irqPrev = irqCurr;
    return false;
#endif
}