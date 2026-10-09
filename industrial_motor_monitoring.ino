```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Wi-Fi settings
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// MQTT broker
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// MQTT topics
const char* TOPIC_TEMPERATURE = "industrial/motor/temperature";
const char* TOPIC_VIBRATION   = "industrial/motor/vibration";
const char* TOPIC_STATUS      = "industrial/motor/status";
const char* TOPIC_CONTROL     = "industrial/motor/control";
const char* TOPIC_SPEED       = "industrial/motor/speed";

// ESP32 pins
#define TEMP_PIN 4
#define VIBRATION_PIN 27
#define STEP_PIN 23
#define DIR_PIN 18
#define ENABLE_PIN 19

WiFiClient espClient;
PubSubClient mqttClient(espClient);

OneWire oneWire(TEMP_PIN);
DallasTemperature temperatureSensor(&oneWire);

bool motorRunning = true;
int motorSpeed = 60;

unsigned long lastTemperatureTime = 0;
unsigned long lastMotorStep = 0;

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT Message: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(message);

  if (String(topic) == TOPIC_CONTROL) {
    if (message == "START") {
      motorRunning = true;
      digitalWrite(ENABLE_PIN, LOW);
      mqttClient.publish(TOPIC_STATUS, "RUNNING");
      Serial.println("Motor STARTED");
    } else if (message == "STOP") {
      motorRunning = false;
      digitalWrite(ENABLE_PIN, HIGH);
      mqttClient.publish(TOPIC_STATUS, "STOPPED");
      Serial.println("Motor STOPPED");
    }
  }

  if (String(topic) == TOPIC_SPEED) {
    motorSpeed = constrain(message.toInt(), 1, 100);
    Serial.print("Motor Speed: ");
    Serial.print(motorSpeed);
    Serial.println("%");
  }
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");

    String clientID = "WokwiMotorESP32-" +
                      String(random(0xffff), HEX);

    if (mqttClient.connect(clientID.c_str())) {
      Serial.println("Connected!");

      mqttClient.subscribe(TOPIC_CONTROL);
      mqttClient.subscribe(TOPIC_SPEED);

      mqttClient.publish(
        TOPIC_STATUS,
        motorRunning ? "RUNNING" : "STOPPED"
      );
    } else {
      Serial.print("Failed, state=");
      Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  temperatureSensor.begin();

  pinMode(VIBRATION_PIN, INPUT_PULLUP);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  digitalWrite(ENABLE_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);

  connectWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  Serial.println("Industrial Motor Monitoring Started");
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  // Read and publish temperature every 2 seconds
  if (millis() - lastTemperatureTime >= 2000) {
    lastTemperatureTime = millis();

    temperatureSensor.requestTemperatures();
    float temperature = temperatureSensor.getTempCByIndex(0);

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    char temperatureString[10];
    dtostrf(temperature, 1, 2, temperatureString);

    mqttClient.publish(
      TOPIC_TEMPERATURE,
      temperatureString
    );
  }

  // Simulate vibration using the pushbutton
  bool vibrationDetected =
      digitalRead(VIBRATION_PIN) == LOW;

  mqttClient.publish(
    TOPIC_VIBRATION,
    vibrationDetected ? "VIBRATION DETECTED" : "NORMAL"
  );

  // Control the stepper motor
  if (motorRunning) {
    digitalWrite(ENABLE_PIN, LOW);
    digitalWrite(DIR_PIN, HIGH);

    int stepDelay = map(motorSpeed, 1, 100, 2000, 200);

    if (micros() - lastMotorStep >=
        (unsigned long)stepDelay) {
      lastMotorStep = micros();

      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(500);
      digitalWrite(STEP_PIN, LOW);
    }
  } else {
    digitalWrite(ENABLE_PIN, HIGH);
  }

  delay(500);
}
```