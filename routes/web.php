<?php

use Illuminate\Support\Facades\Route;

Route::get('/', function () {
    return response()->json([
        'message' => 'Sistema de Acceso API',
        'version' => '1.0.0',
        'endpoints' => [
            'auth' => '/api/auth',
            'usuarios' => '/api/usuarios',
            'accesos' => '/api/accesos',
            'alarmas' => '/api/alarmas',
        ],
    ]);
});
