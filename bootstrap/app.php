<?php

use Illuminate\Foundation\Application;
use Illuminate\Foundation\Configuration\Exceptions;
use Illuminate\Foundation\Configuration\Middleware;

return Application::configure(basePath: dirname(__DIR__))
    ->withRouting(
        web: __DIR__.'/../routes/web.php',
        api: __DIR__.'/../routes/api.php',
        apiPrefix: 'api',
        commands: __DIR__.'/../routes/console.php',
        health: '/up',
    )
    ->withMiddleware(function (Middleware $middleware) {
        // Laravel 11 incluye CORS nativo (\Illuminate\Http\Middleware\HandleCors)
        // Se ejecuta automáticamente para las rutas configuradas en config/cors.php
        // NO usamos EnsureFrontendRequestsAreStateful porque no queremos modo SPA con cookies
        
        // Middleware fallback para OPTIONS en caso de que Render intercepte las peticiones
        // Se ejecuta ANTES que cualquier otro middleware para capturar OPTIONS
        $middleware->api(prepend: [
            \App\Http\Middleware\HandleCorsPreflight::class,
        ]);
    })
    ->withExceptions(function (Exceptions $exceptions) {
        // El middleware CORS nativo de Laravel 11 maneja automáticamente los headers CORS
        // incluso en respuestas de error, así que no necesitamos agregar headers manualmente aquí
    })->create();
