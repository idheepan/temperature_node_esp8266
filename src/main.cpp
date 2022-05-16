#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <SparkFun_SHTC3.h>
#include <string>
// Setting network credentials
const char *ssid = "SM-G98";
const char *password = "mouseHeat";
const char *serverName = "http://192.168.32.231:8000/temp_data";
const unsigned int RETRIES = 10;
const unsigned int READ_INTERVAL = 500;
// BLUE LED
const int BLUE_LED = 2;
const int RED_LED = 0;
const int LED_ON = LOW;
const int LED_OFF = HIGH; // Flipped

// Temperature sensors
const int TCA_ADDR = 0x71;
const int SHTC3_ADDR = 0x70;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClient client;
// Sensor object
SHTC3 shtc3;
// Array of which ports have a sensor attached to them
const uint8_t num_ports = 6;
// Variable to save current epoch time
unsigned long epochTime;

// Function that gets current epoch time
unsigned long getTime()
{
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

void tca_select(uint8_t i)
{
  if (i > 7)
    return;

  Wire.beginTransmission(TCA_ADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}
struct SensorReading {
  float temperature;
  float rh;
};

SensorReading get_reading(uint8_t port)
{
  tca_select(port);
  unsigned int tries_remaining = RETRIES;
  while (shtc3.update() != SHTC3_Status_Nominal && tries_remaining > 0)
  {
    delay(300);
    tries_remaining -= 1;
  }
  if (tries_remaining == 0)
  {
    Serial.println("Retried, failed everytime");
    return SensorReading{(float)-50.0, (float)-50.0};
  }
  // Serial.print("Port: ");
  // Serial.print(port);
  // Serial.print(", temp:  ");
  // Serial.println(shtc3.toDegC());
  // Serial.print(", rh:  ");
  // Serial.println(shtc3.toPercent());
  return SensorReading{shtc3.toDegC(), shtc3.toPercent()};
}

// Initialize as output and turn on both red and blue LEDs
void initLEDs()
{
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, LED_OFF);
  pinMode(BLUE_LED, OUTPUT);
  digitalWrite(BLUE_LED, LED_OFF);
}

void blinkLED(int led, int iterations, unsigned long interval, int end_state)
{
  for (int i = 0; i < iterations; i++)
  {
    digitalWrite(BLUE_LED, HIGH);
    delay(interval);
    digitalWrite(BLUE_LED, LOW);
    delay(interval);
  }
  digitalWrite(BLUE_LED, end_state);
}

// Initialize WiFi
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  blinkLED(BLUE_LED, 3, 500, LED_ON);
}

void initSensors()
{
  // Initialise all sensors and ports
  Wire.begin();
  delay(5);
  for (uint8_t i = 0; i < num_ports; i++)
  {
    // See what's connected
    tca_select(i);
    if (shtc3.begin() == SHTC3_Status_Nominal)
    {
      Serial.printf("SHTC3 connected on STEMMA%d", i);
      Serial.println();
    }
  }
}

struct PostDataPacket
{
  unsigned long epochTime;
  float sensor1temp;
  float sensor2temp;
  float sensor1rh;
  float sensor2rh;
};

void collectData(PostDataPacket &dataPacket)
{
  SensorReading result1 = get_reading(1);
  SensorReading result2 = get_reading(2);
  dataPacket.sensor1temp = result1.temperature;
  dataPacket.sensor2temp = result2.temperature;
  dataPacket.sensor1rh = result1.rh;
  dataPacket.sensor2rh = result2.rh;
}

void sendData(PostDataPacket &dataPacket)
{
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  http.addHeader("Content-Type", "application/json");
  std::string post_pack = " {\"time_acquired\" : " + std::to_string(dataPacket.epochTime) +
                          ", \"sensor1_temp\" : " + std::to_string(dataPacket.sensor1temp) +
                          ", \"sensor2_temp\" : " + std::to_string(dataPacket.sensor2temp) +
                          ", \"sensor1_rh\" : " + std::to_string(dataPacket.sensor1rh) +
                          ", \"sensor2_rh\" :  " + std::to_string(dataPacket.sensor2rh) + "}";
  int httpResponseCode = http.POST(reinterpret_cast<const uint8_t *>(post_pack.c_str()), post_pack.size());
  Serial.print("Response code: ");
  Serial.println(httpResponseCode);
  http.end();
}

void setup()
{
  Serial.begin(115200);
  initLEDs();
  initSensors();
  initWiFi();
  timeClient.begin();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    blinkLED(BLUE_LED, 3, 500, LED_OFF);
    digitalWrite(RED_LED, LED_ON);
    return;
  }
  else if (LED_OFF == digitalRead(BLUE_LED))
  {
    blinkLED(BLUE_LED, 1, 500, LED_ON);
  }
  PostDataPacket dataPacket = {0, 0, 0, 0, 0};
  collectData(dataPacket);
  dataPacket.epochTime = getTime();
  sendData(dataPacket);
  delay(READ_INTERVAL);
}