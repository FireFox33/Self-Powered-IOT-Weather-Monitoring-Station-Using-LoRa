#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>

// WiFi credentials
const char *ssid = "XXXXXX";
const char *password = "XXXXXX";

// Google Script ID
const String GOOGLE_SCRIPT_ID = "ADD UR GOOGLE SCRIPT ID";

// LoRa pins for ESP32
#define SCK_LORA 5
#define MISO_LORA 19
#define MOSI_LORA 27
#define SS_LORA 18
#define RST_LORA 14
#define DIO0_LORA 26

// Define the structure for sensor data
struct SensorData
{
    float temperature;
    float humidity;
    float pressure;
    String rainStatus;
    int soilMoisture;
    int CO;
    int CO2;
    int NH4;
    int airQuality;
};

SensorData receivedData;

void setup()
{
    Serial.begin(115200);
    // Connect to WiFi
    WiFi.mode(WIFI_STA); // Set WiFi to station mode
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize LoRa
    Serial.println("Initializing LoRa...");
    SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_LORA);
    LoRa.setPins(SS_LORA, RST_LORA, DIO0_LORA);

    if (!LoRa.begin(433E6))
    {
        Serial.println("LoRa initialization failed!");
        while (1)
            ;
    }

    LoRa.setSyncWord(0xF3);
    Serial.println("LoRa initialized successfully!");
}

void loop()
{
    // Check if data is available from LoRa
    int packetSize = LoRa.parsePacket();

    if (packetSize)
    {
        Serial.println("Received LoRa packet");

        // Read LoRa packet
        String message = "";
        while (LoRa.available())
        {
            message += (char)LoRa.read();
        }

        Serial.println("Message: " + message);

        // Parse data from the message
        parseLoRaData(message);

        // Send data to Google Sheets
        sendDataToGoogleSheets();
    }

    delay(100); // Small delay to prevent CPU hogging
}

void parseLoRaData(String message)
{
    // Expected format: "temp:25.5,hum:60.2,pres:1013.5,rain:NotRaining,soil:45,air:320"

    int tempIndex = message.indexOf("temp:");
    int humIndex = message.indexOf("hum:");
    int presIndex = message.indexOf("pres:");
    int rainIndex = message.indexOf("rain:");
    int soilIndex = message.indexOf("soil:");
    int coIndex = message.indexOf("CO:");
    int co2Index = message.indexOf("CO2:");
    int nh4Index = message.indexOf("NH4:");
    int airIndex = message.indexOf("air:");

    if (tempIndex >= 0)
    {
        String tempStr = message.substring(tempIndex + 5, message.indexOf(",", tempIndex));
        receivedData.temperature = tempStr.toFloat();
    }

    if (humIndex >= 0)
    {
        String humStr = message.substring(humIndex + 4, message.indexOf(",", humIndex));
        receivedData.humidity = humStr.toFloat();
    }

    if (presIndex >= 0)
    {
        String presStr = message.substring(presIndex + 5, message.indexOf(",", presIndex));
        receivedData.pressure = presStr.toFloat();
    }

    if (rainIndex >= 0)
    {
        int rainEndIndex = message.indexOf(",", rainIndex);
        receivedData.rainStatus = message.substring(rainIndex + 5, rainEndIndex >= 0 ? rainEndIndex : message.length());
    }

    if (soilIndex >= 0)
    {
        String soilStr = message.substring(soilIndex + 5, message.indexOf(",", soilIndex));
        receivedData.soilMoisture = soilStr.toInt();
    }

    if (airIndex >= 0)
    {
        int airEndIndex = message.indexOf(",", airIndex);
        String airStr = message.substring(airIndex + 4, airEndIndex >= 0 ? airEndIndex : message.length());
        receivedData.airQuality = airStr.toInt();
    }

    // Print parsed data for debugging
    Serial.println("Parsed data:");
    Serial.print("Temperature: ");
    Serial.println(receivedData.temperature);
    Serial.print("Humidity: ");
    Serial.println(receivedData.humidity);
    Serial.print("Pressure: ");
    Serial.println(receivedData.pressure);
    Serial.print("Rain Status: ");
    Serial.println(receivedData.rainStatus);
    Serial.print("Soil Moisture: ");
    Serial.println(receivedData.soilMoisture);
    Serial.print("Air Quality: ");
    Serial.println(receivedData.airQuality);
}

void sendDataToGoogleSheets()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi not connected");
        return;
    }
    HTTPClient http;
    String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec";

    // Add URL parameters for data
    url += "?temperature=" + String(receivedData.temperature);
    url += "&humidity=" + String(receivedData.humidity);
    url += "&pressure=" + String(receivedData.pressure);
    url += "&rain=" + receivedData.rainStatus;
    url += "&soil=" + String(receivedData.soilMoisture);
    url += "&air=" + String(receivedData.airQuality);

    Serial.println("Making HTTP request to: " + url);

    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    // Send HTTP GET request
    int httpCode = http.GET();

    if (httpCode > 0)
    {
        String payload = http.getString();
        Serial.println("HTTP Response code: " + String(httpCode));
        Serial.println("Response: " + payload);
    }
    else
    {
        Serial.println("Error on HTTP request: " + http.errorToString(httpCode));
    }
    http.end();
    Serial.println();
}
