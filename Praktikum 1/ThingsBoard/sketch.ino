#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

#define DHTPIN 15

DHTesp dht;

// WiFi Wokwi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ThingsBoard
const char* mqtt_server = "eu.thingsboard.cloud";
const char* accessToken = "YOUR TOKEN";

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");

    if (client.connect("ESP32", accessToken, NULL)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  dht.setup(DHTPIN, DHTesp::DHT22);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  long now = millis();

  if (now - lastMsg > 2000) {
    lastMsg = now;

    TempAndHumidity data = dht.getTempAndHumidity();

    float temperature = data.temperature;
    float humidity = data.humidity;

    Serial.print("Temp: ");
    Serial.println(temperature);

    Serial.print("Humidity: ");
    Serial.println(humidity);

    // kirim ke ThingsBoard
    String payload = "{";
    payload += "\"temperature\":";
    payload += temperature;
    payload += ",";
    payload += "\"humidity\":";
    payload += humidity;
    payload += "}";

    client.publish("v1/devices/me/telemetry", payload.c_str());

  }
}