#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
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

// Variables para manejo de errores y watchdog
unsigned long lastWatchdogFeed = 0;
const unsigned long WATCHDOG_FEED_INTERVAL = 3000; // Alimentar watchdog cada 3 segundos
unsigned long lastLoopTime = 0;
const unsigned long MAX_LOOP_TIME = 10000; // Máximo 10 segundos por iteración de loop
int errorCount = 0;
const int MAX_ERRORS_BEFORE_RESTART = 50; // Reiniciar si hay más de 50 errores consecutivos

// Declaraciones forward de funciones
void handleGetConfig();
void handleReconfig();
void handleResetWiFi();
void handleEnrollFingerprint();
void handleDeleteFingerprint();
void handleCommand();
void handleStatus();
void handleControl();
bool processCommand(String command, JsonObject payload);
void sendWhatsAppAlert();
void sendCallAlert();
void sendHybridAlert();
void addCORSHeaders(WebServer& server);
void handleOptions(WebServer& server);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Configurar Watchdog Timer (8 segundos)
  // Si el loop() tarda más de 8 segundos, el ESP32 se reiniciará automáticamente
  esp_task_wdt_init(8, true); // 8 segundos, reiniciar si no se alimenta
  esp_task_wdt_add(NULL); // Agregar la tarea actual al watchdog
  
  Serial.println("Watchdog timer configurado (8 segundos)");
  
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
    Serial.println("ERROR CRÍTICO: No se pudo encontrar el sensor de huellas");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor ERROR");
    lcd.setCursor(0, 1);
    lcd.print("Reiniciando...");
    
    // Intentar verificar el sensor varias veces antes de reiniciar
    delay(2000);
    for (int i = 0; i < 3; i++) {
      Serial.print("Reintento de verificación del sensor: ");
      Serial.println(i + 1);
      if (finger.verifyPassword()) {
        Serial.println("Sensor encontrado en reintento!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sensor OK");
        break;
      }
      delay(1000);
    }
    
    // Si después de los reintentos no funciona, reiniciar
    if (!finger.verifyPassword()) {
      Serial.println("Sensor no encontrado después de reintentos. Reiniciando...");
      delay(2000);
      ESP.restart();
    }
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
  // Handlers OPTIONS para CORS (preflight)
  server.on("/registrarHuella", HTTP_OPTIONS, []() { handleOptions(server); });
  server.on("/eliminarHuella", HTTP_OPTIONS, []() { handleOptions(server); });
  server.on("/config", HTTP_OPTIONS, []() { handleOptions(server); });
  server.on("/reconfig", HTTP_OPTIONS, []() { handleOptions(server); });
  server.on("/resetwifi", HTTP_OPTIONS, []() { handleOptions(server); });
  server.on("/command", HTTP_OPTIONS, []() { handleOptions(server); });
  server.on("/status", HTTP_OPTIONS, []() { handleOptions(server); });
  server.on("/control", HTTP_OPTIONS, []() { handleOptions(server); });
  
  // Handlers normales
  server.on("/registrarHuella", HTTP_GET, handleEnrollFingerprint);
  server.on("/eliminarHuella", HTTP_POST, handleDeleteFingerprint);
  server.on("/config", HTTP_GET, handleGetConfig);
  server.on("/reconfig", HTTP_POST, handleReconfig);
  server.on("/resetwifi", HTTP_GET, handleResetWiFi);
  server.on("/command", HTTP_POST, handleCommand);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/control", HTTP_POST, handleControl);
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
  unsigned long loopStartTime = millis();
  
  // Alimentar watchdog periódicamente
  if (millis() - lastWatchdogFeed > WATCHDOG_FEED_INTERVAL) {
    esp_task_wdt_reset();
    lastWatchdogFeed = millis();
  }
  
  // Verificar que el loop no esté tomando demasiado tiempo
  if (lastLoopTime > 0 && (millis() - lastLoopTime) > MAX_LOOP_TIME) {
    Serial.println("WARNING: Loop tardó más de 10 segundos");
    errorCount++;
  } else {
    errorCount = 0; // Resetear contador si el loop es normal
  }
  
  // Si hay demasiados errores consecutivos, reiniciar
  if (errorCount > MAX_ERRORS_BEFORE_RESTART) {
    Serial.println("ERROR: Demasiados errores consecutivos. Reiniciando...");
    delay(2000);
    ESP.restart();
  }
  
  lastLoopTime = millis();
  
  // Manejar cliente del servidor web (no bloqueante)
  server.handleClient();
  
  // Reconectar WiFi si es necesario (no bloqueante)
  wifiManager.reconnect();
  
  // Si el acceso fue concedido recientemente, ignorar otras entradas
  if (accessGranted && (millis() - accessGrantedTime < ACCESS_DURATION)) {
    return; // No procesar huellas o teclado durante este tiempo
  }
  
  // Después del período de acceso concedido, esperar a que el dedo se retire
  if (accessGranted) {
    // Esperar a que el dedo se retire del sensor antes de continuar (con timeout)
    unsigned long waitStart = millis();
    const unsigned long MAX_FINGER_WAIT = 10000; // Máximo 10 segundos
    
    while (finger.getImage() != FINGERPRINT_NOFINGER && (millis() - waitStart < MAX_FINGER_WAIT)) {
      esp_task_wdt_reset(); // Alimentar watchdog durante la espera
      delay(100);
      server.handleClient(); // Mantener el servidor web respondiendo
    }
    
    // Si el timeout se alcanzó, forzar salida
    if (millis() - waitStart >= MAX_FINGER_WAIT) {
      Serial.println("WARNING: Timeout esperando que el dedo se retire");
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
  
  // Alimentar watchdog al final del loop
  esp_task_wdt_reset();
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
    String html = "<!DOCTYPE html><html lang='es' class='h-full'>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Configuración ESP32 - Sistema de Acceso</title>";
    html += "<script src='https://cdn.tailwindcss.com'></script>";
    html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css'>";
    html += "<script>";
    html += "if (localStorage.getItem('darkMode') === 'true' || (!localStorage.getItem('darkMode') && window.matchMedia('(prefers-color-scheme: dark)').matches)) {";
    html += "document.documentElement.classList.add('dark');";
    html += "}";
    html += "</script>";
    html += "</head>";
    html += "<body class='bg-gray-100 dark:bg-gray-900 min-h-full flex items-center justify-center p-4'>";
    html += "<div class='bg-white dark:bg-gray-800 rounded-lg shadow-lg p-6 md:p-8 w-full max-w-md'>";
    html += "<div class='flex items-center justify-center mb-6'>";
    html += "<i class='fas fa-cog text-3xl text-blue-600 dark:text-blue-400 mr-3'></i>";
    html += "<h1 class='text-2xl md:text-3xl font-bold text-gray-800 dark:text-white'>Configuración ESP32</h1>";
    html += "</div>";
    html += "<div class='bg-blue-50 dark:bg-blue-900/30 border border-blue-200 dark:border-blue-800 rounded-lg p-4 mb-6'>";
    html += "<p class='text-sm text-blue-800 dark:text-blue-200 font-semibold mb-2'><i class='fas fa-info-circle mr-2'></i>Instrucciones:</p>";
    html += "<ol class='text-xs text-blue-700 dark:text-blue-300 space-y-1 list-decimal list-inside'>";
    html += "<li>Conecta este dispositivo a tu red WiFi</li>";
    html += "<li>Ingresa la URL de tu servidor API</li>";
    html += "<li>Ingresa el código de registro del administrador</li>";
    html += "<li>Ingresa un nombre para este dispositivo</li>";
    html += "</ol>";
    html += "</div>";
    html += "<form id='configForm' class='space-y-4'>";
    html += "<div>";
    html += "<label class='block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2'><i class='fas fa-wifi mr-2'></i>Red WiFi:</label>";
    html += "<input type='text' name='ssid' placeholder='Nombre de la red WiFi' required ";
    html += "class='w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500 focus:border-transparent'>";
    html += "</div>";
    html += "<div>";
    html += "<label class='block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2'><i class='fas fa-lock mr-2'></i>Contraseña WiFi:</label>";
    html += "<input type='password' name='password' placeholder='Contraseña de la red WiFi' required ";
    html += "class='w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500 focus:border-transparent'>";
    html += "</div>";
    html += "<div>";
    html += "<label class='block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2'><i class='fas fa-server mr-2'></i>URL del Servidor:</label>";
    html += "<input type='text' name='serverUrl' placeholder='http://192.168.1.100 o https://api.midominio.com' required ";
    html += "class='w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500 focus:border-transparent'>";
    html += "</div>";
    html += "<div>";
    html += "<label class='block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2'><i class='fas fa-shield-alt mr-2'></i>Usar HTTPS:</label>";
    html += "<select name='useHTTPS' class='w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500 focus:border-transparent'>";
    html += "<option value='0'>No (HTTP)</option>";
    html += "<option value='1'>Sí (HTTPS)</option>";
    html += "</select>";
    html += "</div>";
    html += "<div>";
    html += "<label class='block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2'><i class='fas fa-key mr-2'></i>Código de Registro:</label>";
    html += "<input type='text' name='codigo' placeholder='Código del administrador' required ";
    html += "class='w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500 focus:border-transparent'>";
    html += "</div>";
    html += "<div>";
    html += "<label class='block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2'><i class='fas fa-tag mr-2'></i>Nombre del Dispositivo:</label>";
    html += "<input type='text' name='nombre' placeholder='Ej: Puerta Principal' required ";
    html += "class='w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500 focus:border-transparent'>";
    html += "</div>";
    html += "<button type='submit' class='w-full bg-blue-600 hover:bg-blue-700 dark:bg-blue-500 dark:hover:bg-blue-600 text-white font-semibold py-3 px-4 rounded-lg transition-colors duration-200 flex items-center justify-center'>";
    html += "<i class='fas fa-save mr-2'></i>Configurar Dispositivo";
    html += "</button>";
    html += "</form>";
    html += "<div id='result' class='mt-4'></div>";
    html += "</div>";
    html += "<script>";
    html += "document.getElementById('configForm').addEventListener('submit', async (e) => {";
    html += "e.preventDefault();";
    html += "const submitBtn = e.target.querySelector('button[type=\"submit\"]');";
    html += "const originalText = submitBtn.innerHTML;";
    html += "submitBtn.disabled = true;";
    html += "submitBtn.innerHTML = '<i class=\"fas fa-spinner fa-spin mr-2\"></i>Configurando...';";
    html += "const formData = new FormData(e.target);";
    html += "const data = Object.fromEntries(formData);";
    html += "try {";
    html += "const response = await fetch('/config', {";
    html += "method: 'POST',";
    html += "headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "body: new URLSearchParams(data)";
    html += "});";
    html += "const result = await response.json();";
    html += "const resultDiv = document.getElementById('result');";
    html += "if (result.success) {";
    html += "resultDiv.innerHTML = '<div class=\"bg-green-100 dark:bg-green-900/30 border border-green-400 dark:border-green-700 text-green-700 dark:text-green-300 px-4 py-3 rounded-lg\"><i class=\"fas fa-check-circle mr-2\"></i>' + result.message + '</div>';";
    html += "setTimeout(() => { resultDiv.innerHTML = '<div class=\"bg-blue-100 dark:bg-blue-900/30 border border-blue-400 dark:border-blue-700 text-blue-700 dark:text-blue-300 px-4 py-3 rounded-lg\"><i class=\"fas fa-sync-alt fa-spin mr-2\"></i>Reiniciando dispositivo...</div>'; }, 2000);";
    html += "} else {";
    html += "resultDiv.innerHTML = '<div class=\"bg-red-100 dark:bg-red-900/30 border border-red-400 dark:border-red-700 text-red-700 dark:text-red-300 px-4 py-3 rounded-lg\"><i class=\"fas fa-exclamation-circle mr-2\"></i>Error: ' + (result.error || 'Desconocido') + '</div>';";
    html += "submitBtn.disabled = false;";
    html += "submitBtn.innerHTML = originalText;";
    html += "}";
    html += "} catch (error) {";
    html += "const resultDiv = document.getElementById('result');";
    html += "resultDiv.innerHTML = '<div class=\"bg-red-100 dark:bg-red-900/30 border border-red-400 dark:border-red-700 text-red-700 dark:text-red-300 px-4 py-3 rounded-lg\"><i class=\"fas fa-exclamation-triangle mr-2\"></i>Error de conexión: ' + error.message + '</div>';";
    html += "submitBtn.disabled = false;";
    html += "submitBtn.innerHTML = originalText;";
    html += "}";
    html += "});";
    html += "</script>";
    html += "</body></html>";
    addCORSHeaders(configServer);
    configServer.send(200, "text/html; charset=utf-8", html);
  });
  
  // Handler para guardar configuración (se usa en /config y /register)
  auto handleConfigSave = [&configServer]() {
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
    
    addCORSHeaders(configServer);
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
  };
  
  // Registrar el handler en ambos endpoints
  configServer.on("/config", HTTP_POST, handleConfigSave);
  configServer.on("/register", HTTP_POST, handleConfigSave);
  
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

// Función helper para agregar headers CORS
void addCORSHeaders(WebServer& server) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
  server.sendHeader("Access-Control-Max-Age", "3600");
}

// Handler para preflight OPTIONS requests
void handleOptions(WebServer& server) {
  addCORSHeaders(server);
  server.send(204); // No Content
}

// Handlers del servidor web local
void handleGetConfig() {
  addCORSHeaders(server);
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
  addCORSHeaders(server);
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
  
  addCORSHeaders(server);
  server.send(200, "application/json", "{\"message\": \"Credenciales WiFi borradas. Reiniciando para configurar nuevo WiFi...\"}");
  delay(2000);
  ESP.restart();
}

void handleEnrollFingerprint() {
  addCORSHeaders(server);
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
      addCORSHeaders(server);
      server.send(200, "application/json", "{\"success\": true, \"idHuella\": " + String(id) + "}");
    } else {
      addCORSHeaders(server);
      server.send(500, "application/json", "{\"success\": false, \"error\": \"Error al enviar huella al servidor\"}");
    }
  } else {
    addCORSHeaders(server);
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
  addCORSHeaders(server);
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

// Handler para recibir comandos remotos
void handleCommand() {
  addCORSHeaders(server);
  
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      server.send(400, "application/json", "{\"success\": false, \"error\": \"JSON inválido\"}");
      return;
    }
    
    String messageId = doc["message_id"].as<String>();
    String command = doc["command"].as<String>();
    JsonObject payload = doc["payload"].as<JsonObject>();
    
    Serial.print("Comando recibido: ");
    Serial.println(command);
    Serial.print("Message ID: ");
    Serial.println(messageId);
    
    // Procesar el comando
    bool success = processCommand(command, payload);
    
    if (success) {
      String response = "{\"success\": true, \"message_id\": \"" + messageId + "\"}";
      server.send(200, "application/json", response);
    } else {
      String response = "{\"success\": false, \"message_id\": \"" + messageId + "\", \"error\": \"Error al procesar comando\"}";
      server.send(500, "application/json", response);
    }
  } else {
    server.send(400, "application/json", "{\"success\": false, \"error\": \"Cuerpo de petición vacío\"}");
  }
}

// Handler para obtener estado del dispositivo
void handleStatus() {
  addCORSHeaders(server);
  
  String json = "{";
  json += "\"success\": true,";
  json += "\"device_id\": " + String(deviceConfig.deviceId) + ",";
  json += "\"configured\": " + String(deviceConfig.configured ? "true" : "false") + ",";
  json += "\"wifi_connected\": " + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  
  if (WiFi.status() == WL_CONNECTED) {
    json += "\"ip_address\": \"" + WiFi.localIP().toString() + "\",";
    json += "\"ssid\": \"" + WiFi.SSID() + "\",";
    json += "\"rssi\": " + String(WiFi.RSSI()) + ",";
  }
  
  json += "\"free_heap\": " + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime\": " + String(millis() / 1000) + ",";
  json += "\"fingerprint_sensor\": " + String(finger.verifyPassword() ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

// Handler para control remoto
void handleControl() {
  addCORSHeaders(server);
  
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      server.send(400, "application/json", "{\"success\": false, \"error\": \"JSON inválido\"}");
      return;
    }
    
    String action = doc["action"].as<String>();
    bool success = false;
    
    Serial.print("Acción de control recibida: ");
    Serial.println(action);
    
    if (action == "relay_on") {
      digitalWrite(relayPin, HIGH);
      success = true;
      Serial.println("Relé activado");
    } else if (action == "relay_off") {
      digitalWrite(relayPin, LOW);
      success = true;
      Serial.println("Relé desactivado");
    } else if (action == "restart") {
      server.send(200, "application/json", "{\"success\": true, \"message\": \"Reiniciando...\"}");
      delay(1000);
      ESP.restart();
      return;
    } else if (action == "reset_wifi") {
      handleResetWiFi();
      return;
    } else {
      server.send(400, "application/json", "{\"success\": false, \"error\": \"Acción no reconocida\"}");
      return;
    }
    
    if (success) {
      server.send(200, "application/json", "{\"success\": true}");
    } else {
      server.send(500, "application/json", "{\"success\": false, \"error\": \"Error al ejecutar acción\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\": false, \"error\": \"Cuerpo de petición vacío\"}");
  }
}

// Función para procesar comandos
bool processCommand(String command, JsonObject payload) {
  if (command == "config") {
    // Procesar configuración
    if (payload.containsKey("ssid") && payload.containsKey("password") && 
        payload.containsKey("serverUrl") && payload.containsKey("codigo") && 
        payload.containsKey("nombre")) {
      String ssid = payload["ssid"].as<String>();
      String password = payload["password"].as<String>();
      String serverUrl = payload["serverUrl"].as<String>();
      bool useHTTPS = payload.containsKey("useHTTPS") && payload["useHTTPS"].as<bool>();
      String codigo = payload["codigo"].as<String>();
      String nombre = payload["nombre"].as<String>();
      
      return registrarDispositivo(serverUrl, codigo, nombre, useHTTPS);
    }
    return false;
  } else if (command == "fingerprint_add") {
    // Agregar huella para usuario
    if (payload.containsKey("user_id")) {
      int userId = payload["user_id"].as<int>();
      isRegistering = true;
      int id = generarIDUnico();
      int p = getFingerprintEnroll(id);
      if (p == FINGERPRINT_OK) {
        // Enviar huella a Laravel API
        DynamicJsonDocument doc(256);
        doc["idHuella"] = id;
        String jsonData;
        serializeJson(doc, jsonData);
        apiClient->post("/api/esp32/huella", jsonData.c_str());
        isRegistering = false;
        return true;
      }
      isRegistering = false;
    }
    return false;
  } else if (command == "fingerprint_delete") {
    // Eliminar huella
    if (payload.containsKey("fingerprint_id")) {
      int fingerprintId = payload["fingerprint_id"].as<int>();
      return deleteFingerprint(fingerprintId);
    }
    return false;
  } else if (command == "status") {
    // El estado ya se maneja en handleStatus()
    return true;
  } else if (command == "sync") {
    // Sincronizar datos (usuarios, huellas)
    // TODO: Implementar sincronización completa
    return true;
  }
  
  return false;
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
  if (!apiClient) {
    // No hacer nada si no hay API configurada, pero loguear el error
    static unsigned long lastApiErrorLog = 0;
    if (millis() - lastApiErrorLog > 60000) { // Log cada minuto para evitar spam
      Serial.println("WARNING: API Client no configurado");
      lastApiErrorLog = millis();
    }
    return;
  }
  
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) {
    return;
  } else if (p != FINGERPRINT_OK) {
    // Loguear errores del sensor pero no bloquear
    Serial.print("ERROR en sensor de huella (getImage): ");
    Serial.println(p);
    return;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.print("ERROR en sensor de huella (image2Tz): ");
    Serial.println(p);
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
  idUsuario = 0; // Inicializar por defecto
  
  if (!apiClient) {
    Serial.println("ERROR: API Client no disponible en getUserNameFromID");
    return "";
  }
  
  String endpoint = "/api/esp32/usuario/" + String(idHuella);
  String response = apiClient->get(endpoint.c_str());
  
  // Verificar si se recibió respuesta
  if (response.length() == 0) {
    Serial.println("ERROR: No se recibió respuesta del servidor en getUserNameFromID");
    return "";
  }
  
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("ERROR al parsear JSON en getUserNameFromID: ");
    Serial.println(error.c_str());
    Serial.print("Respuesta recibida: ");
    Serial.println(response);
    return "";
  }
  
  // Validar que los campos existen antes de acceder
  if (!doc.containsKey("nombre") || !doc.containsKey("idUsuario")) {
    Serial.println("ERROR: Respuesta JSON no contiene campos esperados");
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

  unsigned long enrollStartTime = millis();
  const unsigned long MAX_ENROLL_TIME = 30000; // 30 segundos máximo para capturar huella
  
  while (p != FINGERPRINT_OK && (millis() - enrollStartTime < MAX_ENROLL_TIME)) {
    esp_task_wdt_reset(); // Alimentar watchdog durante la espera
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
        Serial.print("ERROR en sensor: ");
        Serial.println(p);
        delay(100);
        break;
    }
    delay(100);
  }
  
  if (millis() - enrollStartTime >= MAX_ENROLL_TIME) {
    Serial.println("ERROR: Timeout esperando imagen de huella");
    return 0xFF; // Retornar código de error (0xFF es inválido para Adafruit_Fingerprint)
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
  unsigned long waitStart = millis();
  const unsigned long MAX_WAIT_TIME = 10000; // 10 segundos máximo
  
  while (finger.getImage() != FINGERPRINT_NOFINGER && (millis() - waitStart < MAX_WAIT_TIME)) {
    esp_task_wdt_reset(); // Alimentar watchdog durante la espera
    delay(100);
  }
  
  if (millis() - waitStart >= MAX_WAIT_TIME) {
    Serial.println("WARNING: Timeout esperando que se retire el dedo");
  }

  Serial.println("Coloca el mismo dedo nuevamente...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Coloca el mismo");
  lcd.setCursor(0, 1);
  lcd.print("dedo nuevamente");

  p = -1;
  unsigned long enrollStartTime2 = millis(); // Nueva variable para el segundo enroll
  
  while (p != FINGERPRINT_OK && (millis() - enrollStartTime2 < MAX_ENROLL_TIME)) {
    esp_task_wdt_reset(); // Alimentar watchdog durante la espera
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Imagen tomada");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      default:
        Serial.print("ERROR en sensor (2da imagen): ");
        Serial.println(p);
        delay(100);
        break;
    }
    delay(100);
  }
  
  if (millis() - enrollStartTime2 >= MAX_ENROLL_TIME) {
    Serial.println("ERROR: Timeout esperando segunda imagen de huella");
    return 0xFF; // Retornar código de error (0xFF es inválido para Adafruit_Fingerprint)
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
  if (!apiClient) {
    Serial.println("WARNING: No se pudo registrar acceso - API Client no disponible");
    return;
  }
  
  // Intentar registrar acceso, pero no bloquear si falla
  DynamicJsonDocument doc(256);
  doc["idUsuario"] = idUsuario;
  doc["tipo_acceso"] = tipoAcceso;
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  String response = apiClient->post("/api/esp32/acceso", jsonData.c_str());
  
  if (response.length() == 0) {
    Serial.println("WARNING: No se pudo registrar acceso en el servidor");
  } else {
    Serial.println("Acceso registrado exitosamente");
  }
}

void registrarAlarmaEnBD(String descripcion) {
  if (!apiClient) {
    Serial.println("WARNING: No se pudo registrar alarma - API Client no disponible");
    return;
  }
  
  // Intentar registrar alarma, pero no bloquear si falla
  DynamicJsonDocument doc(256);
  doc["descripcion"] = descripcion;
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  String response = apiClient->post("/api/esp32/alarma", jsonData.c_str());
  
  if (response.length() == 0) {
    Serial.println("WARNING: No se pudo registrar alarma en el servidor");
  } else {
    Serial.println("Alarma registrada exitosamente");
  }
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
    Serial.println("ERROR: No se pudo obtener el número de teléfono del administrador");
    return;
  }
  
  if (callMeBotAPIKey.length() == 0) {
    Serial.println("ERROR: API Key de CallMeBot no configurada");
    return;
  }

  String message = "ALERTA: Activación de la alarma";
  
  // Usar WiFiClientSecure para HTTPS
  WiFiClientSecure client;
  client.setInsecure(); // Desactivar verificación de certificado (para desarrollo)
  client.setTimeout(5000); // Timeout de 5 segundos
  
  const char* host = "api.callmebot.com";
  const int port = 443; // HTTPS
  String path = String("/whatsapp.php?phone=") + urlencode(adminPhone) +
                 "&text=" + urlencode(message) +
                 "&apikey=" + urlencode(callMeBotAPIKey);

  unsigned long connectStart = millis();
  if (!client.connect(host, port)) {
    Serial.println("ERROR: No se pudo conectar a CallMeBot (WhatsApp)");
    client.stop();
    return;
  }

  // Verificar timeout de conexión
  if (millis() - connectStart > 5000) {
    Serial.println("ERROR: Timeout conectando a CallMeBot (WhatsApp)");
    client.stop();
    return;
  }

  client.println("GET " + path + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Connection: close");
  client.println();

  String response = "";
  unsigned long timeout = millis();
  const unsigned long RESPONSE_TIMEOUT = 5000;
  
  while (client.connected() && millis() - timeout < RESPONSE_TIMEOUT) {
    esp_task_wdt_reset(); // Alimentar watchdog durante la espera
    while (client.available()) {
      char c = client.read();
      response += c;
    }
  }

  client.stop();
  
  if (response.length() > 0) {
    Serial.println("Notificación WhatsApp enviada exitosamente");
  } else {
    Serial.println("WARNING: No se recibió respuesta de CallMeBot (WhatsApp)");
  }
}

void sendCallAlert() {
  String adminPhone = getAdminPhoneNumber();
  if (adminPhone.length() == 0) {
    Serial.println("ERROR: No se pudo obtener el número de teléfono del administrador");
    return;
  }
  
  if (callMeBotAPIKey.length() == 0) {
    Serial.println("ERROR: API Key de CallMeBot no configurada");
    return;
  }

  String message = "ALERTA: Activación de la alarma";
  
  // Usar WiFiClientSecure para HTTPS
  WiFiClientSecure client;
  client.setInsecure(); // Desactivar verificación de certificado (para desarrollo)
  client.setTimeout(10000); // Timeout de 10 segundos para llamadas
  
  const char* host = "api.callmebot.com";
  const int port = 443; // HTTPS
  String path = String("/call.php?phone=") + urlencode(adminPhone) +
                 "&text=" + urlencode(message) +
                 "&apikey=" + urlencode(callMeBotAPIKey);

  unsigned long connectStart = millis();
  if (!client.connect(host, port)) {
    Serial.println("ERROR: No se pudo conectar a CallMeBot (Llamada)");
    client.stop();
    return;
  }

  // Verificar timeout de conexión
  if (millis() - connectStart > 10000) {
    Serial.println("ERROR: Timeout conectando a CallMeBot (Llamada)");
    client.stop();
    return;
  }

  client.println("GET " + path + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Connection: close");
  client.println();

  String response = "";
  unsigned long timeout = millis();
  const unsigned long RESPONSE_TIMEOUT = 10000; // Más tiempo para llamadas
  
  while (client.connected() && millis() - timeout < RESPONSE_TIMEOUT) {
    esp_task_wdt_reset(); // Alimentar watchdog durante la espera
    while (client.available()) {
      char c = client.read();
      response += c;
    }
  }

  client.stop();
  
  if (response.length() > 0) {
    Serial.println("Llamada de alerta iniciada exitosamente");
  } else {
    Serial.println("WARNING: No se recibió respuesta de CallMeBot (Llamada)");
  }
}

void sendHybridAlert() {
  // Enviar ambas notificaciones: WhatsApp y Llamada
  Serial.println("Enviando notificaciones híbridas (WhatsApp + Llamada)...");
  sendWhatsAppAlert();
  delay(1000); // Pequeño delay entre notificaciones
  sendCallAlert();
}

void activarAlarma() {
  digitalWrite(relayFailPin, HIGH);
  sendHybridAlert(); // Enviar WhatsApp y llamada
  registrarAlarmaEnBD("Alarma activada");
  delay(5000);
  digitalWrite(relayFailPin, LOW);
  failedAttempts = 0;
}

