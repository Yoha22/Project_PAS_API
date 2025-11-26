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
      clientSecure.setTimeout(30000); // Timeout de 30 segundos para conexiones HTTPS
    }
  }
  
  // Realizar petición GET
  String get(const char* endpoint, bool useAuth = true) {
    String url = String(config->serverUrl) + endpoint;
    Serial.print("GET: ");
    Serial.println(url);
    
    if (useHTTPS) {
      String host = extractHost(url);
      Serial.print("Conectando a HTTPS: ");
      Serial.print(host);
      Serial.print(":");
      Serial.println(config->serverPort);
      
      unsigned long connectStart = millis();
      bool connected = clientSecure.connect(host.c_str(), config->serverPort);
      unsigned long connectTime = millis() - connectStart;
      
      if (!connected) {
        Serial.print("Error de conexión HTTPS después de ");
        Serial.print(connectTime);
        Serial.println(" ms");
        Serial.print("Host: ");
        Serial.println(host);
        Serial.print("Puerto: ");
        Serial.println(config->serverPort);
        clientSecure.stop();
        return "";
      }
      
      Serial.print("Conectado exitosamente en ");
      Serial.print(connectTime);
      Serial.println(" ms");
      
      clientSecure.print("GET ");
      clientSecure.print(extractPath(url).c_str());
      clientSecure.println(" HTTP/1.1");
      clientSecure.print("Host: ");
      clientSecure.println(host);
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
      String host = extractHost(url);
      Serial.print("Conectando a HTTPS: ");
      Serial.print(host);
      Serial.print(":");
      Serial.println(config->serverPort);
      
      unsigned long connectStart = millis();
      bool connected = clientSecure.connect(host.c_str(), config->serverPort);
      unsigned long connectTime = millis() - connectStart;
      
      if (!connected) {
        Serial.print("Error de conexión HTTPS después de ");
        Serial.print(connectTime);
        Serial.println(" ms");
        Serial.print("Host: ");
        Serial.println(host);
        Serial.print("Puerto: ");
        Serial.println(config->serverPort);
        clientSecure.stop();
        return "";
      }
      
      Serial.print("Conectado exitosamente en ");
      Serial.print(connectTime);
      Serial.println(" ms");
      
      clientSecure.print("POST ");
      clientSecure.print(extractPath(url).c_str());
      clientSecure.println(" HTTP/1.1");
      clientSecure.print("Host: ");
      clientSecure.println(host);
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
    unsigned long readStart = millis();
    const unsigned long READ_TIMEOUT = 30000; // 30 segundos para leer respuesta
    
    Serial.println("Esperando respuesta del servidor...");
    
    while (stream->connected() && millis() - timeout < READ_TIMEOUT) {
      if (stream->available()) {
        char c = stream->read();
        response += c;
        timeout = millis(); // Resetear timeout cuando hay datos
      } else {
        delay(10); // Pequeño delay cuando no hay datos disponibles
        yield(); // Mantener el sistema responsive
      }
      
      // Verificar si ha pasado mucho tiempo sin datos
      if (millis() - timeout > 5000 && response.length() == 0) {
        Serial.println("Timeout esperando datos del servidor");
        break;
      }
    }
    
    unsigned long readTime = millis() - readStart;
    Serial.print("Tiempo de lectura: ");
    Serial.print(readTime);
    Serial.print(" ms, Bytes recibidos: ");
    Serial.println(response.length());
    
    // Verificar si la respuesta está en formato chunked
    bool isChunked = response.indexOf("Transfer-Encoding: chunked") >= 0 || 
                     response.indexOf("transfer-encoding: chunked") >= 0;
    
    // Extraer JSON del cuerpo de la respuesta
    int jsonStart = response.indexOf("\r\n\r\n");
    if (jsonStart >= 0) {
      String body = response.substring(jsonStart + 4);
      body.trim();
      
      // Si es chunked, decodificar
      if (isChunked) {
        body = decodeChunkedResponse(body);
      }
      
      // Buscar JSON directamente si no se encontró
      int jsonPos = body.indexOf("{");
      if (jsonPos >= 0) {
        int jsonEnd = body.lastIndexOf("}");
        if (jsonEnd > jsonPos) {
          return body.substring(jsonPos, jsonEnd + 1);
        }
      }
      
      return body;
    }
    
    // Si no se encontró \r\n\r\n, buscar JSON directamente
    int jsonStart2 = response.indexOf("{");
    if (jsonStart2 >= 0) {
      int jsonEnd2 = response.lastIndexOf("}");
      if (jsonEnd2 > jsonStart2) {
        return response.substring(jsonStart2, jsonEnd2 + 1);
      }
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
    // Extraer host de URL (ej: https://project-pas-api.onrender.com -> project-pas-api.onrender.com)
    int start = url.indexOf("://");
    if (start < 0) return url;
    start += 3;
    
    // Buscar el primer "/" después del protocolo
    int end = url.indexOf("/", start);
    if (end < 0) {
      // No hay path, buscar ":" para puerto o usar el final
      int portPos = url.indexOf(":", start);
      if (portPos < 0) {
        return url.substring(start); // No hay puerto ni path
      }
      return url.substring(start, portPos);
    }
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

