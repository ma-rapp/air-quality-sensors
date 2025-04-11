#include <WiFi.h>

// https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino 3.13.2
#include <InfluxDbClient.h>

// https://github.com/adafruit/DHT-sensor-library 1.4.6
#include "DHT.h"

const char* ssid = "SSID";  // replace with your network credentials
const char* password = "password";  // replace with your network credentials

const char* influxdb_url = "http://1.2.3.4:8086";  // replace with your InfluxDB server address
const char* influxdb_token = "TOKEN";  // replace with your InfluxDB token
const char* influxdb_org = "home";
const char* influxdb_bucket = "appartment";
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

InfluxDBClient client(influxdb_url, influxdb_org, influxdb_bucket, influxdb_token);

Point sensor("airquality");
int sensorid = 0;

// pin for pwm reading
#define CO2_PWM_IN 5
#define CO2_CALIB 6
const long CO2_RANGE = 5000;

#define DHTPIN 2
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

#define SENSORID_PIN1 9
#define SENSORID_PIN2 8
#define SENSORID_PIN3 7

const long updatePeriod = 60 * 1000;
unsigned long nextUpdate;
unsigned long lastSuccessfulUpdate;
const unsigned long unsuccessfulUpdateRestartTimeout = 15 * updatePeriod;

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(CO2_PWM_IN, INPUT);
  pinMode(SENSORID_PIN1, INPUT_PULLUP);
  pinMode(SENSORID_PIN2, INPUT_PULLUP);
  pinMode(SENSORID_PIN3, INPUT_PULLUP);

  Serial.begin(115200);
  delay(1000);

  sensorid = get_sensor_id();
  Serial.print("Sensor ID=");
  Serial.println(sensorid);

  if (sensorid == 7) {
    calibrateCO2();
  }

  char sensorid_str[2] = { '0' + sensorid, 0 };
  sensor.addTag("sensorid", sensorid_str);

  digitalWrite(LED_RED, LOW);
  connectWIFI();
  digitalWrite(LED_RED, HIGH);

  digitalWrite(LED_BLUE, LOW);
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  digitalWrite(LED_BLUE, HIGH);

  Serial.print("Wait for sensor startup");
  while (millis() < 60 * 1000) {
    Serial.print(".");
    // wait for CO2 sensor to get ready
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, LOW);
    delay(10);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    delay(1990);
  }
  Serial.println("");

  dht.begin();

  getCO2Median(5);  // burn one measurement
  nextUpdate = millis() + updatePeriod;

  Serial.println("initialization finished");
}

void calibrateCO2() {
  Serial.println("Calibrating zero point of CO2 sensor");
  Serial.println("Sleeping for 30 minutes");
  unsigned long start = millis();
  unsigned long end = millis() + 30 * 60 * 1000;
  while (millis() < end) {
    unsigned long remaining_minutes = (end - millis()) / 1000 / 60 + 1;
    Serial.print("Remaining: ");
    Serial.print(remaining_minutes);
    Serial.println(" min");
    for (int pulse = 0; pulse < remaining_minutes; pulse++) {
      digitalWrite(LED_BLUE, LOW);
      delay(100);
      digitalWrite(LED_BLUE, HIGH);
      delay(200);
    }
    delay(10 * 1000);
  }
  pinMode(CO2_CALIB, OUTPUT);
  digitalWrite(CO2_CALIB, LOW);
  delay(8000);
  digitalWrite(CO2_CALIB, HIGH);
  pinMode(CO2_CALIB, INPUT);
  while (true) {
    digitalWrite(LED_GREEN, LOW);
    delay(500);
    digitalWrite(LED_GREEN, HIGH);
    delay(500);
  }
}

int get_sensor_id() {
  return (digitalRead(SENSORID_PIN1) << 0) | (digitalRead(SENSORID_PIN2) << 1) | (digitalRead(SENSORID_PIN3) << 2);
}

void connectWIFI() {
  Serial.println();
  Serial.println("******************************************************");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

bool waitForCO2PWMLevel(int level, int timeoutMs) {
  int timeout = millis() + timeoutMs;
  while (digitalRead(CO2_PWM_IN) != level) {
    if (millis() > timeout) {
      return false;
    }
  }
  return true;
}

int getCO2(int timeoutMs = 2000) {
  unsigned long start = 0;
  unsigned long end = 0;
  while (end <= start) {
    if (!waitForCO2PWMLevel(LOW, timeoutMs)) return -1;   // wait for low...
    if (!waitForCO2PWMLevel(HIGH, timeoutMs)) return -1;  // ..to high transition..
    start = micros();
    if (!waitForCO2PWMLevel(LOW, timeoutMs)) return -1;  // ..and measure time until low
    end = micros();
  }
  unsigned long TH = end - start;
  return CO2_RANGE * (TH - 2000) / 1000000;
}

int getValidCO2() {
  while (true) {
    int co2 = getCO2();
    if ((co2 > 300) && (co2 < CO2_RANGE)) {
      return co2;
    }
  }
}

int sort_asc(const void* cmp1, const void* cmp2) {
  int a = *((int*)cmp1);
  int b = *((int*)cmp2);
  return a - b;
}

int getCO2Median(int tries) {
  int measurements[tries];
  for (int i = 0; i < tries; i++) {
    measurements[i] = getValidCO2();
  }
  qsort(measurements, tries, sizeof(measurements[0]), sort_asc);
  return measurements[(tries - 1) / 2];
}

void waitUntilNextUpdate() {
  Serial.print("Wait until next update (now is ");
  Serial.print(millis());
  Serial.print(", next update at ");
  Serial.print(nextUpdate);
  Serial.print(")");
  if (nextUpdate < updatePeriod) {
    // handle millis overflow
    while (millis() > nextUpdate) { sleep(100); }
  }
  while (millis() < nextUpdate) {
    Serial.print(".");
    unsigned long wait = nextUpdate - millis();
    if (wait > 2000) {
      wait = 2000;
    }
    if (wait > 5) {
      digitalWrite(LED_GREEN, LOW);
      delay(5);
      digitalWrite(LED_GREEN, HIGH);
      delay(wait - 5);
    } else {
      delay(wait);
    }
  }
  Serial.println("");
}

void checkForRestartTimeout() {
  unsigned long now = millis();
  if ((now > lastSuccessfulUpdate) && (now - lastSuccessfulUpdate > unsuccessfulUpdateRestartTimeout)) {
    ESP.restart();
  }
}

void loop() {
  waitUntilNextUpdate();
  Serial.println("Collecting sensor data");
  nextUpdate += updatePeriod;

  sensor.clearFields();

  int co2 = getCO2Median(5);
  sensor.addField("co2", co2);

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  sensor.addField("temperature", temp);
  sensor.addField("humidity", hum);

  sensor.addField("pm2_5", -1);
  sensor.addField("pm5", -1);
  sensor.addField("pm10", -1);

  bool success = true;
  if ((sensorid == 0) || (sensorid == 7)) {
    Serial.print("NOT writing: ");
    Serial.println(sensor.toLineProtocol());
  } else {

    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());

    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
      success = false;
    }
  }

  if (success) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    delay(5);
    digitalWrite(LED_GREEN, HIGH);
    delay(200);
    digitalWrite(LED_GREEN, LOW);
    delay(5);
    digitalWrite(LED_GREEN, HIGH);

    lastSuccessfulUpdate = millis();
  } else {
    digitalWrite(LED_RED, LOW);
    checkForRestartTimeout();
  }
}
