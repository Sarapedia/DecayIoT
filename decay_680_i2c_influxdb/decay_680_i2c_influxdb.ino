#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define WIFI_SSID "xxx"
#define WIFI_PASSWORD "xxx"

#define INFLUXDB_URL "xxx"
#define INFLUXDB_TOKEN "xxx"
#define INFLUXDB_ORG "xxx"
#define INFLUXDB_BUCKET "xxx"

#define TZ_INFO "UTC2"

#define BIOGAS_TESTSETUP_ID "0"  // Uniqe Test-Setup ID
//defines for bmp680
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Arduino.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)
unsigned long previousMillis = 0;
const long interval = 10000;  
Adafruit_BME680 bme; // I2C (Pin 22scl, pin 21 sda) on esp32, D1,D2 on wemosd1 mini
float temperature = 0.0;
float humidity = 0.0;
float gas = 0.0;
float pressure = 0.0;



InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("biogas_data");

void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Add tags to the data point
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());
  sensor.addTag("Biogas_Testsetup_ID", BIOGAS_TESTSETUP_ID);

  // Setup Biogas-Sensors

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }  
// Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  
}
void loop() {

  // Read Biogas-Sensors
if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }
  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Gas = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.println();
  delay(2000);

  //dht loop
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;
    // Read temperature as Celsius (the default)
    float newT = bme.temperature;
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float newT = dht.readTemperature(true);
    // if temperature read failed, don't change t value
    if (isnan(newT)) {
      Serial.println("Failed to read from bme sensor!");
    }
    else {
      temperature = newT;
      Serial.println(temperature);
    }
    // Read Humidity
    float newH = bme.humidity;
    // if humidity read failed, don't change h value 
    if (isnan(newH)) {
      Serial.println("Failed to read from bme sensor!");
    }
    else {
      humidity = newH;
      Serial.println(humidity);
    }
    // read Gas resistance
    float newG = bme.gas_resistance/1000;
    // if humidity read failed, don't change h value 
    if (isnan(newG)) {
      Serial.println("Failed to read from bme sensor!");
    }
    else {
      gas = newG;
      Serial.println(gas);
    }
    // read pressure
     float newP = bme.pressure/100;
    // if humidity read failed, don't change h value 
    if (isnan(newP)) {
      Serial.println("Failed to read from bme sensor!");
    }
    else {
      pressure = newP;
      Serial.println(pressure);
    }
    
  }
  

 write_datapoint_to_database(humidity, pressure, temperature, gas);
  
  // delay(300000);
}

int write_datapoint_to_database(float humidity, float pressure, float temperature, float gas) {

  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
    return 0;
  } else {

    sensor.clearFields();
    sensor.addField("humidity", humidity);
    sensor.addField("pressure", pressure);
    sensor.addField("temperature", temperature);
    sensor.addField("gas", gas);


    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
      return 0;
    } else {
      return 1;
    }
  }
}
