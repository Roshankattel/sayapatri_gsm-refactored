#include "utils.h"
#include "mbedtls/md.h"
#include "config.h"
#include "gsmSetup.h"

void notifyTimer(const bool upperBar)
{
    if (millis() - turnOnTime > NOTIFY_TIME)
    {
        welcomePage(upperBar);
        gsmDisplay(modem.isGprsConnected());
        debugln("Called Notify Timer");
        processState = WELCOME;
        buzzMode = 0;
        amount = 0;
    }
}

void lcdDisplay(const char *errMsg1, const char *errMsg2)
{
    clearLCD();
    if (*errMsg2 == '\0')
    {
        writeString(errMsg1, 160, 180);
    }

    else
    {
        writeString(errMsg1, 160, 140);
        writeString(errMsg2, 160, 180);
    }
}

const String getHash(const String data)
{
    char *data_arr = (char *)ps_malloc((data.length() + 1) * sizeof(char));
    data.toCharArray(data_arr, data.length() + 1);
    char *payload = data_arr;

    byte *shaResult = (byte *)ps_malloc((32) * sizeof(byte));

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    const size_t payloadLength = strlen(payload);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char *)payload, payloadLength);
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);

    // debug("Hash: ");

    String hashedData;
    for (int i = 0; i < 32; i++)
    {
        char str[3];
        sprintf(str, "%02x", (int)shaResult[i]);
        // debug(str);
        hashedData += str;
    }
    free(data_arr);
    free(shaResult);
    return (hashedData);
}
