#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SHT3x.h>

const char* ssid = "SSID";
const char* password = "WLAN_PW";
unsigned long delayTime;
char temperatureCString[6];
char humidityString[6];
char MQ2String[3];
SHT3x sht30(0x44); //adress of SHT30
const int analogInPin = A0;  //ADC-pin of AZ-Envy for the gas sensor

WiFiServer server(80);

void setup() {
  Serial.begin(9600);
  delay(10);
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());

  pinMode(analogInPin,INPUT); //set ADC-pin as input
  pinMode(LED_BUILTIN,OUTPUT); //set the integrated led as output
  sht30.Begin();
}
void getData() {
  sht30.UpdateData();
  
  float temperature = sht30.GetTemperature(); //read the temperature from SHT30
  float humid = sht30.GetRelHumidity(); //read the humidity from SHT30
  int sensorValue = analogRead(analogInPin); //read the ADC-pin → connected to MQ-2

  //calibrate your temperature values - due to heat reasons from the MQ-2 (up to 4°C)
  float temperature_deviation = 0.5; //enter the deviation in order to calibrate the temperature value 
  float temperature_calibrated = temperature - temperature_deviation; //final value

  sprintf(MQ2String, "%d",sensorValue);        
  dtostrf(temperature_calibrated, 5, 1, temperatureCString);
  dtostrf(humid, 5, 1, humidityString);
/*
  Serial.print("Temperature: ");
  Serial.println(temperature_calibrated);
  Serial.println(temperatureCString);
  Serial.print("Rel.Humidity: ");            
  Serial.println(humid);    
  Serial.println(humidityString);
  Serial.print("MQ-2 Sensor = ");            
  Serial.println(sensorValue);    
  Serial.println(MQ2String);
  delay(100);
  */
 }
 
void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("New client");
    // bolean to locate when the http request ends
    boolean blank_line = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        if (c == '\n' && blank_line) {
            getData();

            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            // your actual web page that displays temperature
            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            client.println("<head><META HTTP-EQUIV=\"refresh\" CONTENT=\"15\"></head>");
            client.println("<body><h1>AZ-Envy Web Server</h1>");
            client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
            client.println("<h3>Temperatur = ");
            client.println(temperatureCString);
            client.println("&deg;C</h3><h3>Luftfeuchte = ");
            client.println(humidityString);
            client.println("%</h3>");
            client.println("<h3>MQ-2 Sensor = ");
            client.println(MQ2String);
            client.println("");
            client.println("</h3></td></tr></tbody></table></body></html>");  
            break;
        }
        if (c == '\n') {
          // when starts reading a new line
          blank_line = true;
        }
        else if (c != '\r') {
          // when finds a character on the current line
          blank_line = false;
        }
      }
    }  
    // closing the client connection
    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
  }
} 
