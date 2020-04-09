#include <Arduino.h>
#include <ArduinoJson.h>
#include "Fs.h"
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

DHT dht;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

struct Config {
  char wifi_ssid[64];
  char wifi_password[64];
  char aws_iot_endpoint[64];
  char thing_name[64];
};

Config config;

void loadConfiguration(Config &config) {
  File config_file = SPIFFS.open("/config.json", "r");
  if (!config_file) {
    Serial.println("Failed to open config file");
  }
  else {
    Serial.println("config file opened");
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, config_file);
  if (error) {
    Serial.println("Failed to read config file");
  }

  strlcpy(config.wifi_ssid, doc["wifi_ssid"], sizeof(config.wifi_ssid));
  strlcpy(config.wifi_password, doc["wifi_password"], sizeof(config.wifi_password));
  strlcpy(config.aws_iot_endpoint, doc["aws_iot_endpoint"], sizeof(config.aws_iot_endpoint));
  strlcpy(config.thing_name, doc["thing_name"], sizeof(config.thing_name));

  config_file.close();
}

void callback(char* topic, byte* payload, int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

WiFiClientSecure espClient;
PubSubClient client(config.aws_iot_endpoint, 8883, callback, espClient);

void setup_wifi() {
  delay(10);

  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(config.wifi_ssid);

  WiFi.begin(config.wifi_ssid, config.wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  while(!timeClient.update()){
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());

  delay(200);

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    Serial.println("Failed to open Cert file");
  }
  else {
    Serial.println("Cert file opened");
  }

  delay(200);

  if (espClient.loadCertificate(cert)) {
    Serial.println("Cert loaded");
  }
  else {
    Serial.println("Cert failed to load");
  }

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) {
    Serial.println("Failed to open private key file");
  }
  else {
    Serial.println("Private key file opened");
  }

  delay(200);

  if (espClient.loadPrivateKey(private_key)) {
    Serial.println("Private key loaded");
  }
  else {
    Serial.println("Private key failed to load");
  }

  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r");
  if (!ca) {
    Serial.println("Failed to open CA file");
  }
  else {
    Serial.println("CA file opened");
  }

  delay(200);

  if (espClient.loadCACert(ca)) {
    Serial.println("CA loaded");
  }
  else {
    Serial.println("CA failed to load");
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(config.thing_name)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      char buf[256];
      espClient.getLastSSLError(buf,256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  dht.setup(5);
  loadConfiguration(config);

  setup_wifi();
  reconnect();
}

long lastMsg = 0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;
    float h = dht.getHumidity();
    float t = dht.getTemperature();

    StaticJsonDocument<200> message;
    StaticJsonDocument<200> state;
    StaticJsonDocument<200> reported;

    if (!isnan(h)) {
      Serial.print("humidity: ");
      Serial.println(h);
      reported["humidity"] = h;
    }

    if (!isnan(t)) {
      Serial.print("temperature: ");
      Serial.println(t);
      reported["temperature"] = t;
    }

    state["reported"] = reported;
    message["state"] = state;

    char jsonBuffer[512];
    serializeJson(message, jsonBuffer);

    String topic = "$aws/things/";
    topic += config.thing_name;
    topic += "/shadow/update";

    char* topicArr = strcpy(new char[topic.length() + 1], topic.c_str());

    client.publish(topicArr, jsonBuffer);

    char prettyJson[512];
    serializeJsonPretty(message, prettyJson);

    Serial.print("Publish message to topic ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(prettyJson);

    Serial.println("-------------");
  }
}