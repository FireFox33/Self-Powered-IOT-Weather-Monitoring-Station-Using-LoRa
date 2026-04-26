#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>

// MQ135 definitions
#define placa "ESP-32"
#define Voltage_Resolution 3.3 // 5v for arduino, 3.3v for esp32
#define MQ135_PIN 33
#define type "MQ-135"          // MQ135
#define ADC_Bit_Resolution 12  // 10 for arduino, 12 for esp32
#define RatioMQ135CleanAir 3.6 // RS / R0 = 3.6 ppm(adjust as needed)
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, MQ135_PIN, type);

// LoRa pins
#define SCK_LORA 5
#define MISO_LORA 19
#define MOSI_LORA 27
#define SS_LORA 18
#define RST_LORA 14
#define DIO0_LORA 26
#define DHTPIN 4 // GPIO4 on ESP32
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Sensor pins
#define RAIN_SENSOR_PIN 35   // Digital pin for rain sensor
#define SOIL_MOISTURE_PIN 32 // Analog pin for soil moisture sensor
#define MQ135_PIN 33         // Analog pin for MQ135 gas sensor

// Variables for sensor data
float temperature;
float humidity;
float pressure;
String rainStatus;
int soilMoisture;
int airQuality;
int CO;
int CO2;
int NH4;

void setup()
{
    Serial.begin(115200);
    // Setup sensor pins
    pinMode(RAIN_SENSOR_PIN, INPUT);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(MQ135_PIN, INPUT);
    // Initialize DHT
    dht.begin();
    // Initialize MQ135
    MQ135.setRegressionMethod(1);
    MQ135.init();
    // calibrating MQ135
    Serial.print("Calibrating please wait.");
    float calcR0 = 0;
    for (int i = 1; i <= 10; i++)
    {
        MQ135.update(); // Update data, the arduino will read the voltage from the analog pin
        calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
        Serial.print(".");
    }
    MQ135.setR0(calcR0 / 10);
    Serial.println(" done!.");
    if (isinf(calcR0))
    {
        Serial.println("Warning: Conection issue, R0 is infinite (Open circuit
                       detected) please check your wiring and supply ");
            while (1);
    }
    if (calcR0 == 0)
    {
        Serial.println("Warning: Conection issue found, R0 is zero (Analog pin
                       shorts to ground) please check your wiring and supply ");
            while (1);
    }
    // calibration done.
    // MQ135.serialDebug(true); //debug for mq lib
    //  Initialize LoRa

    Serial.println("Initializing LoRa transmitter...");
    SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_LORA);
    LoRa.setPins(SS_LORA, RST_LORA, DIO0_LORA);
    if (!LoRa.begin(433E6))
    {
        Serial.println("LoRa initialization failed!");
        while (1)
            ;
    }
    // Set LoRa transmission parameters

    LoRa.setSyncWord(0xF3);
    LoRa.setTxPower(20);
    Serial.println("LoRa initialized successfully!");
    Serial.println("Transmitter setup complete. Will start sending data...");
}

void loop()
{
    // Read sensor data
    readSensorData();

    // Print data to serial monitor
    printSensorData();

    // Transmit data
    transmitData();

    delay(5000); // Small delay when not in deep sleep mode
}

void readSensorData()
{

    MQ135.update(); // read rawvalue
    MQ135.setA(605.18);
    MQ135.setB(-3.937); // CO mapping
    CO = MQ135.readSensor();
    MQ135.setA(110.47);
    MQ135.setB(-2.862); // CO2 mapping
    CO2 = MQ135.readSensor();
    MQ135.setA(102.2);
    MQ135.setB(-2.473); // NH4 mapping
    NH4 = MQ135.readSensor();
    // Read DHT22
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    // Read rain sensor (digital)
    int rainValue = digitalRead(RAIN_SENSOR_PIN);
    rainStatus = (rainValue == LOW) ? "Raining" : "Not_Raining";

    // Read soil moisture (analog)
    int soilRaw = analogRead(SOIL_MOISTURE_PIN);
    // Convert to percentage
    // 3500-dry value and 1500-wet value
    soilMoisture = map(soilRaw, 3500, 1700, 0, 100);
    soilMoisture = constrain(soilMoisture, 0, 100); // Keep within 0-100%

    // Read MQ135 air quality sensor (analog)
    airQuality = analogRead(MQ135_PIN);
}

void printSensorData()
{
    Serial.println("Sensor readings:");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("Rain Status: ");
    Serial.println(rainStatus);

    Serial.print("Soil Moisture: ");
    Serial.print(soilMoisture);
    Serial.println(" %");

    Serial.print("CO (MQ135): ");
    Serial.println(CO);
    Serial.print("CO2 (MQ135): ");
    Serial.println(CO2);
    Serial.print("NH4 (MQ135): ");
    Serial.println(NH4);
    Serial.print("Air Quality (MQ135): ");
    Serial.println(airQuality);
}

void transmitData()
{
    // Format data string with all sensor values
    String dataString = "temp:" + String(temperature) +
                        ",hum:" + String(humidity) +
                        ",rain:" + rainStatus +
                        ",soil:" + String(soilMoisture) +
                        ",CO:" + String(CO) +
                        ",CO2:" + String(CO2) +
                        ",NH4:" + String(NH4) +
                        ",air:" + String(airQuality);

    Serial.println("Transmitting: " + dataString);

    // Begin LoRa packet
    LoRa.beginPacket();
    LoRa.print(dataString);
    bool success = LoRa.endPacket();
    // Serial.println(success);

    if (success)
    {
        Serial.println("Packet sent successfully!");
    }
    else
    {
        Serial.println("Failed to send packet!");
    }
    Serial.print("Data string length: ");
    Serial.println(dataString.length());
}
