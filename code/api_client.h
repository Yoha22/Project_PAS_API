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
    return request("GET", endpoint, nullptr, useAuth);
  }
  
  // Realizar petición POST
  String post(const char* endpoint, const char* data, bool useAuth = true) {
    return request("POST", endpoint, data, useAuth);
  }
  
private:
  // Método interno para realizar peticiones HTTP
  String request(const char* method, const char* endpoint, const char* data, bool useAuth) {
    String url = String(config->serverUrl) + endpoint;
    Serial.print("[API] ");
    Serial.print(method);
    Serial.print(": ");
    Serial.println(url);
    
    if (useHTTPS) {
      String host = extractHost(url);
      Serial.print("[API] Conectando a HTTPS: ");
      Serial.print(host);
      Serial.print(":");
      Serial.println(config->serverPort);
      
      unsigned long connectStart = millis();
      bool connected = clientSecure.connect(host.c_str(), config->serverPort);
      unsigned long connectTime = millis() - connectStart;
      
      if (!connected) {
        Serial.print("[API] ERROR: Conexión HTTPS falló después de ");
        Serial.print(connectTime);
        Serial.println(" ms");
        Serial.print("[API] Host: ");
        Serial.println(host);
        Serial.print("[API] Puerto: ");
        Serial.println(config->serverPort);
        clientSecure.stop();
        return "";
      }
      
      Serial.print("[API] Conectado exitosamente en ");
      Serial.print(connectTime);
      Serial.println(" ms");
      
      // Enviar request
      clientSecure.print(method);
      clientSecure.print(" ");
      clientSecure.print(extractPath(url).c_str());
      clientSecure.println(" HTTP/1.1");
      clientSecure.print("Host: ");
      clientSecure.println(host);
      
      if (data != nullptr) {
        clientSecure.println("Content-Type: application/json");
        clientSecure.print("Content-Length: ");
        clientSecure.println(strlen(data));
      }
      
      if (useAuth) {
        clientSecure.print("Authorization: Bearer ");
        clientSecure.println(config->deviceToken);
      }
      clientSecure.println("Connection: close");
      clientSecure.println();
      
      if (data != nullptr) {
        clientSecure.print(data);
        Serial.print("[API] Request body enviado (");
        Serial.print(strlen(data));
        Serial.println(" bytes)");
      }
      
      return readResponse(&clientSecure);
    } else {
      String host = extractHost(url);
      if (!client.connect(host.c_str(), config->serverPort)) {
        Serial.println("[API] ERROR: Conexión HTTP falló");
        return "";
      }
      
      Serial.println("[API] Conectado vía HTTP");
      
      // Enviar request
      client.print(method);
      client.print(" ");
      client.print(extractPath(url).c_str());
      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.println(host);
      
      if (data != nullptr) {
        client.println("Content-Type: application/json");
        client.print("Content-Length: ");
        client.println(strlen(data));
      }
      
      if (useAuth) {
        client.print("Authorization: Bearer ");
        client.println(config->deviceToken);
      }
      client.println("Connection: close");
      client.println();
      
      if (data != nullptr) {
        client.print(data);
      }
      
      return readResponse(&client);
    }
  }
  String readResponse(Client* stream) {
    String response = "";
    unsigned long readStart = millis();
    const unsigned long FIRST_BYTE_TIMEOUT = 15000; // 15 segundos para recibir el primer byte
    const unsigned long READ_TIMEOUT = 30000; // 30 segundos total para leer respuesta completa
    const unsigned long IDLE_TIMEOUT = 5000; // 5 segundos sin datos después de recibir algo
    unsigned long lastByteTime = millis();
    unsigned long firstByteTime = 0;
    bool firstByteReceived = false;
    int contentLength = -1;
    bool isChunked = false;
    bool connectionClosed = false;
    
    Serial.println("[API-READ] Esperando respuesta del servidor...");
    Serial.print("[API-READ] Timeout primer byte: ");
    Serial.print(FIRST_BYTE_TIMEOUT / 1000);
    Serial.println(" segundos");
    
    // Esperar primer byte con timeout más largo
    while (!firstByteReceived && (millis() - readStart < FIRST_BYTE_TIMEOUT)) {
      if (stream->available()) {
        firstByteReceived = true;
        firstByteTime = millis();
        Serial.print("[API-READ] Primer byte recibido en ");
        Serial.print(firstByteTime - readStart);
        Serial.println(" ms");
        break;
      }
      yield(); // Mantener sistema responsive
      delay(10);
    }
    
    if (!firstByteReceived) {
      Serial.println("[API-READ] ERROR: Timeout esperando primer byte del servidor");
      stream->stop();
      return "";
    }
    
    // Leer respuesta completa
    lastByteTime = millis();
    while ((millis() - readStart < READ_TIMEOUT) && 
           (!connectionClosed || stream->available())) {
      
      // Verificar si la conexión sigue activa
      if (!stream->connected() && !stream->available()) {
        connectionClosed = true;
        if (response.length() > 0) {
          // Ya recibimos datos, esperar un poco más por datos pendientes
          delay(100);
          if (!stream->available()) {
            Serial.println("[API-READ] Conexión cerrada por servidor");
            break;
          }
        }
      }
      
      // Leer datos disponibles
      if (stream->available()) {
        while (stream->available()) {
          char c = stream->read();
          response += c;
          lastByteTime = millis(); // Actualizar tiempo del último byte
        }
        
        // Intentar detectar Content-Length y Transfer-Encoding en headers
        if (contentLength < 0 && response.indexOf("\r\n\r\n") < 0) {
          int clPos = response.indexOf("Content-Length:");
          if (clPos >= 0) {
            int clEnd = response.indexOf("\r\n", clPos);
            if (clEnd > clPos) {
              String clStr = response.substring(clPos + 15, clEnd);
              clStr.trim();
              contentLength = clStr.toInt();
              Serial.print("[API-READ] Content-Length detectado: ");
              Serial.println(contentLength);
            }
          }
          
          // Detectar Transfer-Encoding: chunked
          if (response.indexOf("Transfer-Encoding: chunked") >= 0 || 
              response.indexOf("transfer-encoding: chunked") >= 0) {
            isChunked = true;
            Serial.println("[API-READ] Respuesta en formato chunked detectada");
          }
        }
        
        // Si tenemos Content-Length y ya recibimos todo, salir
        if (contentLength > 0) {
          int bodyStart = response.indexOf("\r\n\r\n");
          if (bodyStart >= 0) {
            int bodyLength = response.length() - (bodyStart + 4);
            if (bodyLength >= contentLength) {
              Serial.println("[API-READ] Respuesta completa recibida (Content-Length)");
              break;
            }
          }
        }
      } else {
        // Si no hay datos disponibles, verificar timeout de inactividad
        if (response.length() > 0 && (millis() - lastByteTime > IDLE_TIMEOUT)) {
          Serial.println("[API-READ] Timeout de inactividad después de recibir datos");
          break;
        }
        yield();
        delay(10);
      }
      
      // Para respuestas chunked, verificar si terminó (último chunk es "0\r\n\r\n")
      if (isChunked && response.length() > 0) {
        int lastChunkPos = response.lastIndexOf("0\r\n\r\n");
        if (lastChunkPos >= 0 && lastChunkPos > response.length() - 10) {
          Serial.println("[API-READ] Respuesta chunked completa detectada");
          break;
        }
      }
    }
    
    unsigned long readTime = millis() - readStart;
    Serial.print("[API-READ] Tiempo total de lectura: ");
    Serial.print(readTime);
    Serial.println(" ms");
    Serial.print("[API-READ] Bytes recibidos: ");
    Serial.println(response.length());
    
    if (response.length() == 0) {
      Serial.println("[API-READ] ERROR: No se recibió ninguna respuesta");
      stream->stop();
      return "";
    }
    
    // Cerrar conexión explícitamente
    stream->stop();
    
    // Detectar formato chunked si no se detectó antes
    if (!isChunked) {
      isChunked = response.indexOf("Transfer-Encoding: chunked") >= 0 || 
                  response.indexOf("transfer-encoding: chunked") >= 0;
    }
    
    // Extraer cuerpo de la respuesta
    int headerEnd = response.indexOf("\r\n\r\n");
    String body = "";
    
    if (headerEnd >= 0) {
      body = response.substring(headerEnd + 4);
      
      // Log de headers para depuración
      String headers = response.substring(0, headerEnd);
      int statusLineEnd = headers.indexOf("\r\n");
      if (statusLineEnd > 0) {
        Serial.print("[API-READ] Status: ");
        Serial.println(headers.substring(0, statusLineEnd));
      }
      
    } else {
      // Si no se encontró separador de headers, asumir que toda la respuesta es el body
      body = response;
    }
    
    body.trim();
    
    // Si es chunked, decodificar
    if (isChunked && body.length() > 0) {
      Serial.println("[API-READ] Decodificando respuesta chunked...");
      body = decodeChunkedResponse(body);
      Serial.print("[API-READ] Tamaño después de decodificar: ");
      Serial.println(body.length());
    }
    
    // Extraer JSON del cuerpo
    if (body.length() > 0) {
      int jsonStart = body.indexOf("{");
      if (jsonStart >= 0) {
        int jsonEnd = body.lastIndexOf("}");
        if (jsonEnd > jsonStart) {
          String jsonBody = body.substring(jsonStart, jsonEnd + 1);
          Serial.print("[API-READ] JSON extraído (");
          Serial.print(jsonBody.length());
          Serial.println(" bytes)");
          return jsonBody;
        }
      }
      
      // Si no es JSON, devolver el body completo (puede ser texto plano)
      Serial.println("[API-READ] Respuesta no es JSON, devolviendo body completo");
      return body;
    }
    
    Serial.println("[API-READ] ERROR: No se pudo extraer cuerpo de la respuesta");
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

