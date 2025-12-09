#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <SSLClient.h>
#include <ArduinoJson.h>
#include "ca.h"   // Certificados raíz
#include "time.h"

// -------- WiFi --------
const char* ssid     = "GL-MT300N-V2-ad2";
const char* password = "goodlife";

// -------- MQTT --------
const char* mqttHost = "192.168.8.1";
const int   mqttPort = 8883;
const char* mqttUser = "sonda1";
const char* mqttPass = "sonda1";

String sondaId = "34";

// Tópicos dinámicos
String topicO2     = "/sonda/" + sondaId + "/o2/datos";
String topicAlarma = "/sonda/" + sondaId + "/o2/alarma";

// -------- Hardware --------
const int pinLuz = 2;
const int pinPot = 39;
int umbral = 1000;

// Estados del sistema
bool alarmaActiva = false;
bool alarmaAnterior = false;

// -------- MQTT + TLS --------
WiFiClient wifiClient;
SSLClient sslClient(wifiClient, TAs, TAs_NUM, 25, 1);
MqttClient mqttClient(sslClient);

// -------- Tiempo --------
unsigned long lastSend = 0;
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;


// ---------------------------------------------------------------------
// -----------------------   CONECTAR WIFI   ----------------------------
// ---------------------------------------------------------------------
void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("[WIFI] Conectando...");
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
    Serial.print(".");
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Conectado!");
  } else {
    Serial.println("\n[WIFI] No conectó (reintento más tarde)");
  }
}


// ---------------------------------------------------------------------
// ---------------------   CONECTAR MQTT TLS   --------------------------
// ---------------------------------------------------------------------
void conectarMQTT() {
  if (mqttClient.connected()) return;

  Serial.println("[MQTT] Conectando TLS...");

  mqttClient.setUsernamePassword(mqttUser, mqttPass);

  if (!mqttClient.connect(mqttHost, mqttPort)) {
    Serial.print("[MQTT] Error: ");
    Serial.println(mqttClient.connectError());
    return; // No bloquea → reintentará en loop()
  }

  Serial.println("[MQTT] Conectado a broker TLS");

  // Resuscribir
  mqttClient.subscribe(topicAlarma);
  Serial.println("[MQTT] Suscrito a: " + topicAlarma);
}


// ---------------------------------------------------------------------
// ---------------------   CALLBACK MQTT   ------------------------------
// ---------------------------------------------------------------------
void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  String body = "";

  while (mqttClient.available()) {
    body += (char)mqttClient.read();
  }

  body.trim();                    // elimina espacios / saltos
  body.toLowerCase();             // insensible a mayúsculas

  Serial.println("[MQTT] Recibido:");
  Serial.println("  Topic: " + topic);
  Serial.println("  MSG:   " + body);

  if (topic == topicAlarma) {
    if (body.startsWith("true"))  alarmaActiva = true;
    else if (body.startsWith("false")) alarmaActiva = false;
  }
}


// ---------------------------------------------------------------------
// ----------------------------   SETUP   -------------------------------
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(pinLuz, OUTPUT);
  digitalWrite(pinLuz, LOW);

  conectarWiFi();

  mqttClient.onMessage(onMqttMessage);

  // Sincronización NTP robusta
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  Serial.println("[NTP] Esperando sincronización...");

  while (time(nullptr) < 100000) {  // Tiempo válido > 1970-01-02
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n[NTP] Hora obtenida!");

  conectarMQTT();
}


// ---------------------------------------------------------------------
// ----------------------------   LOOP   --------------------------------
// ---------------------------------------------------------------------
void loop() {
  conectarWiFi();
  conectarMQTT();

  // Necesario para recibir mensajes MQTT
  mqttClient.poll();

  // LED según alarma recibida
  digitalWrite(pinLuz, alarmaActiva ? HIGH : LOW);

  // Publicación cada 2s
  if (millis() - lastSend > 2000) {
    lastSend = millis();

    long ts = time(nullptr);

    if (ts < 100000) {
      Serial.println("[NTP] Tiempo no válido, saltando envío");
      return;
    }

    int o2 = analogRead(pinPot);
    Serial.printf("[INFO] Pot=%d Timestamp=%ld\n", o2, ts);

    // ---------------- JSON con ArduinoJson ----------------
    StaticJsonDocument<128> doc;
    doc["valor"] = o2;
    doc["timestamp"] = ts;

    String json;
    serializeJson(doc, json);

    mqttClient.beginMessage(topicO2);
    mqttClient.print(json);
    mqttClient.endMessage();
    Serial.println("[MQTT] Publicado O2: " + json);

    // --- Lógica de alarma ---
    bool alarma = (o2 > umbral);

    if (alarma != alarmaAnterior) {
      String valor = alarma ? "true" : "false";

      mqttClient.beginMessage(topicAlarma);
      mqttClient.print(valor);
      mqttClient.endMessage();

      Serial.println("[MQTT] Publicada ALARMA: " + valor);

      alarmaAnterior = alarma;
    }
  }
}
