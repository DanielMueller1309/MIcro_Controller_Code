/**
 * Secure Write Example code for InfluxDBClient library for Arduino
 * Enter WiFi and InfluxDB parameters below
 *
 * Demonstrates connection to any InfluxDB instance accesible via:
 *  - unsecured http://...
 *  - secure https://... (appropriate certificate is required)
 *  - InfluxDB 2 Cloud at https://cloud2.influxdata.com/ (certificate is preconfigured)
 * Measures signal level of the actually connected WiFi network
 * This example demonstrates time handling, secure connection and measurement writing into InfluxDB
 * Data can be immediately seen in a InfluxDB 2 Cloud UI - measurement wifi_status
 *
 * Complete project details at our blog: https://RandomNerdTutorials.com/
 *
 **/

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

#include <Wire.h>
#include <SHT3x.h>
//mq2stuff
#include <MQ2.h>


//envy sensors
char temperatureCString[6];
char humidityString[6];
char MQ2String[3];
SHT3x sht30(0x44); //adress of SHT30
float humid; // for realhomidity
const int analogInPin = A0;  //ADC-pin of AZ-Envy for the gas sensor
//mq2 pin order
MQ2 mq2(analogInPin);
//mq2 vars of gas
float lpg, co, smoke;

// WiFi AP SSID
#define WIFI_SSID "WLANSSID"
// WiFi password
#define WIFI_PASSWORD "WLANPW"
// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "http://docker.home:8086"
// InfluxDB v2 server or cloud API token (Use: InfluxDB UI -> Data -> API Tokens -> Generate API Token)
#define INFLUXDB_TOKEN "Token-ID"
// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG "homenet"
// InfluxDB v2 bucket name (Use: InfluxDB UI ->  Data -> Buckets)
#define INFLUXDB_BUCKET "az_envy"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "WET0WEST,M3.5.0/1,M10.5.0"

// InfluxDB client instance with preconfigured InfluxCloud certificate
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// InfluxDB client instance without preconfigured InfluxCloud certificate for insecure connection
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Data point
Point wlan("Wifi");
Point sensor("Sensors");

//sensor data get
void getData() {
  sht30.UpdateData();

  float temperature = sht30.GetTemperature(); //read the temperature from SHT30
        humid = sht30.GetRelHumidity(); //read the humidity from SHT30
  int sensorValue = analogRead(analogInPin); //read the ADC-pin → connected to MQ-2

  //calibrate your temperature values - due to heat reasons from the MQ-2 (up to 4°C)
  float temperature_deviation = 0.5; //enter the deviation in order to calibrate the temperature value
  float temperature_calibrated = temperature - temperature_deviation; //final value

  sprintf(MQ2String, "%d",sensorValue);
  dtostrf(temperature_calibrated, 5, 1, temperatureCString);
  dtostrf(humid, 5, 1, humidityString);

  // mq2 get data
  lpg = mq2.readLPG();
  co = mq2.readCO();
  smoke = mq2.readSmoke();

///*
  Serial.print("Temperature: ");
  Serial.println(temperature_calibrated);
  Serial.println(temperatureCString);
  Serial.print("Rel.Humidity: ");
  Serial.println(humid);
  Serial.println(humidityString);
  Serial.print("MQ-2 Sensor = ");
  Serial.println(sensorValue);
  Serial.println(MQ2String);
  Serial.println("lpg:");
  Serial.println(lpg);
  Serial.println("co:");
  Serial.println(co);
  Serial.println("smoke:");
  Serial.println(smoke);

  delay(100);
//  */
 }




void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  // Print the IP address
  Serial.println(WiFi.localIP());
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  // Add tags
  wlan.addTag("device", DEVICE);
  wlan.addTag("SSID", WiFi.SSID());
  sensor.addTag("device", "AZ-Envy");

  // Alternatively, set insecure connection to skip server certificate validation
  client.setInsecure();

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  // Store measured value into point
  wlan.clearFields();
  // Report RSSI of currently connected network
  wlan.addField("rssi", WiFi.RSSI());
  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(wlan));


  getData();

  sensor.addField("temperatur", temperatureCString);
  sensor.addField("humidity", humid);
  sensor.addField("MQ2String", MQ2String);
  sensor.addField("lpg", lpg);
  sensor.addField("co", co);
  sensor.addField("smoke", smoke);

  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(wlan)) {
    Serial.print("InfluxDB wlan point write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB sensor point write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  //Wait 1s
  Serial.println("Wait 1s");
  delay(1000);
}
