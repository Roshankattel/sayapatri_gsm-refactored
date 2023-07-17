#include <main.h>

/*Private Function Prototypes*/
void ttgoGsmSetup(void);
void startListeningToNFC(void);
String handleCardDetected(void);
bool connectGPRS(void);
bool initializeModem(void);

void processDisplay(void *)
{
  for (;;)
  {
    showProcessing();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

bool connectGPRS(void)
{
  bool success = false;
  const int MAX_CONNECTION_ATTEMPTS = 3;
  int connectionAttempts = 0;

  while (!success && connectionAttempts < MAX_CONNECTION_ATTEMPTS)
  {
    debugln("\nConnecting to: " + String(GPRS_APN));

    if (!modem.isGprsConnected() && !modem.gprsConnect(GPRS_APN, GPRS_USERNAME, GPRS_PASSWORD))
    {
      debugln("FAILED to connect to GPRS");
      connectionAttempts++;
      delay(5000); // Wait 5 seconds before retrying
    }
    else
    {
      debugln("GPRS connected");
      success = true;
    }
  }

  if (!success)
  {
    debugln("Unable to connect to GPRS after multiple attempts");
  }

  return success;
}

void setup()
{

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

#if DEBUG == 1
  Serial.begin(115200);
#endif

  tftInit();

  clearScreen();
  xTaskCreatePinnedToCore(
      processDisplay,       // function name
      "Processing Display", // task name
      100000,               // stack size
      NULL,                 // task parameters,
      1,
      &display_handle,
      CONFIG_ARDUINO_EVENT_RUNNING_CORE);

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();

#if DEBUG == 1
  // Got ok data, print it out!
  Serial.print("\nFound chip PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF, DEC);
#endif

  // configure board to read RFID tags
  nfc.SAMConfig();

  if (!versiondata)
  {
    debugln("\nDidn't find PN53x board");
    writeString("NFC not working !", 160, 180);
    while (1)
      ; // halt
  }

  debugln("\nInitializing modem...");

  /*Check if SIM or Network available ?*/
  ttgoGsmSetup();

  /*Display Network availablity*/
  if (connectGPRS())
  {
    if (display_handle != NULL)
    {
      // vTaskSuspend(display_handle);
      vTaskDelete(display_handle);
    }

    homePage();
    gsmDisplay(true);
    welcomePage(false);
  }
  else
  {
    if (display_handle != NULL)
    {
      // vTaskSuspend(display_handle);
      vTaskDelete(display_handle);
    }

    homePage();

    debugln("Network Unavilable");
    gsmDisplay(false);
    lcdDisplay("Network Unavailable", "");
    delay(40000);
    ESP.restart();
  }
}

void loop()
{
  checkBuzzerStatus();

  switch (processState)
  {
  case WELCOME:
    if (welcomeProcess())
    {
      processState = AMOUNT;
      amountPage(rechargeRequest);
    }
    break;
  case AMOUNT:
    if (amountProcess())
    {
      processState = SCAN;
      scanPage(rechargeRequest, amount);
      startListeningToNFC();
    }
    break;
  case SCAN:
    if (scanProcess())
    {
      processState = NOTIFY;
    }
    break;
  case NOTIFY:
    notifyProcess();
    notifyTimer(false);
    break;
  }
}

void ttgoGsmSetup(void)
{
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  // Turn on the Modem power first
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Pull down PWRKEY for more than 1 second according to manual requirements
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(100);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  /*Check if SIM or Network available ?*/
  bool modemStat = initializeModem();

  while (!modemStat)
  {
    debugln("Modem initialization failed. Retrying in 1 minute...");
    delay(60000); // wait for 1 minute
    modemStat = initializeModem();
  }
}

bool initializeModem(void)
{

  if (!modem.restart())
  {
    debugln("Modem initialization failed.");
    return false;
  }

  // modem initialized successfully
  debugln("MODEM INITIALIZED");

  // generate device token
  // char deviceID[40];
  // snprintf(deviceID, 40, "%llu", ESP.getEfuseMac());
  // String imei = modem.getIMEI();

  return true;
}