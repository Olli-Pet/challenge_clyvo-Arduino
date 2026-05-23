// =============================================================
// OLLIE PET - Prototipo IoT para monitoramento de pets
// Disciplina: Disruptive Architectures - FIAP
// Plataforma: ESP32 + Wokwi + MQTT
// =============================================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ArduinoJson.h>

// =========================
// CONFIGURACAO DO WIFI
// =========================
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// =========================
// CONFIGURACAO MQTT
// =========================
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "ollie-pet-chip-001";

const char* TOPIC_STATUS = "fiap/iot/2tdsa/olliepet/status";
const char* TOPIC_TELEMETRIA = "fiap/iot/2tdsa/olliepet/telemetria";
const char* TOPIC_TEMPERATURA = "fiap/iot/2tdsa/olliepet/sensor/temperatura";
const char* TOPIC_BATIMENTOS = "fiap/iot/2tdsa/olliepet/sensor/batimentos";
const char* TOPIC_LOCALIZACAO = "fiap/iot/2tdsa/olliepet/sensor/localizacao";
const char* TOPIC_ALERTA = "fiap/iot/2tdsa/olliepet/alerta";

// =========================
// PINOS DO CIRCUITO
// =========================
const int DHT_PIN = 15;
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LED_BATIMENTO = 26;
const int LED_ALERTA = 27;
const int BUZZER = 14;

// =========================
// REGRAS DA SIMULACAO
// =========================
const float TEMP_ALERTA = 39.2;          // temperatura corporal alta para pets
const int DISTANCIA_ALERTA_CM = 180;     // pet distante da base simulada
const unsigned long INTERVALO_MQTT = 3000;
const unsigned long INTERVALO_BATIMENTO = 420;

DHTesp dhtSensor;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long ultimoEnvioMqtt = 0;
unsigned long ultimoPiscaBatimento = 0;
bool estadoLedBatimento = false;

// =========================
// FUNCOES DE CONEXAO
// =========================
void conectarWifi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println(" conectado!");
  Serial.print("IP do ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando ao broker MQTT...");

    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println(" conectado!");
      mqttClient.publish(TOPIC_STATUS, "{\"status\":\"online\",\"device\":\"OLLIE-CHIP-001\"}", true);
    } else {
      Serial.print(" falhou, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" tentando novamente em 2 segundos");
      delay(2000);
    }
  }
}

// =========================
// LEITURA DOS SENSORES
// =========================
float lerTemperatura() {
  TempAndHumidity dados = dhtSensor.getTempAndHumidity();

  if (isnan(dados.temperature)) {
    return 0.0;
  }

  return dados.temperature;
}

int lerDistanciaCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duracao == 0) {
    return 0;
  }

  return duracao / 58;
}

int simularBatimentos(float temperatura) {
  // Pequena variacao para parecer um sinal vivo no dashboard.
  int variacao = random(-6, 7);
  int batimentos = 92 + variacao;

  if (temperatura >= TEMP_ALERTA) {
    batimentos += 18;
  }

  return batimentos;
}

String classificarLocalizacao(int distanciaCm) {
  if (distanciaCm == 0) {
    return "sem leitura";
  }

  if (distanciaCm <= 80) {
    return "perto da base";
  }

  if (distanciaCm <= DISTANCIA_ALERTA_CM) {
    return "em observacao";
  }

  return "fora da zona segura";
}

String classificarSaude(float temperatura, int batimentos, int distanciaCm) {
  if (temperatura >= TEMP_ALERTA || distanciaCm > DISTANCIA_ALERTA_CM || batimentos > 115) {
    return "alerta";
  }

  return "normal";
}

// =========================
// ATUADORES LOCAIS
// =========================
void atualizarBatimentoVisual() {
  if (millis() - ultimoPiscaBatimento >= INTERVALO_BATIMENTO) {
    ultimoPiscaBatimento = millis();
    estadoLedBatimento = !estadoLedBatimento;
    digitalWrite(LED_BATIMENTO, estadoLedBatimento ? HIGH : LOW);
  }
}

void atualizarAlertas(String statusSaude) {
  bool alerta = statusSaude == "alerta";

  digitalWrite(LED_ALERTA, alerta ? HIGH : LOW);

  if (alerta) {
    tone(BUZZER, 900, 180);
  } else {
    noTone(BUZZER);
  }
}

// =========================
// PUBLICACAO MQTT
// =========================
void publicarTelemetria() {
  float temperatura = lerTemperatura();
  int distanciaCm = lerDistanciaCm();
  int batimentos = simularBatimentos(temperatura);
  String localizacao = classificarLocalizacao(distanciaCm);
  String statusSaude = classificarSaude(temperatura, batimentos, distanciaCm);

  atualizarAlertas(statusSaude);

  StaticJsonDocument<384> doc;
  doc["device"] = "OLLIE-CHIP-001";
  doc["pet"] = "Ollie";
  doc["temperatura"] = temperatura;
  doc["batimentos"] = batimentos;
  doc["distancia_cm"] = distanciaCm;
  doc["localizacao"] = localizacao;
  doc["status"] = statusSaude;
  doc["millis"] = millis();

  char payload[384];
  serializeJson(doc, payload);

  mqttClient.publish(TOPIC_TELEMETRIA, payload);

  char valor[32];
  snprintf(valor, sizeof(valor), "%.1f", temperatura);
  mqttClient.publish(TOPIC_TEMPERATURA, valor);

  snprintf(valor, sizeof(valor), "%d", batimentos);
  mqttClient.publish(TOPIC_BATIMENTOS, valor);

  snprintf(valor, sizeof(valor), "%d", distanciaCm);
  mqttClient.publish(TOPIC_LOCALIZACAO, valor);

  mqttClient.publish(TOPIC_ALERTA, statusSaude.c_str());

  Serial.println("Telemetria publicada:");
  Serial.println(payload);
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(34));

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_BATIMENTO, OUTPUT);
  pinMode(LED_ALERTA, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  conectarWifi();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  conectarMqtt();
}

// =========================
// LOOP PRINCIPAL
// =========================
void loop() {
  if (!mqttClient.connected()) {
    conectarMqtt();
  }

  mqttClient.loop();
  atualizarBatimentoVisual();

  if (millis() - ultimoEnvioMqtt >= INTERVALO_MQTT) {
    ultimoEnvioMqtt = millis();
    publicarTelemetria();
  }
}
