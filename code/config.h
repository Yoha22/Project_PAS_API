#ifndef CONFIG_H
#define CONFIG_H

#include <Preferences.h>

// Estructura de configuración del dispositivo
struct DeviceConfig {
  char serverUrl[200];
  int serverPort;
  char deviceToken[65];
  int deviceId;
  bool useHTTPS;
  bool configured;
};

class ConfigManager {
private:
  Preferences preferences;
  static const char* NAMESPACE;
  
public:
  ConfigManager() {}
  
  bool begin() {
    return preferences.begin(NAMESPACE, false);
  }
  
  void end() {
    preferences.end();
  }
  
  // Cargar configuración desde memoria
  bool loadConfig(DeviceConfig& config) {
    if (!preferences.isKey("configured")) {
      return false; // No hay configuración guardada
    }
    
    config.configured = preferences.getBool("configured", false);
    if (!config.configured) {
      return false;
    }
    
    // Corregido: usar versión String y luego copiar (ESP32 solo acepta 3 argumentos)
    String serverUrlStr = preferences.getString("serverUrl", "");
    strncpy(config.serverUrl, serverUrlStr.c_str(), sizeof(config.serverUrl) - 1);
    config.serverUrl[sizeof(config.serverUrl) - 1] = '\0';
    
    config.serverPort = preferences.getInt("serverPort", 80);
    
    // Corregido: usar versión String y luego copiar (ESP32 solo acepta 3 argumentos)
    String tokenStr = preferences.getString("deviceToken", "");
    strncpy(config.deviceToken, tokenStr.c_str(), sizeof(config.deviceToken) - 1);
    config.deviceToken[sizeof(config.deviceToken) - 1] = '\0';
    
    config.deviceId = preferences.getInt("deviceId", 0);
    config.useHTTPS = preferences.getBool("useHTTPS", false);
    
    return true;
  }
  
  // Guardar configuración en memoria
  bool saveConfig(const DeviceConfig& config) {
    preferences.putBool("configured", config.configured);
    preferences.putString("serverUrl", config.serverUrl);
    preferences.putInt("serverPort", config.serverPort);
    preferences.putString("deviceToken", config.deviceToken);
    preferences.putInt("deviceId", config.deviceId);
    preferences.putBool("useHTTPS", config.useHTTPS);
    return true;
  }
  
  // Limpiar configuración (para reconfiguración)
  void clearConfig() {
    preferences.clear();
  }
  
  // Verificar si está configurado
  bool isConfigured() {
    return preferences.getBool("configured", false);
  }
};

const char* ConfigManager::NAMESPACE = "device_config";

#endif

