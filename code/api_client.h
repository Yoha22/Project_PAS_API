#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

class APIClient {
private:
  WiFiClient client;
  WiFiClientSecure clientSecure;
  DeviceConfig* config;
  bool useHTTPS;
  
public:
  APIClient(DeviceConfig* cfg) : config(cfg) {
    useHTTPS = config->useHTTPS;
    if (useHTTPS) {
      clientSecure.setInsecure(); // Para desarrollo, en producción usar certificados
    }
  }
  
  // Realizar petición GET
  String get(const char* endpoint, bool useAuth = true) {
    String url = String(config->serverUrl) + endpoint;
    Serial.print("GET: ");
    Serial.println(url);
    
    if (useHTTPS) {
      if (!clientSecure.connect(extractHost(url).c_str(), config->serverPort)) {
        Serial.println("Error de conexión HTTPS");
        return "";
      }
      
      clientSecure.print("GET ");
      clientSecure.print(extractPath(url).c_str());
      clientSecure.println(" HTTP/1.1");
      clientSecure.print("Host: ");
      clientSecure.println(extractHost(url).c_str());
      if (useAuth) {
        clientSecure.print("Authorization: Bearer ");
        clientSecure.println(config->deviceToken);
      }
      clientSecure.println("Connection: close");
      clientSecure.println();
      
      return readResponse(&clientSecure);
    } else {
      if (!client.connect(extractHost(url).c_str(), config->serverPort)) {
        Serial.println("Error de conexión HTTP");
        return "";
      }
      
      client.print("GET ");
      client.print(extractPath(url).c_str());
      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.println(extractHost(url).c_str());
      if (useAuth) {
        client.print("Authorization: Bearer ");
        client.println(config->deviceToken);
      }
      client.println("Connection: close");
      client.println();
      
      return readResponse(&client);
    }
  }
  
  // Realizar petición POST
  String post(const char* endpoint, const char* data, bool useAuth = true) {
    String url = String(config->serverUrl) + endpoint;
    Serial.print("POST: ");
    Serial.println(url);
    
    if (useHTTPS) {
      if (!clientSecure.connect(extractHost(url).c_str(), config->serverPort)) {
        Serial.println("Error de conexión HTTPS");
        return "";
      }
      
      clientSecure.print("POST ");
      clientSecure.print(extractPath(url).c_str());
      clientSecure.println(" HTTP/1.1");
      clientSecure.print("Host: ");
      clientSecure.println(extractHost(url).c_str());
      clientSecure.println("Content-Type: application/json");
      if (useAuth) {
        clientSecure.print("Authorization: Bearer ");
        clientSecure.println(config->deviceToken);
      }
      clientSecure.print("Content-Length: ");
      clientSecure.println(strlen(data));
      clientSecure.println();
      clientSecure.print(data);
      
      return readResponse(&clientSecure);
    } else {
      if (!client.connect(extractHost(url).c_str(), config->serverPort)) {
        Serial.println("Error de conexión HTTP");
        return "";
      }
      
      client.print("POST ");
      client.print(extractPath(url).c_str());
      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.println(extractHost(url).c_str());
      client.println("Content-Type: application/json");
      if (useAuth) {
        client.print("Authorization: Bearer ");
        client.println(config->deviceToken);
      }
      client.print("Content-Length: ");
      client.println(strlen(data));
      client.println();
      client.print(data);
      
      return readResponse(&client);
    }
  }
  
private:
  String readResponse(Client* stream) {
    String response = "";
    unsigned long timeout = millis();
    
    while (stream->connected() && millis() - timeout < 5000) {
      while (stream->available()) {
        char c = stream->read();
        response += c;
      }
    }
    
    // Extraer JSON del cuerpo de la respuesta
    int jsonStart = response.indexOf("\r\n\r\n");
    if (jsonStart >= 0) {
      String body = response.substring(jsonStart + 4);
      body.trim();
      return body;
    }
    
    return "";
  }
  
  String extractHost(const String& url) {
    // Extraer host de URL (ej: http://192.168.1.100 -> 192.168.1.100)
    int start = url.indexOf("://");
    if (start < 0) return url;
    start += 3;
    int end = url.indexOf("/", start);
    if (end < 0) end = url.indexOf(":", start);
    if (end < 0) return url.substring(start);
    return url.substring(start, end);
  }
  
  String extractPath(const String& url) {
    // Extraer path de URL
    int start = url.indexOf("://");
    if (start < 0) return url;
    start += 3;
    int pathStart = url.indexOf("/", start);
    if (pathStart < 0) return "/";
    return url.substring(pathStart);
  }
};

#endif

