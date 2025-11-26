#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "wifi_manager.h"
#include "api_client.h"

// Constantes
const char* callMeBotAPI = "https://api.callmebot.com/whatsapp.php";

// Objetos globales
WebServer server(80);
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Configuración
DeviceConfig deviceConfig;
ConfigManager configManager;
WiFiManagerCustom wifiManager;
APIClient* apiClient = nullptr;

// Pines
const int relayPin = 4;
const int relayFailPin = 5;
const int panicButtonPin = 12;

// Pines del teclado 4x4
const int ROW_PINS[4] = {13, 14, 27, 26};
const int COL_PINS[4] = {25, 33, 32, 15};

// Mapeo de teclas del teclado
const char KEY_MAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Variables de estado
bool isRegistering = false;
bool waitingForPassword = false;
bool accessGranted = false;
unsigned long accessGrantedTime = 0;
const unsigned long ACCESS_DURATION = 3000; // 3 segundos
int failedAttempts = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long passwordStartTime = 0;
const unsigned long PASSWORD_TIMEOUT = 20000; // 20 segundos
String callMeBotAPIKey = "";

// Variables para sistema de contraseña
String currentPassword = "";
int currentUserID = 0;
String currentUserName = "";

// Declaraciones forward de funciones
void handleGetConfig();
void handleReconfig();
void handleResetWiFi();
void handleEnrollFingerprint();
void handleDeleteFingerprint();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Inicializar LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  
  // Inicializar sensor de huellas
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  
  if (finger.verifyPassword()) {
    Serial.println("Sensor de huellas encontrado!");
    lcd.setCursor(0, 1);
    lcd.print("Sensor OK");
  } else {
    Serial.println("No se pudo encontrar el sensor de huellas :(");
    lcd.setCursor(0, 1);
    lcd.print("Sensor ERROR");
    while (1);
  }
  
  delay(1000);
  
  // Inicializar configuración
  configManager.begin();
  
  // Cargar configuración
  if (!configManager.loadConfig(deviceConfig)) {
    // No hay configuración, iniciar modo de configuración
    Serial.println("No hay configuración guardada");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Modo Config");
    iniciarModoConfiguracion();
  }
  
  // Conectar WiFi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");
  
  if (!wifiManager.connect()) {
    Serial.println("No se pudo conectar con credenciales guardadas");
    iniciarModoConfiguracion();
  }
  
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  
  delay(2000);
  
  // Registrar dispositivo si no está registrado
  if (strlen(deviceConfig.deviceToken) == 0) {
    Serial.println("Dispositivo no registrado, debe registrarse manualmente");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No registrado");
    // El dispositivo debe registrarse a través del portal web o API
  } else {
    // Inicializar cliente API
    apiClient = new APIClient(&deviceConfig);
    
    // Obtener API Key
    callMeBotAPIKey = getAPIKeyFromDB();
    if (callMeBotAPIKey.length() == 0) {
      Serial.println("Advertencia: No se pudo obtener el API Key");
    } else {
      Serial.println("API Key obtenido");
    }
  }
  
  // Configurar servidor web local
  server.on("/registrarHuella", HTTP_GET, handleEnrollFingerprint);
  server.on("/eliminarHuella", HTTP_POST, handleDeleteFingerprint);
  server.on("/config", HTTP_GET, handleGetConfig);
  server.on("/reconfig", HTTP_POST, handleReconfig);
  server.on("/resetwifi", HTTP_GET, handleResetWiFi);
  server.begin();
  
  // Configurar pines
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  pinMode(relayFailPin, OUTPUT);
  digitalWrite(relayFailPin, LOW);
  pinMode(panicButtonPin, INPUT_PULLUP);
  
  // Configurar pines del teclado
  for (int i = 0; i < 4; i++) {
    pinMode(ROW_PINS[i], OUTPUT);
    digitalWrite(ROW_PINS[i], HIGH);
    pinMode(COL_PINS[i], INPUT_PULLUP);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Esperando user");
  
  Serial.println("Sistema listo!");
}

void loop() {
  server.handleClient();
  wifiManager.reconnect();
  
  // Si el acceso fue concedido recientemente, ignorar otras entradas
  if (accessGranted && (millis() - accessGrantedTime < ACCESS_DURATION)) {
    return; // No procesar huellas o teclado durante este tiempo
  }
  
  // Después del período de acceso concedido, esperar a que el dedo se retire
  if (accessGranted) {
    // Esperar a que el dedo se retire del sensor antes de continuar
    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      delay(100);
    }
    accessGranted = false;
    // Pequeño delay adicional para estabilizar
    delay(500);
  }
  
  if (!isRegistering) {
    if (!waitingForPassword) {
      verifyFingerprint();
    } else {
      handlePasswordInput();
    }
    delay(300);
  }
  
  // Botón de pánico
  int reading = digitalRead(panicButtonPin);
  if (reading == LOW) {
    unsigned long currentTime = millis();
    if ((currentTime - lastDebounceTime) > debounceDelay) {
      lastDebounceTime = currentTime;
      Serial.println("¡Botón de pánico presionado!");
      activarAlarma();
    }
  }
}

// Modo de configuración inicial
void iniciarModoConfiguracion() {
  // Generar nombre del AP con últimos 4 dígitos de MAC
  String mac = WiFi.macAddress();
  String apName = "SistemaAcceso-" + mac.substring(mac.length() - 5);
  apName.replace(":", "");
  
  Serial.print("Iniciando AP: ");
  Serial.println(apName);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AP: " + apName);
  lcd.setCursor(0, 1);
  lcd.print("192.168.4.1");
  
  // Desconectar WiFi si está conectado
  WiFi.disconnect(true);
  delay(100);
  
  // Configurar modo AP
  WiFi.mode(WIFI_AP);
  delay(100);
  
  // Crear el AP manualmente (sin WiFiManager para evitar conflictos)
  bool apStarted = WiFi.softAP(apName.c_str(), ""); // Sin contraseña
  
  if (!apStarted) {
    Serial.println("ERROR: No se pudo iniciar el AP");
    lcd.clear();
    lcd.print("Error AP");
    delay(3000);
    ESP.restart();
    return;
  }
  
  // Configurar IP del AP
  IPAddress IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(IP, gateway, subnet);
  
  delay(500);
  
  Serial.print("AP iniciado correctamente. IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("SSID: ");
  Serial.println(apName);
  Serial.println("Esperando conexión al AP...");
  
  // Crear servidor de configuración
  WebServer configServer(80);
  
  // Página principal con formulario para WiFi Y registro de dispositivo
  configServer.on("/", HTTP_GET, [&configServer, apName]() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Configuración ESP32</title>";
    html += "<style>body{font-family:Arial,sans-serif;padding:20px;background:#f0f0f0;}";
    html += ".container{max-width:500px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#333;text-align:center;margin-bottom:20px;}";
    html += "label{display:block;margin-top:15px;color:#555;font-weight:bold;}";
    html += "input,select{width:100%;padding:10px;margin-top:5px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:14px;}";
    html += "button{width:100%;padding:12px;margin-top:20px;background:#4CAF50;color:white;border:none;border-radius:5px;font-size:16px;cursor:pointer;font-weight:bold;}";
    html += "button:hover{background:#45a049;}";
    html += "#result{margin-top:20px;padding:10px;border-radius:5px;}</style></head><body>";
    html += "<div class='container'><h1>⚙️ Configuración ESP32</h1>";
    html += "<form id='configForm'>";
    html += "<label>Red WiFi:</label>";
    html += "<input type='text' name='ssid' placeholder='Nombre de la red WiFi' required>";
    html += "<label>Contraseña WiFi:</label>";
    html += "<input type='password' name='password' placeholder='Contraseña de la red WiFi' required>";
    html += "<label>URL del Servidor:</label>";
    html += "<input type='text' name='serverUrl' placeholder='http://192.168.1.100 o https://api.midominio.com' required>";
    html += "<label>Usar HTTPS:</label>";
    html += "<select name='useHTTPS'><option value='0'>No (HTTP)</option><option value='1'>Sí (HTTPS)</option></select>";
    html += "<label>Código de Registro:</label>";
    html += "<input type='text' name='codigo' placeholder='Código del administrador' required>";
    html += "<label>Nombre del Dispositivo:</label>";
    html += "<input type='text' name='nombre' placeholder='Ej: Puerta Principal' required>";
    html += "<button type='submit'>Configurar Dispositivo</button>";
    html += "</form><div id='result'></div></div>";
    html += "<script>";
    html += "document.getElementById('configForm').addEventListener('submit', async (e) => {";
    html += "e.preventDefault();";
    html += "const formData = new FormData(e.target);";
    html += "const data = Object.fromEntries(formData);";
    html += "const response = await fetch('/config', {";
    html += "method: 'POST',";
    html += "headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "body: new URLSearchParams(data)";
    html += "});";
    html += "const result = await response.json();";
    html += "const resultDiv = document.getElementById('result');";
    html += "if (result.success) {";
    html += "resultDiv.innerHTML = '<div style=\"color: green; margin-top: 20px; padding: 10px; background: #e8f5e9; border-radius: 5px;\">' + result.message + '</div>';";
    html += "setTimeout(() => { resultDiv.innerHTML = '<div style=\"color: blue; margin-top: 20px; padding: 10px; background: #e3f2fd; border-radius: 5px;\">Reiniciando dispositivo...</div>'; }, 2000);";
    html += "} else {";
    html += "resultDiv.innerHTML = '<div style=\"color: red; margin-top: 20px; padding: 10px; background: #ffebee; border-radius: 5px;\">Error: ' + (result.error || 'Desconocido') + '</div>';";
    html += "}});</script></body></html>";
    configServer.send(200, "text/html; charset=utf-8", html);
  });
  
  // Endpoint para guardar configuración
  configServer.on("/config", HTTP_POST, [&configServer]() {
    String ssid = configServer.arg("ssid");
    String password = configServer.arg("password");
    String serverUrl = configServer.arg("serverUrl");
    String codigo = configServer.arg("codigo");
    String nombre = configServer.arg("nombre");
    bool useHTTPS = configServer.arg("useHTTPS") == "1";
    
    Serial.println("Recibiendo configuración...");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Server URL: ");
    Serial.println(serverUrl);
    
    // Guardar credenciales WiFi
    Preferences preferences;
    preferences.begin("wifi_config", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    
    // Guardar configuración del servidor (sin registrar todavía)
    strncpy(deviceConfig.serverUrl, serverUrl.c_str(), sizeof(deviceConfig.serverUrl) - 1);
    deviceConfig.serverUrl[sizeof(deviceConfig.serverUrl) - 1] = '\0';
    
    int port = useHTTPS ? 443 : 80;
    if (serverUrl.indexOf(":") > 0) {
      int portStart = serverUrl.lastIndexOf(":");
      if (portStart > 0) {
        String portStr = serverUrl.substring(portStart + 1);
        port = portStr.toInt();
      }
    }
    deviceConfig.serverPort = port;
    deviceConfig.useHTTPS = useHTTPS;
    deviceConfig.configured = false; // Se configurará después de conectar al WiFi
    
    // Guardar configuración temporal (sin token todavía)
    configManager.saveConfig(deviceConfig);
    
    Serial.println("Credenciales WiFi y configuración guardadas");
    Serial.println("Reiniciando para conectar al WiFi...");
    
    configServer.send(200, "application/json", "{\"success\": true, \"message\": \"Configuración guardada. Conectando al WiFi y registrando dispositivo...\"}");
    delay(2000);
    
    // Cerrar servidor antes de cambiar modo WiFi
    configServer.stop();
    delay(500);
    
    // Cambiar a modo Station para conectar al WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    Serial.print("Conectando a WiFi: ");
    Serial.println(ssid);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConectado al WiFi!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      
      // Ahora intentar registrar el dispositivo
      Serial.println("Intentando registrar dispositivo en el servidor...");
      if (registrarDispositivo(serverUrl, codigo, nombre, useHTTPS)) {
        Serial.println("Dispositivo registrado exitosamente!");
        ESP.restart();
      } else {
        Serial.println("Error al registrar dispositivo, pero WiFi configurado. Reiniciando...");
        ESP.restart();
      }
    } else {
      Serial.println("\nError: No se pudo conectar al WiFi");
      Serial.println("Reiniciando para intentar nuevamente...");
      ESP.restart();
    }
  });
  
  configServer.begin();
  
  Serial.println("Servidor de configuración iniciado en 192.168.4.1");
  Serial.println("Conecta tu dispositivo al AP y abre http://192.168.4.1");
  
  // Loop infinito esperando configuración
  while (true) {
    configServer.handleClient();
    delay(100);
  }
}

// Registrar dispositivo en el servidor
bool registrarDispositivo(String serverUrl, String codigo, String nombre, bool useHTTPS) {
  // Determinar host y puerto
  String host = extractHost(serverUrl);
  int port = useHTTPS ? 443 : 80; // Puerto por defecto según protocolo
  
  // Buscar puerto explícito en la URL (solo si hay :puerto después del host)
  // Buscar después del :// para evitar confundir con el : del protocolo
  int protocolPos = serverUrl.indexOf("://");
  if (protocolPos >= 0) {
    int afterProtocol = protocolPos + 3; // Después de "://"
    int colonPos = serverUrl.indexOf(":", afterProtocol); // Buscar : después del protocolo
    int slashPos = serverUrl.indexOf("/", afterProtocol); // Buscar / después del protocolo
    
    // Si hay un : después del protocolo y antes de la ruta, es el puerto
    if (colonPos > 0 && (slashPos < 0 || colonPos < slashPos)) {
      int portEnd = (slashPos > 0) ? slashPos : serverUrl.length();
      String portStr = serverUrl.substring(colonPos + 1, portEnd);
      int extractedPort = portStr.toInt();
      if (extractedPort > 0 && extractedPort < 65536) {
        port = extractedPort;
      }
    }
  }
  
  String path = "/api/esp32/register";
  
  // Crear JSON
  DynamicJsonDocument doc(512);
  doc["nombre"] = nombre;
  doc["codigo"] = codigo;
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  Serial.print("Conectando a: ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);
  Serial.print("HTTPS: ");
  Serial.println(useHTTPS ? "Sí" : "No");
  
  if (useHTTPS) {
    // Usar WiFiClientSecure para HTTPS
    WiFiClientSecure client;
    
    // Para Render.com y otros servicios, necesitamos desactivar la verificación del certificado
    // (solo para desarrollo, en producción deberías usar certificados válidos)
    client.setInsecure(); // Desactiva verificación de certificado
    
    if (!client.connect(host.c_str(), port)) {
      Serial.println("Error de conexión HTTPS al servidor");
      return false;
    }
    
    Serial.println("Conectado vía HTTPS");
    
    client.print("POST ");
    client.print(path);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonData.length());
    client.println();
    client.print(jsonData);
    
    String response = "";
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000) {
      while (client.available()) {
        char c = client.read();
        response += c;
      }
    }
    
    client.stop();
    
    Serial.print("Respuesta del servidor: ");
    Serial.println(response);
    
    // Parsear respuesta - manejar Transfer-Encoding: chunked
    String jsonResponse = extractJSONFromResponse(response);
    
    if (jsonResponse.length() == 0) {
      Serial.println("Error: No se pudo extraer JSON de la respuesta");
      return false;
    }
    
    Serial.print("JSON extraído: ");
    Serial.println(jsonResponse);
    
    DynamicJsonDocument responseDoc(1024);
    DeserializationError error = deserializeJson(responseDoc, jsonResponse);
    
    if (error || !responseDoc["success"]) {
      Serial.print("Error parseando respuesta: ");
      Serial.println(error.c_str());
      if (responseDoc.containsKey("error")) {
        Serial.print("Error del servidor: ");
        Serial.println(responseDoc["error"].as<String>());
      }
      return false;
    }
    
    // Guardar configuración
    strncpy(deviceConfig.serverUrl, serverUrl.c_str(), sizeof(deviceConfig.serverUrl) - 1);
    deviceConfig.serverUrl[sizeof(deviceConfig.serverUrl) - 1] = '\0';
    deviceConfig.serverPort = port;
    strncpy(deviceConfig.deviceToken, responseDoc["data"]["token"].as<const char*>(), sizeof(deviceConfig.deviceToken) - 1);
    deviceConfig.deviceToken[sizeof(deviceConfig.deviceToken) - 1] = '\0';
    deviceConfig.deviceId = responseDoc["data"]["id"];
    deviceConfig.useHTTPS = useHTTPS;
    deviceConfig.configured = true;
    
    configManager.saveConfig(deviceConfig);
    
    Serial.println("Dispositivo registrado exitosamente!");
    return true;
    
  } else {
    // Usar WiFiClient normal para HTTP
    WiFiClient client;
    
    if (!client.connect(host.c_str(), port)) {
      Serial.println("Error de conexión HTTP al servidor");
      return false;
    }
    
    Serial.println("Conectado vía HTTP");
    
    client.print("POST ");
    client.print(path);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonData.length());
    client.println();
    client.print(jsonData);
    
    String response = "";
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000) {
      while (client.available()) {
        char c = client.read();
        response += c;
      }
    }
    
    client.stop();
    
    Serial.print("Respuesta del servidor: ");
    Serial.println(response);
    
    // Parsear respuesta - manejar Transfer-Encoding: chunked
    String jsonResponse = extractJSONFromResponse(response);
    
    if (jsonResponse.length() == 0) {
      Serial.println("Error: No se pudo extraer JSON de la respuesta");
      return false;
    }
    
    Serial.print("JSON extraído: ");
    Serial.println(jsonResponse);
    
    DynamicJsonDocument responseDoc(1024);
    DeserializationError error = deserializeJson(responseDoc, jsonResponse);
    
    if (error || !responseDoc["success"]) {
      Serial.print("Error parseando respuesta: ");
      Serial.println(error.c_str());
      if (responseDoc.containsKey("error")) {
        Serial.print("Error del servidor: ");
        Serial.println(responseDoc["error"].as<String>());
      }
      return false;
    }
    
    // Guardar configuración
    strncpy(deviceConfig.serverUrl, serverUrl.c_str(), sizeof(deviceConfig.serverUrl) - 1);
    deviceConfig.serverUrl[sizeof(deviceConfig.serverUrl) - 1] = '\0';
    deviceConfig.serverPort = port;
    strncpy(deviceConfig.deviceToken, responseDoc["data"]["token"].as<const char*>(), sizeof(deviceConfig.deviceToken) - 1);
    deviceConfig.deviceToken[sizeof(deviceConfig.deviceToken) - 1] = '\0';
    deviceConfig.deviceId = responseDoc["data"]["id"];
    deviceConfig.useHTTPS = useHTTPS;
    deviceConfig.configured = true;
    
    configManager.saveConfig(deviceConfig);
    
    Serial.println("Dispositivo registrado exitosamente!");
    return true;
  }
}

// Función para extraer JSON de respuesta HTTP (maneja chunked encoding)
String extractJSONFromResponse(const String& response) {
  // Buscar el inicio del JSON directamente
  int jsonStart = response.indexOf("{\"");
  if (jsonStart < 0) {
    // Intentar buscar después de los headers
    jsonStart = response.indexOf("\r\n\r\n");
    if (jsonStart < 0) {
      return "";
    }
    String body = response.substring(jsonStart + 4);
    // Decodificar si es chunked
    return decodeChunkedResponse(body);
  }
  
  // Extraer JSON desde el inicio encontrado
  int jsonEnd = response.lastIndexOf("}");
  if (jsonEnd > jsonStart) {
    return response.substring(jsonStart, jsonEnd + 1);
  }
  
  // Si no se encuentra el final, extraer hasta el final y limpiar
  String json = response.substring(jsonStart);
  jsonEnd = json.lastIndexOf("}");
  if (jsonEnd > 0) {
    return json.substring(0, jsonEnd + 1);
  }
  
  return "";
}

// Función para decodificar respuesta chunked
String decodeChunkedResponse(const String& chunkedData) {
  String result = "";
  int pos = 0;
  
  while (pos < chunkedData.length()) {
    // Buscar el tamaño del chunk (en hexadecimal)
    int lineEnd = chunkedData.indexOf("\r\n", pos);
    if (lineEnd < 0) break;
    
    String chunkSizeStr = chunkedData.substring(pos, lineEnd);
    chunkSizeStr.trim();
    
    // Si el tamaño es "0", es el último chunk
    if (chunkSizeStr == "0" || chunkSizeStr.length() == 0) {
      break;
    }
    
    // Convertir tamaño hexadecimal a decimal
    int chunkSize = 0;
    for (int i = 0; i < chunkSizeStr.length(); i++) {
      char c = chunkSizeStr.charAt(i);
      int digit = 0;
      if (c >= '0' && c <= '9') {
        digit = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        digit = c - 'a' + 10;
      } else if (c >= 'A' && c <= 'F') {
        digit = c - 'A' + 10;
      }
      chunkSize = chunkSize * 16 + digit;
    }
    
    // Saltar \r\n después del tamaño
    pos = lineEnd + 2;
    
    // Leer los datos del chunk
    if (pos + chunkSize <= chunkedData.length()) {
      result += chunkedData.substring(pos, pos + chunkSize);
      pos += chunkSize;
    } else {
      break;
    }
    
    // Saltar \r\n después de los datos
    if (pos + 2 <= chunkedData.length() && chunkedData.substring(pos, pos + 2) == "\r\n") {
      pos += 2;
    } else {
      break;
    }
  }
  
  return result;
}

String extractHost(const String& url) {
  int start = url.indexOf("://");
  if (start < 0) return url;
  start += 3;
  int end = url.indexOf("/", start);
  if (end < 0) {
    end = url.indexOf(":", start);
    if (end < 0) return url.substring(start);
  }
  return url.substring(start, end);
}

// HTML del portal de configuración
String getConfigPortalHTML() {
  return R"(
<!DOCTYPE html>
<html>
<head>
  <title>Configuración Sistema de Acceso</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; padding: 20px; background: #f0f0f0; }
    .container { max-width: 500px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { color: #333; text-align: center; }
    label { display: block; margin-top: 15px; color: #555; font-weight: bold; }
    input, select { width: 100%; padding: 10px; margin-top: 5px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
    button { width: 100%; padding: 12px; margin-top: 20px; background: #4CAF50; color: white; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; }
    button:hover { background: #45a049; }
    .info { background: #e7f3ff; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔧 Configuración ESP32</h1>
    <div class="info">
      <strong>Instrucciones:</strong><br>
      1. Conecta este dispositivo a tu red WiFi<br>
      2. Ingresa la URL de tu servidor API<br>
      3. Ingresa el código de registro del administrador<br>
      4. Ingresa un nombre para este dispositivo
    </div>
    <form id="configForm">
      <label>URL del Servidor:</label>
      <input type="text" name="serverUrl" placeholder="http://192.168.1.100 o https://api.midominio.com" required>
      
      <label>Usar HTTPS:</label>
      <select name="useHTTPS">
        <option value="0">No (HTTP)</option>
        <option value="1">Sí (HTTPS)</option>
      </select>
      
      <label>Código de Registro:</label>
      <input type="text" name="codigo" placeholder="Código del administrador" required>
      
      <label>Nombre del Dispositivo:</label>
      <input type="text" name="nombre" placeholder="Ej: Puerta Principal" required>
      
      <button type="submit">Registrar Dispositivo</button>
    </form>
    <div id="result"></div>
  </div>
  <script>
    document.getElementById('configForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const formData = new FormData(e.target);
      const data = Object.fromEntries(formData);
      
      const response = await fetch('/register', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams(data)
      });
      
      const result = await response.json();
      const resultDiv = document.getElementById('result');
      
      if (result.success) {
        resultDiv.innerHTML = '<div style="color: green; margin-top: 20px;">' + result.message + '</div>';
        setTimeout(() => {
          resultDiv.innerHTML = '<div style="color: blue; margin-top: 20px;">Reiniciando dispositivo...</div>';
        }, 2000);
      } else {
        resultDiv.innerHTML = '<div style="color: red; margin-top: 20px;">Error: ' + (result.error || 'Desconocido') + '</div>';
      }
    });
  </script>
</body>
</html>
)";
}

// Handlers del servidor web local
void handleGetConfig() {
  if (!deviceConfig.configured) {
    server.send(200, "application/json", "{\"configured\": false}");
    return;
  }
  
  String json = "{";
  json += "\"configured\": true,";
  json += "\"serverUrl\":\"" + String(deviceConfig.serverUrl) + "\",";
  json += "\"serverPort\":" + String(deviceConfig.serverPort) + ",";
  json += "\"deviceId\":" + String(deviceConfig.deviceId) + ",";
  json += "\"useHTTPS\":" + String(deviceConfig.useHTTPS ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleReconfig() {
  configManager.clearConfig();
  server.send(200, "application/json", "{\"message\": \"Configuración borrada. Reiniciando...\"}");
  delay(2000);
  ESP.restart();
}

void handleResetWiFi() {
  // Borrar solo las credenciales WiFi guardadas
  Preferences preferences;
  preferences.begin("wifi_config", false);
  preferences.remove("ssid");
  preferences.remove("password");
  preferences.end();
  
  server.send(200, "application/json", "{\"message\": \"Credenciales WiFi borradas. Reiniciando para configurar nuevo WiFi...\"}");
  delay(2000);
  ESP.restart();
}

void handleEnrollFingerprint() {
  if (!apiClient) {
    server.send(500, "application/json", "{\"success\": false, \"error\": \"Dispositivo no configurado\"}");
    return;
  }
  
  isRegistering = true;
  int id = generarIDUnico();
  int p = -1;
  
  Serial.print("Registrando ID #");
  Serial.println(id);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Registrando ID");
  lcd.setCursor(0, 1);
  lcd.print("ID: ");
  lcd.print(id);
  
  p = getFingerprintEnroll(id);
  if (p == FINGERPRINT_OK) {
    // Enviar huella a Laravel API
    DynamicJsonDocument doc(256);
    doc["idHuella"] = id;
    String jsonData;
    serializeJson(doc, jsonData);
    
    String response = apiClient->post("/api/esp32/huella", jsonData.c_str());
    
    DynamicJsonDocument responseDoc(256);
    deserializeJson(responseDoc, response);
    
    if (responseDoc["success"]) {
      server.send(200, "application/json", "{\"success\": true, \"idHuella\": " + String(id) + "}");
    } else {
      server.send(500, "application/json", "{\"success\": false, \"error\": \"Error al enviar huella al servidor\"}");
    }
  } else {
    server.send(500, "application/json", "{\"success\": false, \"error\": \"Error al registrar la huella\"}");
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bienvenido :-)");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Esperando user");
  isRegistering = false;
}

void handleDeleteFingerprint() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    if (deleteFingerprint(id)) {
      server.send(200, "application/json", "{\"success\": true}");
    } else {
      server.send(500, "application/json", "{\"success\": false, \"error\": \"Error al eliminar la huella\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\": false, \"error\": \"ID de huella no proporcionado\"}");
  }
}

// Funciones de huella digital (sin cambios, solo adaptar llamadas API)
bool deleteFingerprint(uint8_t id) {
  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    Serial.println("Huella eliminada correctamente");
    return true;
  } else {
    Serial.println("Error al eliminar la huella");
    return false;
  }
}

// Función para leer teclado
char readKeypad() {
  static unsigned long lastKeyTime = 0;
  static char lastKey = '\0';
  
  // Debounce global
  if (millis() - lastKeyTime < 300) {
    return '\0';
  }
  
  for (int row = 0; row < 4; row++) {
    digitalWrite(ROW_PINS[row], LOW);
    
    for (int col = 0; col < 4; col++) {
      if (digitalRead(COL_PINS[col]) == LOW) {
        delay(50); // Debounce
        // Verificar que realmente está presionado
        if (digitalRead(COL_PINS[col]) == LOW) {
          char newKey = KEY_MAP[row][col];
          digitalWrite(ROW_PINS[row], HIGH);
          
          // Evitar repetición de la misma tecla
          if (newKey != lastKey || millis() - lastKeyTime > 500) {
            lastKey = newKey;
            lastKeyTime = millis();
            return newKey;
          }
        }
      }
    }
    
    digitalWrite(ROW_PINS[row], HIGH);
  }
  
  lastKey = '\0';
  return '\0';
}

// Función para manejar entrada de contraseña
void handlePasswordInput() {
  char key = readKeypad();
  
  if (key != '\0') {
    Serial.print("Tecla presionada: ");
    Serial.println(key);
    
    if (key == '#') {
      // Confirmar contraseña
      verifyPassword();
    } else if (key == '*') {
      // Limpiar contraseña
      currentPassword = "";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ingrese clave:");
      lcd.setCursor(0, 1);
      lcd.print("*");
    } else if (currentPassword.length() < 6) {
      // Agregar dígito a la contraseña
      currentPassword += key;
      lcd.setCursor(0, 1);
      for (int i = 0; i < currentPassword.length(); i++) {
        lcd.print('*');
      }
    }
  }
  
  // Verificar timeout
  if (millis() - passwordStartTime > PASSWORD_TIMEOUT) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Timeout!");
    lcd.setCursor(0, 1);
    lcd.print("Alarma activada");
    delay(2000);
    activarAlarma();
    resetPasswordState();
  }
}

// Función para resetear estado de contraseña
void resetPasswordState() {
  waitingForPassword = false;
  currentPassword = "";
  currentUserID = 0;
  currentUserName = "";
  
  // Pequeño delay para estabilizar el sistema
  delay(100);
}

// Función para obtener contraseña del usuario usando API
String getUserPasswordFromID(int idUsuario) {
  if (!apiClient) {
    Serial.println("API Client no disponible");
    return "";
  }
  
  Serial.print("Obteniendo clave para usuario ID: ");
  Serial.println(idUsuario);
  
  String endpoint = "/api/esp32/usuario/clave/" + String(idUsuario);
  String response = apiClient->get(endpoint.c_str());
  
  if (response.length() == 0) {
    Serial.println("No se recibió respuesta del servidor");
    return "";
  }
  
  Serial.print("JSON Response: ");
  Serial.println(response);
  
  // Parsear JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("Error parseando JSON: ");
    Serial.println(error.c_str());
    Serial.print("JSON que falló: ");
    Serial.println(response);
    return "";
  }
  
  // Verificar respuesta
  if (doc.containsKey("success") && doc["success"] == true) {
    if (doc.containsKey("clave")) {
      String password = doc["clave"].as<String>();
      Serial.print("Clave obtenida exitosamente: ");
      Serial.println(password);
      return password;
    } else {
      Serial.println("Campo 'clave' no encontrado en JSON");
    }
  } else {
    Serial.println("API retornó success: false");
    if (doc.containsKey("error")) {
      Serial.print("Error: ");
      Serial.println(doc["error"].as<String>());
    }
  }
  
  return "";
}

// Función para verificar contraseña
void verifyPassword() {
  Serial.print("Verificando clave para usuario ID: ");
  Serial.println(currentUserID);
  Serial.print("Clave ingresada: ");
  Serial.println(currentPassword);
  
  String storedPassword = getUserPasswordFromID(currentUserID);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  
  if (storedPassword.length() == 0) {
    Serial.println("No se pudo obtener la clave del servidor");
    lcd.print("Error servidor");
    lcd.setCursor(0, 1);
    lcd.print("Reintentar...");
    failedAttempts++; // Contar como intento fallido
    delay(3000);
    resetPasswordState();
    return;
  }
  
  Serial.print("Clave almacenada: ");
  Serial.println(storedPassword);
  
  if (currentPassword == storedPassword) {
    Serial.println("CLAVE CORRECTA - Acceso concedido");
    lcd.print("Acceso concedido!");
    lcd.setCursor(0, 1);
    lcd.print("Bienvenido!");
    
    // RESETEAR INTENTOS FALLIDOS INMEDIATAMENTE
    failedAttempts = 0;
    
    // Activar relay de acceso
    digitalWrite(relayPin, HIGH);
    
    // Establecer estado de acceso concedido
    accessGranted = true;
    accessGrantedTime = millis();
    
    // Intentar registrar el acceso (pero no bloquear si falla)
    logUserAccess(currentUserID, "Apertura con clave");
    
    // Esperar con el relay activo
    delay(3000);
    digitalWrite(relayPin, LOW);
    
    // Esperar a que el dedo se retire del sensor antes de resetear el estado
    Serial.println("Retira tu dedo del sensor...");
    unsigned long waitStart = millis();
    while (finger.getImage() != FINGERPRINT_NOFINGER && (millis() - waitStart < 10000)) {
      delay(100);
    }
    
    // Resetear estado después de que el dedo se retire
    resetPasswordState();
    
    // Pequeño delay adicional para estabilizar
    delay(500);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Esperando user");
    return; // Salir de la función sin procesar más
    
  } else {
    Serial.println("CLAVE INCORRECTA");
    lcd.print("Clave incorrecta!");
    lcd.setCursor(0, 1);
    lcd.print("Intento: " + String(failedAttempts + 1));
    failedAttempts++;
    
    if (failedAttempts >= 3) {
      Serial.println("DEMASIADOS INTENTOS - Activando alarma");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Alarma activada!");
      delay(2000);
      activarAlarma();
    }
  }
  
  resetPasswordState();
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Esperando user");
}

void verifyFingerprint() {
  if (!apiClient) return; // No hacer nada si no hay API configurada
  
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) {
    return;
  } else if (p != FINGERPRINT_OK) {
    return;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    return;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Huella reconocida!");
    uint8_t id = finger.fingerID;
    int idUsuario;
    String userName = getUserNameFromID(id, idUsuario);
    
    // REINICIAR CONTADOR DE INTENTOS FALLIDOS CUANDO LA HUELLA ES RECONOCIDA
    failedAttempts = 0;
    
    if (idUsuario == 0) {
      if (deleteFingerprint(id)) {
        Serial.println("Huella eliminada porque el ID del usuario es 0");
      } else {
        Serial.println("Error al eliminar la huella con ID 0");
      }
      return;
    }

    if (userName.length() > 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Huella OK");
      lcd.setCursor(0, 1);
      lcd.print("Ingrese clave...");
      
      // Configurar estado para entrada de contraseña
      waitingForPassword = true;
      passwordStartTime = millis();
      currentUserID = idUsuario;
      currentUserName = userName;
      
      // Mostrar instrucciones en LCD
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ingrese clave:");
      lcd.setCursor(0, 1);
      lcd.print("*");
      
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Huella reconocida");
      lcd.setCursor(0, 1);
      lcd.print("Nombre no encontrado");
      delay(2000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Esperando user");
    }
    
  } else {
    Serial.println("Huella no reconocida");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Huella no recon.");
    delay(300);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("     ALERTA     ");
    failedAttempts++;
    if (failedAttempts >= 3) {
      activarAlarma();
    }
  }
}

String getUserNameFromID(uint8_t idHuella, int &idUsuario) {
  if (!apiClient) {
    idUsuario = 0;
    return "";
  }
  
  String endpoint = "/api/esp32/usuario/" + String(idHuella);
  String response = apiClient->get(endpoint.c_str());
  
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.println("Error al parsear JSON");
    idUsuario = 0;
    return "";
  }
  
  String nombre = doc["nombre"].as<String>();
  idUsuario = doc["idUsuario"].as<int>();
  
  return nombre;
}

uint8_t getFingerprintEnroll(uint8_t id) {
  int p = -1;
  Serial.println("Coloca tu huella en el sensor...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Coloca tu huella");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Imagen tomada");
        lcd.setCursor(0, 1);
        lcd.print("Imagen tomada");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      default:
        Serial.println("Error");
        break;
    }
    delay(100);
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    return p;
  }

  Serial.println("Retira tu dedo...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Retira tu dedo");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(100);
  }

  Serial.println("Coloca el mismo dedo nuevamente...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Coloca el mismo");
  lcd.setCursor(0, 1);
  lcd.print("dedo nuevamente");

  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Imagen tomada");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      default:
        break;
    }
    delay(100);
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    return p;
  }

  Serial.println("Verificando huella...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Verificando...");
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    return p;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Huella guardada en ID #" + String(id));
    lcd.setCursor(0, 1);
    lcd.print("Huella guardada");
  }
  
  return p;
}

int generarIDUnico() {
  static int id = 10;
  return id++;
}

void logUserAccess(int idUsuario, String tipoAcceso) {
  if (!apiClient) return;
  
  DynamicJsonDocument doc(256);
  doc["idUsuario"] = idUsuario;
  doc["tipo_acceso"] = tipoAcceso;
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  apiClient->post("/api/esp32/acceso", jsonData.c_str());
}

void registrarAlarmaEnBD(String descripcion) {
  if (!apiClient) return;
  
  DynamicJsonDocument doc(256);
  doc["descripcion"] = descripcion;
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  apiClient->post("/api/esp32/alarma", jsonData.c_str());
}

String getAdminPhoneNumber() {
  if (!apiClient) return "";
  
  String response = apiClient->get("/api/esp32/admin/telefono");
  
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    return "";
  }
  
  return doc["telefono_admin"].as<String>();
}

String getAPIKeyFromDB() {
  if (!apiClient) return "";
  
  String response = apiClient->get("/api/esp32/admin/codigo");
  
  Serial.print("Respuesta completa API Key: ");
  Serial.println(response);
  Serial.print("Longitud: ");
  Serial.println(response.length());
  
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("Error parseando JSON: ");
    Serial.println(error.c_str());
    return "";
  }
  
  Serial.print("JSON parseado correctamente. Codigo: ");
  Serial.println(doc["codigo"].as<String>());
  
  return doc["codigo"].as<String>();
}

String urlencode(const String &str) {
  String encoded = "";
  char c;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encoded += "%20";
    } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      encoded += String((c >> 4) & 0x0F, HEX);
      encoded += String(c & 0x0F, HEX);
    }
  }
  return encoded;
}

void sendWhatsAppAlert() {
  String adminPhone = getAdminPhoneNumber();
  if (adminPhone.length() == 0) {
    Serial.println("No se pudo obtener el número de teléfono del administrador");
    return;
  }

  WiFiClient client;
  const char* host = "api.callmebot.com";
  const int port = 80;
  String path = String("/whatsapp.php?phone=") + urlencode(adminPhone) +
                 "&text=" + urlencode("ALERTA: Activación de la alarma") +
                 "&apikey=" + urlencode(callMeBotAPIKey);

  if (!client.connect(host, port)) {
    Serial.println("Error de conexión a CallMeBot");
    return;
  }

  client.println("GET " + path + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Connection: close");
  client.println();

  String response = "";
  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 5000) {
    while (client.available()) {
      char c = client.read();
      response += c;
    }
  }

  client.stop();
}

void activarAlarma() {
  digitalWrite(relayFailPin, HIGH);
  sendWhatsAppAlert();
  registrarAlarmaEnBD("Alarma activada");
  delay(5000);
  digitalWrite(relayFailPin, LOW);
  failedAttempts = 0;
}

