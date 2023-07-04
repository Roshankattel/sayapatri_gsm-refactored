#include "amount.h"
#include "config.h"
#include "displayFunction.h"

uint32_t sum = 0;

bool amountProcess()
{
    char pressedKey = kpd.getKey();
    if (pressedKey)
    {

        int *num = (int *)ps_malloc(sizeof(int));
        *num = int(pressedKey) - 48;

        if (*num > -1 and *num < 10 and sum <= (AMOUNT_LIMIT / 10))
        {
            sum = sum * 10 + *num;
            debug("Amount:-");
            debugln(sum);
            writeString(String(sum), 160, 180);
        }
        else if (*num == -6 && sum >= 0)
        {
            sum = sum / 10;
            debugln(sum);
            writeString("                   ", 160, 180); // Clearing values to rewrite
            writeString(String(sum), 160, 180);
        }
        else if (*num == -13 && sum > 1) // if '#' is Pressed
        {
            amount = sum;
            sum = 0;
            free(num);

            debugln("\nPlease Scan your Card\n");
            return true;
        }
    }
    return false;
}