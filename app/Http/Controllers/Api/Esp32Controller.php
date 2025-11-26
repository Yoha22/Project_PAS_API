<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Http\Requests\Esp32AccesoRequest;
use App\Http\Requests\Esp32AlarmaRequest;
use App\Http\Requests\Esp32HuellaRequest;
use App\Http\Requests\Esp32RegisterRequest;
use App\Models\Acceso;
use App\Models\Administrador;
use App\Models\Alarma;
use App\Models\DispositivoEsp32;
use App\Models\HuellaTemporal;
use App\Models\Usuario;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Hash;
use Illuminate\Support\Facades\Http;
use Illuminate\Support\Facades\Log;

class Esp32Controller extends Controller
{
    /**
     * Registrar un nuevo dispositivo ESP32
     * POST /api/esp32/register
     */
    public function register(Esp32RegisterRequest $request)
    {
        $validated = $request->validated();

        // Verificar código del administrador
        $administrador = Administrador::where('codigo', $validated['codigo'])->first();

        if (!$administrador) {
            return response()->json([
                'success' => false,
                'error' => 'Código de registro inválido',
            ], 401);
        }

        // Crear dispositivo
        $dispositivo = DispositivoEsp32::create([
            'nombre' => $validated['nombre'],
            'token' => DispositivoEsp32::generarToken(),
            'administrador_id' => $administrador->id,
            'activo' => true,
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo registrado exitosamente',
            'data' => [
                'id' => $dispositivo->id,
                'nombre' => $dispositivo->nombre,
                'token' => $dispositivo->token, // Solo se envía una vez
            ],
        ], 201);
    }

    /**
     * Obtener configuración del dispositivo
     * GET /api/esp32/config
     */
    public function config(Request $request)
    {
        $dispositivo = $request->get('dispositivo');

        return response()->json([
            'success' => true,
            'data' => [
                'id' => $dispositivo->id,
                'nombre' => $dispositivo->nombre,
                'activo' => $dispositivo->activo,
            ],
        ]);
    }

    /**
     * Registrar huella temporal
     * POST /api/esp32/huella
     */
    public function huella(Esp32HuellaRequest $request)
    {
        $validated = $request->validated();

        $huella = HuellaTemporal::create([
            'idHuella' => $validated['idHuella'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Huella temporal registrada exitosamente',
            'data' => $huella,
        ], 201);
    }

    /**
     * Obtener información del usuario por ID de huella
     * GET /api/esp32/usuario/{idHuella}
     */
    public function getUsuarioByHuella(Request $request, int $idHuella)
    {
        $usuario = Usuario::where('huella_digital', $idHuella)->first();

        if (!$usuario) {
            return response()->json([
                'nombre' => '',
                'idUsuario' => 0,
            ]);
        }

        return response()->json([
            'nombre' => $usuario->nombre,
            'idUsuario' => $usuario->id,
        ]);
    }

    /**
     * Obtener clave del usuario por ID
     * GET /api/esp32/usuario/clave/{idUsuario}
     */
    public function getUsuarioClave(Request $request, int $idUsuario)
    {
        $usuario = Usuario::find($idUsuario);

        if (!$usuario) {
            return response()->json([
                'success' => false,
                'error' => 'Usuario no encontrado',
            ], 404);
        }

        if (!$usuario->clave) {
            return response()->json([
                'success' => false,
                'error' => 'Usuario no tiene clave configurada',
            ], 404);
        }

        return response()->json([
            'success' => true,
            'clave' => $usuario->clave,
        ]);
    }

    /**
     * Registrar acceso de usuario
     * POST /api/esp32/acceso
     */
    public function acceso(Esp32AccesoRequest $request)
    {
        $validated = $request->validated();

        $acceso = Acceso::create([
            'usuario_id' => $validated['idUsuario'],
            'fecha_hora' => now(),
            'tipo_acceso' => $validated['tipo_acceso'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Acceso registrado exitosamente',
            'data' => $acceso,
        ], 201);
    }

    /**
     * Registrar alarma
     * POST /api/esp32/alarma
     */
    public function alarma(Esp32AlarmaRequest $request)
    {
        $validated = $request->validated();

        $alarma = Alarma::create([
            'fecha_hora' => now(),
            'descripcion' => $validated['descripcion'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Alarma registrada exitosamente',
            'data' => $alarma,
        ], 201);
    }

    /**
     * Obtener teléfono del administrador
     * GET /api/esp32/admin/telefono
     */
    public function getAdminTelefono(Request $request)
    {
        $dispositivo = $request->get('dispositivo');

        if (!$dispositivo->administrador_id) {
            return response()->json([
                'telefono_admin' => '',
            ]);
        }

        $administrador = Administrador::find($dispositivo->administrador_id);

        if (!$administrador) {
            return response()->json([
                'telefono_admin' => '',
            ]);
        }

        return response()->json([
            'telefono_admin' => $administrador->telefono_admin ?? '',
        ]);
    }

    /**
     * Obtener código/API Key del administrador
     * GET /api/esp32/admin/codigo
     */
    public function getAdminCodigo(Request $request)
    {
        $dispositivo = $request->get('dispositivo');

        if (!$dispositivo->administrador_id) {
            return response()->json([
                'codigo' => '',
            ]);
        }

        $administrador = Administrador::find($dispositivo->administrador_id);

        if (!$administrador) {
            return response()->json([
                'codigo' => '',
            ]);
        }

        return response()->json([
            'codigo' => (string)($administrador->codigo ?? ''),
        ]);
    }

    /**
     * Listar todos los dispositivos (para panel web)
     * GET /api/dispositivos
     */
    public function index(Request $request)
    {
        $dispositivos = DispositivoEsp32::with('administrador')
            ->orderBy('created_at', 'desc')
            ->get();

        return response()->json([
            'success' => true,
            'data' => $dispositivos,
        ]);
    }

    /**
     * Obtener un dispositivo por ID
     * GET /api/dispositivos/{id}
     */
    public function show($id)
    {
        $dispositivo = DispositivoEsp32::with('administrador')->find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        return response()->json([
            'success' => true,
            'data' => $dispositivo,
        ]);
    }

    /**
     * Crear un nuevo dispositivo (desde panel web)
     * POST /api/dispositivos
     */
    public function store(Request $request)
    {
        $validated = $request->validate([
            'nombre' => 'required|string|max:100',
            'codigo' => 'required|string',
        ]);

        // Verificar código del administrador
        $administrador = Administrador::where('codigo', $validated['codigo'])->first();

        if (!$administrador) {
            return response()->json([
                'success' => false,
                'error' => 'Código de registro inválido',
            ], 401);
        }

        $dispositivo = DispositivoEsp32::create([
            'nombre' => $validated['nombre'],
            'token' => DispositivoEsp32::generarToken(),
            'administrador_id' => $administrador->id,
            'activo' => true,
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo creado exitosamente',
            'data' => $dispositivo,
        ], 201);
    }

    /**
     * Actualizar dispositivo
     * PUT /api/dispositivos/{id}
     */
    public function update(Request $request, $id)
    {
        $validated = $request->validate([
            'nombre' => 'required|string|max:100',
        ]);

        $dispositivo = DispositivoEsp32::find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        $dispositivo->update([
            'nombre' => $validated['nombre'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo actualizado exitosamente',
            'data' => $dispositivo,
        ]);
    }

    /**
     * Eliminar dispositivo
     * DELETE /api/dispositivos/{id}
     */
    public function destroy($id)
    {
        $dispositivo = DispositivoEsp32::find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        $dispositivo->delete();

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo eliminado exitosamente',
        ]);
    }

    /**
     * Generar código de registro
     * POST /api/dispositivos/generate-code
     */
    public function generateCode(Request $request)
    {
        $administrador = Administrador::first();

        if (!$administrador || !$administrador->codigo) {
            return response()->json([
                'success' => false,
                'error' => 'No hay administrador con código configurado',
            ], 404);
        }

        return response()->json([
            'success' => true,
            'data' => [
                'codigo' => (string)$administrador->codigo,
            ],
        ]);
    }

    /**
     * Revocar token de dispositivo
     * POST /api/dispositivos/{id}/revoke-token
     */
    public function revokeToken($id)
    {
        $dispositivo = DispositivoEsp32::find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        $dispositivo->update([
            'activo' => false,
            'token' => DispositivoEsp32::generarToken(), // Generar nuevo token (el antiguo ya no funcionará)
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Token revocado exitosamente. El dispositivo quedó inactivo.',
            'data' => $dispositivo,
        ]);
    }

    /**
     * Obtener HTML del portal de configuración del ESP32
     * GET /api/esp32/config-html
     */
    public function getConfigHtml(Request $request)
    {
        Log::info('[ESP32-PROXY] getConfigHtml iniciado', [
            'ip' => $request->input('ip', '192.168.4.1'),
        ]);

        $esp32Ip = $request->input('ip', '192.168.4.1');
        $url = "http://{$esp32Ip}/";
        $timeout = 10; // segundos

        try {
            Log::info('[ESP32-PROXY] Intentando conectar a ESP32', ['url' => $url]);

            $response = Http::timeout($timeout)
                ->connectTimeout(5)
                ->withOptions(['verify' => false])
                ->withHeaders([
                    'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
                ])
                ->get($url);

            if ($response->successful()) {
                $html = $response->body();

                Log::info('[ESP32-PROXY] Respuesta recibida', [
                    'status_code' => $response->status(),
                    'html_length' => strlen($html),
                ]);

                return response()->json([
                    'success' => true,
                    'html' => $html,
                    'ip' => $esp32Ip,
                ], 200);
            } else {
                throw new \Exception("HTTP {$response->status()}: {$response->body()}");
            }

        } catch (\Illuminate\Http\Client\ConnectionException $e) {
            Log::error('[ESP32-PROXY] Error de conexión', [
                'url' => $url,
                'error' => $e->getMessage(),
            ]);

            return response()->json([
                'success' => false,
                'error' => 'No se pudo conectar con el ESP32. Verifica que esté en modo configuración y accesible en ' . $esp32Ip,
                'details' => $e->getMessage(),
            ], 503);

        } catch (\Exception $e) {
            Log::error('[ESP32-PROXY] Error inesperado', [
                'url' => $url,
                'error' => $e->getMessage(),
                'trace' => $e->getTraceAsString(),
            ]);

            return response()->json([
                'success' => false,
                'error' => 'Error al obtener HTML del ESP32',
                'details' => $e->getMessage(),
            ], 500);
        }
    }

    /**
     * Verificar configuración del túnel (endpoint de diagnóstico)
     * GET /api/esp32-proxy/tunnel-status
     */
    public function getTunnelStatus()
    {
        $tunnelUrl = env('ESP32_TUNNEL_URL');
        $isConfigured = !empty($tunnelUrl) && filter_var($tunnelUrl, FILTER_VALIDATE_URL);
        
        return response()->json([
            'tunnel_configured' => $isConfigured,
            'tunnel_url' => $isConfigured ? $tunnelUrl : null,
            'message' => $isConfigured 
                ? 'Túnel configurado correctamente' 
                : 'Túnel no configurado. Usando IP local.',
        ]);
    }

    /**
     * Proxy para registrar huella en el ESP32
     * GET /api/esp32-proxy/registrar-huella
     */
    public function proxyRegistrarHuella(Request $request)
    {
        Log::info('[ESP32-PROXY] proxyRegistrarHuella iniciado', [
            'ip' => $request->input('ip'),
            'tunnel_url_configured' => !empty(env('ESP32_TUNNEL_URL')),
        ]);

        $validated = $request->validate([
            'ip' => 'required|string|ip',
        ]);

        $esp32Ip = $validated['ip'];
        
        // Verificar si hay una URL de túnel configurada (ngrok, Cloudflare Tunnel, etc.)
        $tunnelUrl = env('ESP32_TUNNEL_URL');
        
        if ($tunnelUrl && filter_var($tunnelUrl, FILTER_VALIDATE_URL)) {
            // Usar túnel si está configurado
            $url = rtrim($tunnelUrl, '/') . "/registrarHuella";
            Log::info('[ESP32-PROXY] Usando túnel para conectar al ESP32', [
                'tunnel_url' => $tunnelUrl,
                'final_url' => $url
            ]);
        } else {
            // Usar IP local directa
            $url = "http://{$esp32Ip}/registrarHuella";
            Log::info('[ESP32-PROXY] Usando IP local para conectar al ESP32', [
                'ip' => $esp32Ip,
                'final_url' => $url
            ]);
        }
        
        $timeout = 35; // segundos (tiempo suficiente para capturar huella)

        try {
            Log::info('[ESP32-PROXY] Intentando conectar al ESP32 para registrar huella', ['url' => $url]);

            $response = Http::timeout($timeout)
                ->connectTimeout(10)
                ->withOptions(['verify' => false])
                ->withHeaders([
                    'Accept' => 'text/plain, application/json, */*',
                ])
                ->get($url);

            $statusCode = $response->status();
            $responseBody = $response->body();

            Log::info('[ESP32-PROXY] Respuesta del ESP32 (registrar huella)', [
                'status_code' => $statusCode,
                'response_length' => strlen($responseBody),
            ]);

            if ($response->successful()) {
                // Intentar parsear como JSON primero
                $jsonResponse = json_decode($responseBody, true);
                if (json_last_error() === JSON_ERROR_NONE) {
                    return response()->json($jsonResponse, $statusCode);
                }

                // Si no es JSON, devolver el texto (es el ID de la huella)
                return response()->json([
                    'success' => true,
                    'idHuella' => trim($responseBody),
                    'raw' => trim($responseBody),
                ], $statusCode);
            } else {
                throw new \Exception("HTTP {$statusCode}: {$responseBody}");
            }

        } catch (\Illuminate\Http\Client\ConnectionException $e) {
            Log::error('[ESP32-PROXY] Error de conexión al registrar huella', [
                'url' => $url,
                'error' => $e->getMessage(),
            ]);
            return response()->json([
                'success' => false,
                'error' => 'No se pudo conectar con el ESP32. Verifica que esté encendido y accesible en ' . $esp32Ip,
                'details' => $e->getMessage(),
            ], 503);
        } catch (\Illuminate\Http\Client\RequestException $e) {
            Log::error('[ESP32-PROXY] Error de petición HTTP al registrar huella', [
                'url' => $url,
                'status_code' => $e->response ? $e->response->status() : 'N/A',
                'response_body' => $e->response ? $e->response->body() : 'N/A',
                'error' => $e->getMessage(),
            ]);
            return response()->json([
                'success' => false,
                'error' => 'Error en la respuesta del ESP32: ' . ($e->response ? $e->response->status() : 'Desconocido'),
                'details' => $e->getMessage(),
            ], 500);
        } catch (\Throwable $e) {
            Log::error('[ESP32-PROXY] Error inesperado al registrar huella', [
                'url' => $url,
                'error' => $e->getMessage(),
                'file' => $e->getFile(),
                'line' => $e->getLine(),
            ]);
            return response()->json([
                'success' => false,
                'error' => 'Ocurrió un error inesperado al intentar registrar la huella.',
                'details' => $e->getMessage(),
            ], 500);
        }
    }

    /**
     * Enviar configuración al ESP32
     * POST /api/esp32/config
     */
    public function postConfig(Request $request)
    {
        Log::info('[ESP32-PROXY] postConfig iniciado', [
            'data' => $request->except(['password']), // No loguear password
        ]);

        $validated = $request->validate([
            'ssid' => 'required|string|max:100',
            'password' => 'required|string|max:100',
            'serverUrl' => 'required|string|url|max:255',
            'useHTTPS' => 'boolean',
            'codigo' => 'required|string|max:50',
            'nombre' => 'required|string|max:100',
            'ip' => 'string|ip', // IP del ESP32
        ]);

        $esp32Ip = $request->input('ip', '192.168.4.1');
        $url = "http://{$esp32Ip}/config";
        $timeout = 30; // segundos (más tiempo para procesar configuración)

        try {
            $formData = [
                'ssid' => $validated['ssid'],
                'password' => $validated['password'],
                'serverUrl' => $validated['serverUrl'],
                'useHTTPS' => $request->input('useHTTPS', '0'),
                'codigo' => $validated['codigo'],
                'nombre' => $validated['nombre'],
            ];

            Log::info('[ESP32-PROXY] Enviando configuración al ESP32', [
                'url' => $url,
                'data' => array_merge($formData, ['password' => '***']),
            ]);

            $response = Http::timeout($timeout)
                ->connectTimeout(10)
                ->withOptions(['verify' => false])
                ->asForm()
                ->post($url, $formData);

            $statusCode = $response->status();
            $responseBody = $response->body();

            Log::info('[ESP32-PROXY] Respuesta del ESP32', [
                'status_code' => $statusCode,
                'response' => $responseBody,
            ]);

            if ($response->successful()) {
                // Intentar parsear como JSON
                $jsonResponse = json_decode($responseBody, true);
                if (json_last_error() === JSON_ERROR_NONE) {
                    return response()->json($jsonResponse, $statusCode);
                }

                // Si no es JSON, retornar como texto
                return response()->json([
                    'success' => true,
                    'message' => $responseBody,
                ], $statusCode);
            } else {
                throw new \Exception("HTTP {$statusCode}: {$responseBody}");
            }

        } catch (\Illuminate\Http\Client\ConnectionException $e) {
            Log::error('[ESP32-PROXY] Error de conexión al enviar configuración', [
                'url' => $url,
                'error' => $e->getMessage(),
            ]);

            return response()->json([
                'success' => false,
                'error' => 'No se pudo conectar con el ESP32. Verifica que esté en modo configuración.',
                'details' => $e->getMessage(),
            ], 503);

        } catch (\Exception $e) {
            Log::error('[ESP32-PROXY] Error inesperado al enviar configuración', [
                'url' => $url,
                'error' => $e->getMessage(),
                'trace' => $e->getTraceAsString(),
            ]);

            return response()->json([
                'success' => false,
                'error' => 'Error al enviar configuración al ESP32',
                'details' => $e->getMessage(),
            ], 500);
        }
    }

    /**
     * Activar modo configuración en el ESP32
     * POST /api/esp32/activate-config-mode
     */
    public function activateConfigMode(Request $request)
    {
        Log::info('[ESP32-PROXY] activateConfigMode iniciado', [
            'ip' => $request->input('ip'),
        ]);

        $validated = $request->validate([
            'ip' => 'required|string|ip', // IP actual del ESP32 (no en modo config)
        ]);

        $esp32Ip = $validated['ip'];
        $url = "http://{$esp32Ip}/reconfig";
        $timeout = 10;

        try {
            Log::info('[ESP32-PROXY] Intentando activar modo configuración', ['url' => $url]);

            $response = Http::timeout($timeout)
                ->connectTimeout(5)
                ->withOptions(['verify' => false])
                ->post($url);

            $statusCode = $response->status();
            $responseBody = $response->body();

            Log::info('[ESP32-PROXY] Respuesta de activación', [
                'status_code' => $statusCode,
                'response' => $responseBody,
            ]);

            if ($response->successful()) {
                $jsonResponse = json_decode($responseBody, true);
                if (json_last_error() === JSON_ERROR_NONE) {
                    return response()->json($jsonResponse, $statusCode);
                }

                return response()->json([
                    'success' => true,
                    'message' => $responseBody,
                ], $statusCode);
            } else {
                throw new \Exception("HTTP {$statusCode}: {$responseBody}");
            }

        } catch (\Illuminate\Http\Client\ConnectionException $e) {
            Log::error('[ESP32-PROXY] Error de conexión al activar modo config', [
                'url' => $url,
                'error' => $e->getMessage(),
            ]);

            return response()->json([
                'success' => false,
                'error' => 'No se pudo conectar con el ESP32. Verifica que esté encendido y en la misma red.',
                'details' => $e->getMessage(),
            ], 503);

        } catch (\Exception $e) {
            Log::error('[ESP32-PROXY] Error al activar modo configuración', [
                'url' => $url,
                'error' => $e->getMessage(),
            ]);

            return response()->json([
                'success' => false,
                'error' => 'Error al activar modo configuración. El ESP32 puede estar ya en modo configuración o no ser accesible.',
                'details' => $e->getMessage(),
            ], 500);
        }
    }
}
