#include "secrets.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL 6

#define TRIG_ENTRADA 5
#define ECHO_ENTRADA 18
#define TRIG_SAIDA 19
#define ECHO_SAIDA 23
#define SERVO_PIN 13
#define LED_VERDE 26
#define LED_VERMELHO 27
#define BOTAO_ENTRADA 32
#define BOTAO_SAIDA 33

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo cancela;

unsigned long ultimoEvento = 0;
const unsigned long intervaloEvento = 3000;
long contadorEventos = 0;
String sessaoId;

void mostrarMensagem(String linha1, String linha2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(linha1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(linha2.substring(0, 16));
}

float medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duracao = pulseIn(echo, HIGH, 30000);
  if (duracao == 0) return -1;
  return duracao * 0.0343 / 2.0;
}

void abrirCancela() {
  cancela.attach(SERVO_PIN);
  cancela.write(90);
}

void fecharCancela() {
  cancela.write(0);
  delay(500);
  cancela.detach();
}

void controlarCancela(bool autorizada) {
  if (autorizada) {
    Serial.println("[CANCELA] Autorizado! Acendendo LED VERDE e abrindo o servo.");
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_VERMELHO, LOW);
    abrirCancela();
    mostrarMensagem("Acesso liberado", "Pode prosseguir");
    delay(2000);

    Serial.println("[CANCELA] Fechando o servo e apagando LED VERDE.");
    fecharCancela();
    digitalWrite(LED_VERDE, LOW);
  } else {
    Serial.println("[CANCELA] Negado! Acendendo LED VERMELHO. Servo permanece fechado.");
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);
    mostrarMensagem("Acesso negado", "Verifique dados");
    delay(2000);

    Serial.println("[CANCELA] Apagando LED VERMELHO.");
    digitalWrite(LED_VERMELHO, LOW);
  }
  mostrarMensagem("SmartPark", "Aguardando...");
}

bool requisitarAcesso(String tipo) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] Wi-Fi desconectado no momento da requisição.");
    return false;
  }

  JsonDocument doc;
  contadorEventos++;
  doc["eventId"] = "evt_" + sessaoId + "_" + String(contadorEventos);
  doc["reservationId"] = RESERVATION_ID;
  doc["type"] = tipo;

  String body;
  serializeJson(doc, body);

  Serial.println("\n--- INICIANDO REQUISIÇÃO HTTP ---");
  Serial.print("[HTTP] Payload enviado: ");
  Serial.println(body);

  const int maxTentativas = 3;
  bool autorizada = false;

  for (int tentativa = 1; tentativa <= maxTentativas; tentativa++) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(60);

    HTTPClient http;
    http.setTimeout(60000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (!http.begin(client, BACKEND_URL)) {
      Serial.println("[HTTP] ERRO FATAL: Falha ao iniciar cliente HTTP.");
      return false;
    }

    http.addHeader("User-Agent", "ESP32-Wokwi");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-device-secret", DEVICE_SECRET);

    unsigned long t0 = millis();
    int httpCode = http.POST(body);
    Serial.printf("[HTTP] Tentativa %d/%d: código %d em %lu ms\n", tentativa, maxTentativas, httpCode, millis() - t0);

    if (httpCode > 0) {
      String response = http.getString();
      Serial.print("[HTTP] Corpo da resposta: ");
      Serial.println(response);

      if (httpCode == 200) {
        JsonDocument responseDoc;
        if (!deserializeJson(responseDoc, response)) {
          autorizada = responseDoc["authorized"] | false;
          Serial.print("[JSON] Valor do campo 'authorized': ");
          Serial.println(autorizada ? "TRUE" : "FALSE");
        } else {
          Serial.println("[JSON] Erro ao ler o JSON de resposta.");
        }
      }
      http.end();
      break;
    }

    char tlsErr[128] = {0};
    int e = client.lastError(tlsErr, sizeof(tlsErr));
    
    Serial.printf("[HTTP] ERRO DE REDE: %s | TLS(%d): %s\n",http.errorToString(httpCode).c_str(), e, tlsErr);
    
    http.end();
    delay(1000);
  }

  Serial.println("--- FIM DA REQUISIÇÃO ---\n");
  return autorizada;
}

void processarEvento(String tipo) {
  if (millis() - ultimoEvento < intervaloEvento) return;
  ultimoEvento = millis();

  Serial.print("\n[EVENTO] Iniciando processamento: ");
  Serial.println(tipo);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] Wi-Fi caiu. Bloqueando acesso por segurança.");
    mostrarMensagem("Sem Wi-Fi", "Acesso bloqueado");
    controlarCancela(false);
    return;
  }

  mostrarMensagem("Verificando", "com o servidor...");
  bool autorizada = requisitarAcesso(tipo);
  controlarCancela(autorizada);
  ultimoEvento = millis();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== SMART PARK INICIANDO ===");

  pinMode(TRIG_ENTRADA, OUTPUT);
  pinMode(ECHO_ENTRADA, INPUT);
  pinMode(TRIG_SAIDA, OUTPUT);
  pinMode(ECHO_SAIDA, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BOTAO_ENTRADA, INPUT_PULLUP);
  pinMode(BOTAO_SAIDA, INPUT_PULLUP);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  mostrarMensagem("SmartPark", "Conectando WiFi");
  Serial.print("[WIFI] Conectando...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\n[WIFI] Wi-Fi conectado com sucesso!");
  Serial.print("[WIFI] Endereço IP: ");
  Serial.println(WiFi.localIP());

  sessaoId = String((uint32_t)ESP.getEfuseMac(), HEX) + "_" + String(esp_random(), HEX);

  mostrarMensagem("SmartPark", "Pronto");
  Serial.println("[SISTEMA] Pronto para receber eventos (Botões/Sensores).\n");
}

void loop() {
  float entrada = medirDistancia(TRIG_ENTRADA, ECHO_ENTRADA);
  float saida = medirDistancia(TRIG_SAIDA, ECHO_SAIDA);

  if (entrada > 0 && entrada <= 10) {
    Serial.println("[SENSOR] Carro detectado no ULTRASSÔNICO DE ENTRADA!");
    processarEvento("ENTRY");
    delay(1000);
  }

  if (saida > 0 && saida <= 10) {
    Serial.println("[SENSOR] Carro detectado no ULTRASSÔNICO DE SAÍDA!");
    processarEvento("EXIT");
    delay(1000);
  }

  if (digitalRead(BOTAO_ENTRADA) == LOW) {
    Serial.println("[SENSOR] BOTÃO DE ENTRADA pressionado!");
    processarEvento("ENTRY");
    delay(500);
  }

  if (digitalRead(BOTAO_SAIDA) == LOW) {
    Serial.println("[SENSOR] BOTÃO DE SAÍDA pressionado!");
    processarEvento("EXIT");
    delay(500);
  }

  delay(50);
}