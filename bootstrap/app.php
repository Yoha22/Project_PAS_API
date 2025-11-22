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
        // CORS debe ejecutarse PRIMERO, antes que cualquier otro middleware
        // Esto es crítico para que las peticiones OPTIONS (preflight) se manejen correctamente
        $middleware->priority([
            \App\Http\Middleware\HandleCors::class,
        ]);
        
        $middleware->api(prepend: [
            \App\Http\Middleware\HandleCors::class,
            \Laravel\Sanctum\Http\Middleware\EnsureFrontendRequestsAreStateful::class,
        ]);
    })
    ->withExceptions(function (Exceptions $exceptions) {
        // Asegurar que los headers CORS se agreguen incluso en errores no capturados
        $exceptions->render(function (\Throwable $e, \Illuminate\Http\Request $request) {
            // Solo para rutas API
            if ($request->is('api/*')) {
                $statusCode = method_exists($e, 'getStatusCode') ? $e->getStatusCode() : 500;
                
                $response = response()->json([
                    'success' => false,
                    'message' => $e->getMessage(),
                ], $statusCode);
                
                // Agregar headers CORS - NUNCA usar '*' cuando credentials es true
                $origin = $request->header('Origin');
                $frontendUrl = config('app.frontend_url', 'https://sistema-acceso-frontend.onrender.com');
                
                // Determinar el origen a usar
                $originToUse = $frontendUrl; // Por defecto
                
                if ($origin) {
                    $originHost = parse_url($origin, PHP_URL_HOST);
                    $frontendHost = parse_url($frontendUrl, PHP_URL_HOST);
                    
                    // Si el origen coincide con el frontend configurado o es de Render, usarlo
                    if ($originHost && $frontendHost && $originHost === $frontendHost) {
                        $originToUse = $origin;
                    } elseif (str_contains($origin, 'onrender.com')) {
                        // Permitir cualquier origen de Render
                        $originToUse = $origin;
                    }
                }
                
                // CRÍTICO: Nunca usar '*' cuando Access-Control-Allow-Credentials es 'true'
                $response->headers->set('Access-Control-Allow-Origin', $originToUse);
                $response->headers->set('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH');
                $response->headers->set('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept, Origin, X-XSRF-TOKEN');
                $response->headers->set('Access-Control-Allow-Credentials', 'true');
                $response->headers->set('Access-Control-Max-Age', '86400');
                $response->headers->set('Vary', 'Origin');
                
                return $response;
            }
        });
    })->create();
