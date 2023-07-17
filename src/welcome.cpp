#include "welcome.h"
#include "config.h"

bool welcomeProcess(void)
{
    /*Input From Keypad*/
    char pressedKey = kpd.getKey();
    if (pressedKey)
    {

        debug("Button Pressed:-");
        debugln(pressedKey);

        int *num = (int *)ps_malloc(sizeof(int));
        *num = int(pressedKey) - 48;
        if (*num == 1 || *num == 2)
        {
            if (*num == 1)
            {
                rechargeRequest = true;
                debugln("\nPlease Enter RECHARGE Amount\n");
            }
            else
            {
                rechargeRequest = false;
                debugln("\nPlease Enter PAY Amount\n");
            }
            free(num);
            return true;
        }
    }
    return false;
}