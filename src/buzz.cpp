#include "buzz.h"

void playBuzz(uint8_t mode)
{
    buzzMode = mode;
    if (buzzStatus)
    {
        return;
    }

    digitalWrite(BUZZER_PIN, HIGH);
    turnOnTime = millis();
    buzzStatus = true;
}

void checkBuzzerStatus()
{
    if (!buzzStatus)
    {
        return;
    }

    switch (buzzMode)
    {
    case SINGLE:
        if (millis() - turnOnTime >= 80)
        {
            debugln("\nturning off buzzer");
            digitalWrite(BUZZER_PIN, LOW);
            buzzStatus = false;
        }
        break;

    case SUCCESS:
        if (millis() - turnOnTime >= 200 and millis() - turnOnTime <= 2 * 200)
        {
            digitalWrite(BUZZER_PIN, LOW);
        }

        else if (millis() - turnOnTime > 2 * 200 and millis() - turnOnTime <= 3 * 200)
        {
            digitalWrite(BUZZER_PIN, HIGH);
        }

        else if (millis() - turnOnTime > 3 * 200)
        {
            digitalWrite(BUZZER_PIN, LOW);
            buzzStatus = false;
        }

        break;

    case ERROR:
        if (millis() - turnOnTime >= 500)
        {
            digitalWrite(BUZZER_PIN, LOW);
            buzzStatus = false;
        }
        break;
    }
}
