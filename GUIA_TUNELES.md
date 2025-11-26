# Guía: Túneles para Exponer el ESP32 a Internet

## ¿Qué es un túnel?

Un **túnel** es una conexión segura que permite que servicios en tu red local (como el ESP32) sean accesibles desde internet sin abrir puertos en tu router ni exponer tu IP pública.

## ¿Cómo funciona?

```
Internet → Túnel (ngrok/Cloudflare) → Tu Red Local → ESP32 (192.168.18.105:80)
```

El túnel:
1. Crea una conexión segura desde tu red local hacia un servidor en internet
2. Recibe peticiones desde internet en una URL pública
3. Reenvía esas peticiones a tu ESP32 en la red local

---

## Opción 1: ngrok (Más fácil y rápida)

### Ventajas
- ✅ Muy fácil de configurar
- ✅ Ideal para desarrollo y pruebas
- ✅ Interfaz web para inspeccionar peticiones
- ✅ Gratis para uso básico

### Desventajas
- ❌ URLs temporales (cambian cada vez en versión gratuita)
- ❌ Límites en el plan gratuito

### Instalación y Configuración

#### Paso 1: Instalar ngrok

**Windows:**
1. Descarga ngrok desde: https://ngrok.com/download
2. Extrae el archivo `ngrok.exe` a una carpeta (ej: `C:\ngrok\`)
3. Agrega esa carpeta a tu PATH de Windows o úsalo desde ahí

**Mac/Linux:**
```bash
# Mac (Homebrew)
brew install ngrok

# Linux
wget https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-linux-amd64.tgz
tar -xzf ngrok-v3-stable-linux-amd64.tgz
sudo mv ngrok /usr/local/bin/
```

#### Paso 2: Obtener tu authtoken

1. Regístrate en https://dashboard.ngrok.com/signup (gratis)
2. Ve a "Your Authtoken" en el dashboard
3. Copia tu token

#### Paso 3: Configurar ngrok

```bash
ngrok config add-authtoken TU_TOKEN_AQUI
```

#### Paso 4: Iniciar el túnel

Abre una terminal y ejecuta:

```bash
ngrok http 192.168.18.105:80
```

**Nota:** Reemplaza `192.168.18.105` con la IP actual de tu ESP32.

#### Paso 5: Obtener la URL pública

Verás algo como esto:

```
Forwarding    https://abc123.ngrok-free.app -> http://192.168.18.105:80
```

La URL `https://abc123.ngrok-free.app` es tu ESP32 expuesto a internet.

#### Paso 6: Actualizar el backend

En el código del backend (`Esp32Controller.php`), cuando uses el túnel:

```php
// En lugar de usar la IP local
$url = "http://{$esp32Ip}/registrarHuella";

// Usar la URL del túnel
$tunnelUrl = env('ESP32_TUNNEL_URL', ''); // Ej: https://abc123.ngrok-free.app
if ($tunnelUrl) {
    $url = "{$tunnelUrl}/registrarHuella";
} else {
    $url = "http://{$esp32Ip}/registrarHuella";
}
```

O mejor, guardar la URL del túnel en una variable de entorno:

```bash
# En Render.com, agregar variable de entorno:
ESP32_TUNNEL_URL=https://abc123.ngrok-free.app
```

#### Paso 7: Usar la URL del túnel en el frontend

En el frontend, cuando configures la IP del ESP32, en lugar de poner la IP local, pon la URL del túnel:

```javascript
// Guardar la URL del túnel
esp32Config.setIP('https://abc123.ngrok-free.app');
```

---

## Opción 2: Cloudflare Tunnel (Más estable y permanente)

### Ventajas
- ✅ URLs permanentes con tu propio dominio
- ✅ Gratis
- ✅ Integración con Cloudflare (DDOS protection, SSL automático)
- ✅ Más seguro
- ✅ No necesita abrir puertos

### Desventajas
- ❌ Requiere un dominio
- ❌ Configuración inicial más compleja

### Instalación y Configuración

#### Paso 1: Tener un dominio

Necesitas un dominio (puedes comprarlo o usar uno gratuito como Freenom).

#### Paso 2: Configurar Cloudflare

1. Regístrate en https://dash.cloudflare.com/sign-up
2. Agrega tu dominio a Cloudflare
3. Cambia los nameservers de tu dominio a los que Cloudflare te proporcione

#### Paso 3: Instalar cloudflared

**Windows:**
1. Descarga desde: https://github.com/cloudflare/cloudflared/releases
2. Extrae `cloudflared.exe` a una carpeta

**Mac/Linux:**
```bash
# Mac (Homebrew)
brew install cloudflared

# Linux
wget https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64
chmod +x cloudflared-linux-amd64
sudo mv cloudflared-linux-amd64 /usr/local/bin/cloudflared
```

#### Paso 4: Iniciar sesión en Cloudflare

```bash
cloudflared tunnel login
```

Esto abrirá tu navegador para autenticarte.

#### Paso 5: Crear un túnel

```bash
cloudflared tunnel create esp32-tunnel
```

Guarda el **tunnel ID** que se genera.

#### Paso 6: Crear el archivo de configuración

Crea un archivo `config.yml` en la carpeta de cloudflared:

```yaml
tunnel: TU_TUNNEL_ID_AQUI
credentials-file: C:\ruta\a\tu\archivo\credentials.json

ingress:
  - hostname: esp32.tudominio.com
    service: http://192.168.18.105:80
  - service: http_status:404
```

**Nota:** Reemplaza:
- `TU_TUNNEL_ID_AQUI` con el ID del túnel
- `esp32.tudominio.com` con tu subdominio deseado
- `192.168.18.105` con la IP de tu ESP32

#### Paso 7: Configurar DNS

```bash
cloudflared tunnel route dns esp32-tunnel esp32.tudominio.com
```

#### Paso 8: Iniciar el túnel

```bash
cloudflared tunnel --config config.yml run
```

O para ejecutarlo como servicio en Windows:

```bash
cloudflared service install
cloudflared service start
```

#### Paso 9: Usar la URL en tu aplicación

Ahora puedes acceder a tu ESP32 desde:
```
https://esp32.tudominio.com
```

Esta URL será **permanente** y no cambiará.

---

## Opción 3: Usar un servidor intermedio (Recomendado para producción)

En lugar de exponer el ESP32 directamente, puedes hacer que el ESP32 se conecte al backend periódicamente para recibir comandos.

### Arquitectura mejorada:

```
ESP32 → Polling al Backend → Backend responde con comandos
```

**Ventajas:**
- ✅ No necesitas túneles
- ✅ Más seguro (no expones el ESP32)
- ✅ Funciona desde cualquier red
- ✅ El ESP32 inicia la conexión (más confiable)

**Implementación:**

El ESP32 haría polling cada X segundos:

```cpp
void checkForCommands() {
  if (!apiClient) return;
  
  // Polling: El ESP32 pregunta al backend si hay comandos
  String response = apiClient->get("/api/esp32/commands/pending");
  
  // Procesar comandos recibidos
  DynamicJsonDocument doc(512);
  deserializeJson(doc, response);
  
  if (doc["hasCommands"]) {
    // Ejecutar comandos
    processCommandQueue(doc["commands"]);
  }
}
```

Y en el backend, guardarías los comandos en una cola:

```php
// Frontend envía comando
Route::post('/api/esp32/{deviceId}/command', function($deviceId, Request $request) {
    Esp32Command::create([
        'device_id' => $deviceId,
        'command' => $request->command,
        'payload' => $request->payload,
        'status' => 'pending'
    ]);
});

// ESP32 pregunta por comandos
Route::get('/api/esp32/commands/pending', function(Request $request) {
    $commands = Esp32Command::where('device_id', $device->id)
        ->where('status', 'pending')
        ->get();
    
    return response()->json([
        'hasCommands' => $commands->count() > 0,
        'commands' => $commands
    ]);
});
```

---

## Comparación Rápida

| Característica | ngrok | Cloudflare Tunnel | Polling |
|---------------|-------|-------------------|---------|
| **Facilidad** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **URL Permanente** | ❌ (gratis) | ✅ | N/A |
| **Seguridad** | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| **Gratis** | ✅ (limitado) | ✅ | ✅ |
| **Requiere Dominio** | ❌ | ✅ | ❌ |
| **Mejor para** | Desarrollo | Producción | Producción |

---

## Recomendación

Para tu caso específico:

1. **Desarrollo/Testing:** Usa **ngrok** (más rápido de configurar)
2. **Producción con dominio:** Usa **Cloudflare Tunnel** (más estable)
3. **Producción sin dominio:** Implementa **polling** (más robusto)

---

## Ejemplo Práctico con ngrok

### Paso a paso rápido:

```bash
# 1. Instalar ngrok (Windows)
# Descargar y extraer ngrok.exe

# 2. Autenticarse
ngrok config add-authtoken TU_TOKEN

# 3. Iniciar túnel apuntando al ESP32
ngrok http 192.168.18.105:80

# 4. Copiar la URL que aparece (ej: https://abc123.ngrok-free.app)

# 5. En Render.com, agregar variable de entorno:
ESP32_TUNNEL_URL=https://abc123.ngrok-free.app

# 6. Modificar el código del backend para usar esta URL
```

### Modificación en el Backend:

```php
// En app/Http/Controllers/Api/Esp32Controller.php

public function proxyRegistrarHuella(Request $request)
{
    $esp32Ip = $request->input('ip');
    
    // Verificar si hay túnel configurado
    $tunnelUrl = env('ESP32_TUNNEL_URL');
    
    if ($tunnelUrl) {
        // Usar túnel si está configurado
        $url = "{$tunnelUrl}/registrarHuella";
    } else {
        // Usar IP local
        $url = "http://{$esp32Ip}/registrarHuella";
    }
    
    // Resto del código...
}
```

---

## Notas Importantes

1. **Seguridad:** Los túneles exponen tu ESP32 a internet. Asegúrate de tener:
   - Autenticación en los endpoints
   - Rate limiting
   - Validación de peticiones

2. **Persistencia:** Con ngrok gratis, la URL cambia cada vez que reinicias. Considera:
   - Usar ngrok con dominio personalizado (plan de pago)
   - Usar Cloudflare Tunnel (gratis y permanente)
   - Actualizar la URL manualmente cada vez

3. **Rendimiento:** Los túneles añaden latencia. Para producción, considera polling o WebSockets.

---

## Siguiente Paso

¿Quieres que implemente alguna de estas soluciones en tu código? Puedo:
- Modificar el backend para soportar URLs de túnel
- Implementar el sistema de polling
- Crear scripts de configuración automática

