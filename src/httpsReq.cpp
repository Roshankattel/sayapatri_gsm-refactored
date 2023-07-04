#include "httpReq.h"
#include "config.h"
#include "ArduinoJson.h"

TinyGsmClient gsm_transpor_layer(modem);
SSLClient secure_presentation_layer(&gsm_transpor_layer);
HttpClient http = HttpClient(secure_presentation_layer, hostname, port);

String sendHttpPostRequest(const String endpoint, const String httpRequestData, const String token = "")
{
    if (!modem.isGprsConnected())
        return "";

    String payload;

    debugln("\nPOST REQUEST DATA:");
    debugln(httpRequestData);

    http.beginRequest();
    http.post(endpoint);

    if (!token.isEmpty())
        http.sendHeader("Authorization", "Bearer " + token);

    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", httpRequestData.length());

    http.beginBody();
    http.print(httpRequestData);
    http.endRequest();
    httpCode = http.responseStatusCode();
    payload = http.responseBody();
    http.stop();

    debugln("\n\nPOST REQUEST RESPONSE:");
    debugln(payload);

    if (httpCode != 200)
    {
        debugln("\n[HTTP] POST...Failed, code:" + String(httpCode));
        return "";
    }

    debugln("\n[HTTP] POST... Success, Code:" + String(httpCode));
    return payload;
}

String merchantLogin()
{
    String httpRequestData;
    httpRequestData.reserve(1024);

    SpiRamJsonDocument doc(4096);
    doc["email"] = MERCHANT_EMAIL;
    doc["password"] = MERCHANT_PASSWORD;
    doc["role"] = "merchant";

    serializeJson(doc, httpRequestData);

    String response = sendHttpPostRequest("/api/v1/auth/login", httpRequestData);
    if (response.isEmpty())
    {
        debugln("\nFailed! The response is empty");
        return "";
    }

    DeserializationError error = deserializeJson(doc, response);
    if (error)
        return "";

    return doc["accessToken"].as<String>();
}

String transactionRequest(const String accessToken, const String tagData)
{
    // DynamicJsonDocument doc(1024);
    String httpRequestData;
    httpRequestData.reserve(1024);

    SpiRamJsonDocument doc(2048);
    doc["tagData"] = tagData;
    doc["amount"] = amount;
    doc["rechargeRequest"] = rechargeRequest;
    doc["channel"] = "pos";

    serializeJson(doc, httpRequestData);

    String response = sendHttpPostRequest("/api/v1/transaction/add", httpRequestData, accessToken);

    if (response.isEmpty())
    {
        debugln("\nFailed! The response is empty");
        return "";
    }

    DeserializationError error = deserializeJson(doc, response);
    if (error)
        return "";

    userName = doc["client"]["name"].as<String>();
    return userName;
}
