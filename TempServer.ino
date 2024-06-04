#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>

#define DHTPIN D4      // Pino onde o sensor está conectado
#define DHTTYPE DHT11  // Tipo de sensor

#define LED_YELLOW D1  // LED Amarelo no pino D1 (GPIO5)
#define LED_GREEN D2   // LED Verde no pino D2 (GPIO4)
#define LED_RED D5     // LED Vermelho no pino D3 (GPIO0)
#define BUTTON_PIN D3  // Pino do botão FLASH

DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

String wifiSSID = "";
String wifiPass = "";
String endpoint = "";
float minTemp = 25.0;
bool resetConfig = false;

void setup() {
  Serial.begin(9600);
  dht.begin();
  
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Usando pull-up interno

  EEPROM.begin(512);  // Inicializar o EEPROM

  loadConfig();

  if (wifiSSID.length() > 0 && !resetConfig) {
    connectToWiFi();
    digitalWrite(LED_YELLOW, LOW);  // Desliga o LED Amarelo
  } else {
    WiFi.softAP("ESP8266_Config");
    Serial.println("Iniciando ponto de acesso para configuração");
    digitalWrite(LED_YELLOW, HIGH);  // Liga o LED Amarelo
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
  }
  
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  // Verificar se o botão de reset foi pressionado
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Botão de reset pressionado. Limpando configurações...");
    clearEEPROM();

    ESP.reset();
  }

  server.handleClient();
  
  if (WiFi.status() == WL_CONNECTED && wifiSSID.length() > 0 && endpoint.length() > 0) {
    float temperature = dht.readTemperature();
  
    if (isnan(temperature)) {
      Serial.println("Falha ao ler do sensor DHT!");
      return;
    }

    Serial.print("Temperatura: ");
    Serial.print(temperature);
    Serial.println(" *C");

    if (temperature < minTemp) {
      digitalWrite(LED_GREEN, LOW);  // Desliga o LED Verde
      digitalWrite(LED_RED, HIGH);   // Liga o LED Vermelho
      Serial.println("Temperatura abaixo do mínimo.");
      if (sendToServer(temperature)) {
        Serial.println("Dados enviados com sucesso");
      } else {
        Serial.println("Falha ao enviar dados");
      }
    } else {
      digitalWrite(LED_RED, LOW);   // Desliga o LED Vermelho
      digitalWrite(LED_GREEN, HIGH); // Liga o LED Verde
    }

    Serial.println("Aguardando 10 segundos até a próxima leitura");
    delay(10000); // Aguarde 2 segundos antes de fazer a próxima leitura
  } else {
    digitalWrite(LED_YELLOW, HIGH);  // Liga o LED Amarelo se não estiver conectado
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
  html += "<title>Configuração do ESP8266</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; }";
  html += ".container { width: 100%; max-width: 600px; margin: 50px auto; padding: 20px; background-color: #fff; box-shadow: 0 0 10px rgba(0,0,0,0.1); border-radius: 8px; }";
  html += "h1 { text-align: center; color: #333; }";
  html += "form { display: flex; flex-direction: column; }";
  html += "label { margin: 10px 0 5px; color: #555; }";
  html += "input[type='text'], input[type='password'] { padding: 10px; border: 1px solid #ddd; border-radius: 4px; }";
  html += "input[type='submit'] { padding: 10px 20px; background-color: #28a745; border: none; border-radius: 4px; color: #fff; font-size: 16px; cursor: pointer; margin-top: 20px; }";
  html += "input[type='submit']:hover { background-color: #218838; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class=\"container\">";
  html += "<h1>Configuração do ESP8266</h1>";
  html += "<form action=\"/save\" method=\"POST\">";
  html += "<label for=\"ssid\">SSID Wi-Fi:</label>";
  html += "<input type=\"text\" id=\"ssid\" name=\"ssid\"><br>";
  html += "<label for=\"password\">Senha Wi-Fi:</label>";
  html += "<input type=\"password\" id=\"password\" name=\"password\"><br>";
  html += "<label for=\"endpoint\">Endpoint:</label>";
  html += "<input type=\"text\" id=\"endpoint\" name=\"endpoint\"><br>";
  html += "<label for=\"mintemp\">Temperatura Mínima:</label>";
  html += "<input type=\"text\" id=\"mintemp\" name=\"mintemp\"><br>";
  html += "<input type=\"submit\" value=\"Salvar\">";
  html += "</form>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  wifiSSID = server.arg("ssid");
  wifiPass = server.arg("password");
  endpoint = server.arg("endpoint");
  minTemp = server.arg("mintemp").toFloat();

  Serial.println("Salvando configurações...");
  Serial.println("SSID: " + wifiSSID);  
  Serial.println("Password: " + wifiPass);
  Serial.println("Endpoint: " + endpoint);
  Serial.println("MinTemp: " + String(minTemp));

  saveConfig();

  String message = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Configuração Salva</title></head><body>";
  message += "<h1>Configurações salvas com sucesso!</h1>";
  message += "<p>O dispositivo irá reiniciar para aplicar as novas configurações.</p>";
  message += "</body></html>";
  
  server.send(200, "text/html", message);
  
  delay(2000); // Pequeno atraso para garantir que a resposta seja enviada ao cliente
  ESP.restart(); // Reinicia o dispositivo
}

void connectToWiFi() {
  Serial.print("Conectando a ");
  Serial.println(wifiSSID);
  
  WiFi.begin(wifiSSID, wifiPass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 40) { // Timeout após 20 segundos
      Serial.println("Falha ao conectar ao Wi-Fi. Iniciando ponto de acesso para configuração.");
      WiFi.softAP("ESP8266_Config");
      digitalWrite(LED_YELLOW, HIGH);  // Liga o LED Amarelo se não conseguir conectar
      return;
    }
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

bool sendToServer(float temperature) {
  BearSSL::WiFiClientSecure client;
  client.setInsecure(); // Não valida o certificado, útil para testes

  HTTPClient https;

  if (endpoint.length() == 0) {
    Serial.println("Endpoint vazio, não é possível conectar.");
    return false;
  }

  Serial.println(endpoint);
  String url = endpoint + "?temp=" + String(temperature, 2);
  Serial.print("Enviando dados. URL: ");
  Serial.println(url);

  https.begin(client, url);

  int httpCode = https.GET();
  if (httpCode > 0) {
    Serial.printf("Código de resposta: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      https.end();
      return true;
    }
  } else {
    Serial.printf("Erro ao enviar requisição: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();
  return false;
}

void clearEEPROM() {
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("Configurações apagadas da EEPROM.");
}

void writeStringToEEPROM(int addrOffset, const String &strToWrite) {
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len); // Salva o comprimento da string
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit(); // Commit após escrever
}

String readStringFromEEPROM(int addrOffset) {
  int len = EEPROM.read(addrOffset);
  if (len < 0 || len > 100) {  // Verificação de limite
    Serial.println("Erro de leitura do EEPROM, valor de comprimento inválido.");
    return "";
  }
  char data[len + 1];
  for (int i = 0; i < len; i++) {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[len] = '\0';
  return String(data);
}

void saveConfig() {
  writeStringToEEPROM(0, wifiSSID);
  writeStringToEEPROM(100, wifiPass);
  writeStringToEEPROM(200, endpoint);
  EEPROM.put(300, minTemp);
  EEPROM.commit();
}

void loadConfig() {
  wifiSSID = readStringFromEEPROM(0);
  wifiPass = readStringFromEEPROM(100);
  endpoint = readStringFromEEPROM(200);
  EEPROM.get(300, minTemp);

  Serial.println("Configurações carregadas:");
  Serial.println("SSID: " + wifiSSID);
  Serial.println("Password: " + wifiPass);
  Serial.println("Endpoint: " + endpoint);
  Serial.println("MinTemp: " + String(minTemp));
}

