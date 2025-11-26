#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>

class WiFiManagerCustom {
private:
  WiFiManager wifiManager;
  Preferences preferences;
  static const char* NAMESPACE;
  
public:
  WiFiManagerCustom() {
    wifiManager.setConfigPortalTimeout(300); // 5 minutos timeout
    wifiManager.setAPCallback(configModeCallback);
  }
  
  // Intentar conectar con credenciales guardadas
  bool connect() {
    preferences.begin(NAMESPACE, true);
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    preferences.end();
    
    if (savedSSID.length() > 0) {
      Serial.println("Intentando conectar con credenciales guardadas...");
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
      
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConectado a WiFi!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        return true;
      }
    }
    
    return false;
  }
  
  // Iniciar portal de configuración
  bool startConfigPortal(const char* apName) {
    Serial.println("Iniciando portal de configuración...");
    Serial.print("SSID del AP: ");
    Serial.println(apName);
    
    bool connected = wifiManager.autoConnect(apName);
    
    if (connected) {
      // Guardar credenciales
      preferences.begin(NAMESPACE, false);
      preferences.putString("ssid", WiFi.SSID());
      preferences.putString("password", WiFi.psk());
      preferences.end();
      
      Serial.println("WiFi configurado y guardado!");
      return true;
    }
    
    return false;
  }
  
  // Verificar estado de conexión
  bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
  }
  
  // Obtener IP local
  IPAddress getLocalIP() {
    return WiFi.localIP();
  }
  
  // Reconectar si se perdió la conexión
  void reconnect() {
    if (!isConnected()) {
      Serial.println("Conexión perdida, intentando reconectar...");
      connect();
    }
  }
  
private:
  static void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("Modo de configuración activado");
    Serial.print("IP del AP: ");
    Serial.println(WiFi.softAPIP());
  }
};

const char* WiFiManagerCustom::NAMESPACE = "wifi_config";

#endif

