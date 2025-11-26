<?php

use App\Http\Controllers\Api\AuthController;
use App\Http\Controllers\Api\Esp32Controller;
use App\Http\Middleware\AuthenticateEsp32;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;

// Ruta OPTIONS global para todas las rutas API - debe estar PRIMERO
// Esto asegura que las peticiones preflight OPTIONS se manejen antes que cualquier otra ruta
Route::options('/{any}', function (Request $request) {
    $origin = $request->header('Origin');
    $allowedOrigins = config('cors.allowed_origins', []);
    
    // Verificar si el origen está permitido
    $isOriginAllowed = false;
    if ($origin) {
        foreach ($allowedOrigins as $allowedOrigin) {
            if ($origin === $allowedOrigin || 
                parse_url($origin, PHP_URL_HOST) === parse_url($allowedOrigin, PHP_URL_HOST)) {
                $isOriginAllowed = true;
                break;
            }
        }
    }
    
    // Usar el origen permitido o el primero de la lista
    $originToUse = $isOriginAllowed ? $origin : ($allowedOrigins[0] ?? '*');
    
    return response('', 204)
        ->header('Access-Control-Allow-Origin', $originToUse)
        ->header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH')
        ->header('Access-Control-Allow-Headers', '*')
        ->header('Access-Control-Allow-Credentials', 'false')
        ->header('Access-Control-Max-Age', '0')
        ->header('Vary', 'Origin');
})->where('any', '.*');

// Ruta de prueba simple (pública) - responde rápido para verificar que el servidor está activo
Route::get('/ping', function (Request $request) {
    return response()->json([
        'success' => true,
        'message' => 'Servidor activo',
        'timestamp' => now()->toIso8601String(),
        'origin' => $request->header('Origin'),
    ]);
});

// Ruta de health check (pública) - responde muy rápido sin consultas a BD
Route::get('/health', function (Request $request) {
    return response()->json([
        'success' => true,
        'status' => 'online',
        'timestamp' => now()->toIso8601String(),
        'service' => 'Project PAS API',
        'version' => '1.0.0',
    ], 200);
});

// Ruta de diagnóstico CORS (pública)
Route::get('/cors-test', function (Request $request) {
    $origin = $request->header('Origin');
    $frontendUrl = config('app.frontend_url', env('FRONTEND_URL', 'https://sistema-acceso-frontend.onrender.com'));
    
    return response()->json([
        'success' => true,
        'message' => 'CORS Test',
        'cors_info' => [
            'request_origin' => $origin,
            'configured_frontend_url' => $frontendUrl,
            'origin_allowed' => $origin && (
                $origin === $frontendUrl || 
                str_contains($origin, parse_url($frontendUrl, PHP_URL_HOST) ?? '')
            ),
            'headers_sent' => [
                'Access-Control-Allow-Origin' => $origin ?: $frontendUrl,
                'Access-Control-Allow-Methods' => 'GET, POST, PUT, DELETE, OPTIONS, PATCH',
                'Access-Control-Allow-Headers' => 'Content-Type, Authorization, X-Requested-With, Accept, Origin',
                'Access-Control-Allow-Credentials' => 'false',
            ],
        ],
        'request_info' => [
            'method' => $request->method(),
            'path' => $request->path(),
            'full_url' => $request->fullUrl(),
            'headers' => [
                'Origin' => $request->header('Origin'),
                'Access-Control-Request-Method' => $request->header('Access-Control-Request-Method'),
                'Access-Control-Request-Headers' => $request->header('Access-Control-Request-Headers'),
            ],
        ],
    ]);
});

// Ruta de depuración (pública)
Route::get('/debug/auth', function (Request $request) {
    $token = $request->bearerToken() ?? $request->header('Authorization');
    
    return response()->json([
        'success' => true,
        'debug_info' => [
            'has_bearer_token' => $request->bearerToken() !== null,
            'bearer_token' => $request->bearerToken() ? substr($request->bearerToken(), 0, 20) . '...' : null,
            'authorization_header' => $request->header('Authorization') ? substr($request->header('Authorization'), 0, 30) . '...' : null,
            'user_authenticated' => $request->user() !== null,
            'user_id' => $request->user()?->id,
            'user_email' => $request->user()?->correo ?? null,
            'auth_guard' => auth()->getDefaultDriver(),
            'sanctum_configured' => class_exists(\Laravel\Sanctum\Sanctum::class),
            'all_headers' => $request->headers->all(),
            'request_method' => $request->method(),
            'request_path' => $request->path(),
            'request_url' => $request->fullUrl(),
        ],
    ]);
});

// Rutas de autenticación (públicas)
Route::prefix('auth')->group(function () {
    // Fallback OPTIONS para preflight - debe estar ANTES de las rutas POST/GET
    Route::options('/{any}', function (Request $request) {
        $origin = $request->header('Origin');
        $allowedOrigins = config('cors.allowed_origins', []);
        
        $isOriginAllowed = false;
        if ($origin) {
            foreach ($allowedOrigins as $allowedOrigin) {
                if ($origin === $allowedOrigin || 
                    parse_url($origin, PHP_URL_HOST) === parse_url($allowedOrigin, PHP_URL_HOST)) {
                    $isOriginAllowed = true;
                    break;
                }
            }
        }
        
        $originToUse = $isOriginAllowed ? $origin : ($allowedOrigins[0] ?? '*');
        
        return response('', 204)
            ->header('Access-Control-Allow-Origin', $originToUse)
            ->header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH')
            ->header('Access-Control-Allow-Headers', '*')
            ->header('Access-Control-Allow-Credentials', 'false')
            ->header('Access-Control-Max-Age', '0')
            ->header('Vary', 'Origin');
    })->where('any', '.*');
    
    Route::get('/check-admin', [AuthController::class, 'checkAdmin']);
    Route::post('/register', [AuthController::class, 'register']);
    Route::post('/login', [AuthController::class, 'login']);
    
    // Rutas protegidas
    Route::middleware('auth:sanctum')->group(function () {
        Route::post('/logout', [AuthController::class, 'logout']);
        Route::get('/me', [AuthController::class, 'me']);
    });
});

// Rutas protegidas
Route::middleware('auth:sanctum')->group(function () {
    Route::get('/user', function (Request $request) {
        return $request->user();
    });
    
    // Ruta de depuración protegida
    Route::get('/debug/auth-protected', function (Request $request) {
        return response()->json([
            'success' => true,
            'message' => 'Ruta protegida accesible',
            'user' => $request->user(),
            'user_id' => $request->user()?->id,
            'user_email' => $request->user()?->correo,
            'token_info' => [
                'has_token' => $request->bearerToken() !== null,
                'token_preview' => $request->bearerToken() ? substr($request->bearerToken(), 0, 20) . '...' : null,
            ],
        ]);
    });
    
    // Administradores
    Route::apiResource('administradores', \App\Http\Controllers\Api\AdminController::class);
    
    // Usuarios
    Route::apiResource('usuarios', \App\Http\Controllers\Api\UsuarioController::class);
    
    // Accesos
    Route::get('accesos/stats', [\App\Http\Controllers\Api\AccesoController::class, 'stats']);
    Route::apiResource('accesos', \App\Http\Controllers\Api\AccesoController::class)->except(['update', 'show', 'destroy']);
    
    // Alarmas
    Route::apiResource('alarmas', \App\Http\Controllers\Api\AlarmaController::class)->except(['show', 'update']);
    
    // Huellas
    Route::post('huellas', [\App\Http\Controllers\Api\HuellaController::class, 'store']);
    Route::get('huellas/temporal', [\App\Http\Controllers\Api\HuellaController::class, 'getTemporal']);
    
    // Dispositivos ESP32 (gestión desde panel web)
    Route::get('dispositivos', [Esp32Controller::class, 'index']);
    Route::post('dispositivos', [Esp32Controller::class, 'store']);
    Route::get('dispositivos/{id}', [Esp32Controller::class, 'show']);
    Route::put('dispositivos/{id}', [Esp32Controller::class, 'update']);
    Route::delete('dispositivos/{id}', [Esp32Controller::class, 'destroy']);
    Route::post('dispositivos/generate-code', [Esp32Controller::class, 'generateCode']);
    Route::post('dispositivos/{id}/revoke-token', [Esp32Controller::class, 'revokeToken']);
    
    // Proxy para configuración ESP32
    Route::get('esp32/config-html', [Esp32Controller::class, 'getConfigHtml']);
    Route::post('esp32/config', [Esp32Controller::class, 'postConfig']);
    Route::post('esp32/activate-config-mode', [Esp32Controller::class, 'activateConfigMode']);
    
    // Proxy para operaciones con huellas
    Route::get('esp32-proxy/registrar-huella', [Esp32Controller::class, 'proxyRegistrarHuella']);
});

// Rutas ESP32 (públicas para registro, protegidas con token de dispositivo para el resto)
Route::prefix('esp32')->group(function () {
    // Registro público (requiere código de administrador)
    Route::post('/register', [Esp32Controller::class, 'register']);
    
    // Rutas protegidas con token de dispositivo
    Route::middleware([AuthenticateEsp32::class])->group(function () {
        Route::get('/config', [Esp32Controller::class, 'config']);
        Route::post('/huella', [Esp32Controller::class, 'huella']);
        Route::get('/usuario/{idHuella}', [Esp32Controller::class, 'getUsuarioByHuella']);
        Route::get('/usuario/clave/{idUsuario}', [Esp32Controller::class, 'getUsuarioClave']);
        Route::post('/acceso', [Esp32Controller::class, 'acceso']);
        Route::post('/alarma', [Esp32Controller::class, 'alarma']);
        Route::get('/admin/telefono', [Esp32Controller::class, 'getAdminTelefono']);
        Route::get('/admin/codigo', [Esp32Controller::class, 'getAdminCodigo']);
    });
});

