<?php

use App\Http\Controllers\Api\AuthController;
use App\Http\Controllers\Api\Esp32Controller;
use App\Http\Middleware\AuthenticateEsp32;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;

// Rutas de autenticación (públicas)
Route::prefix('auth')->group(function () {
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
        Route::post('/acceso', [Esp32Controller::class, 'acceso']);
        Route::post('/alarma', [Esp32Controller::class, 'alarma']);
        Route::get('/admin/telefono', [Esp32Controller::class, 'getAdminTelefono']);
        Route::get('/admin/codigo', [Esp32Controller::class, 'getAdminCodigo']);
    });
});

