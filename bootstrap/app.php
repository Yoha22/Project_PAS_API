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
        $middleware->api(prepend: [
            \Laravel\Sanctum\Http\Middleware\EnsureFrontendRequestsAreStateful::class,
        ]);
        
        // Configurar CORS para permitir peticiones desde el frontend
        $middleware->api(append: [
            function ($request, $next) {
                // Manejar preflight OPTIONS requests
                if ($request->getMethod() === 'OPTIONS') {
                    return response('', 200)
                        ->header('Access-Control-Allow-Origin', env('FRONTEND_URL', 'https://sistema-acceso-frontend.onrender.com'))
                        ->header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
                        ->header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept')
                        ->header('Access-Control-Allow-Credentials', 'true')
                        ->header('Access-Control-Max-Age', '86400');
                }
                
                $response = $next($request);
                
                // Agregar headers CORS a todas las respuestas
                $response->headers->set('Access-Control-Allow-Origin', env('FRONTEND_URL', 'https://sistema-acceso-frontend.onrender.com'));
                $response->headers->set('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
                $response->headers->set('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept');
                $response->headers->set('Access-Control-Allow-Credentials', 'true');
                $response->headers->set('Access-Control-Max-Age', '86400');
                
                return $response;
            },
        ]);
    })
    ->withExceptions(function (Exceptions $exceptions) {
        //
    })->create();
