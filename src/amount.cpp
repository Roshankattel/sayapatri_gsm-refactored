#include "amount.h"
#include "config.h"
#include "displayFunction.h"

uint32_t sum = 0;

bool amountProcess(void)
{
    char pressedKey = kpd.getKey();
    if (pressedKey)
    {

        int *digit = (int *)ps_malloc(sizeof(int));
        *digit = int(pressedKey) - 48;

        if (*digit > -1 and *digit < 10 and sum <= (AMOUNT_LIMIT / 10))
        {
            sum = sum * 10 + *digit;
            debug("Amount:-");
            debugln(sum);
            writeString(String(sum), 160, 180);
        }
        else if (*digit == -6 && sum >= 0)
        {
            sum = sum / 10;
            debugln(sum);
            writeString("                   ", 160, 180); // Clearing values to rewrite
            writeString(String(sum), 160, 180);
        }
        else if (*digit == -13 && sum > 1) // if '#' is Pressed
        {
            amount = sum;
            sum = 0;
            free(digit);

            debugln("\nPlease Scan your Card\n");
            return true;
        }
        free(digit);
    }
    return false;
}