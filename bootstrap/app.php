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
                
                // Agregar headers CORS
                $origin = $request->header('Origin');
                $frontendUrl = config('app.frontend_url', env('FRONTEND_URL', 'https://sistema-acceso-frontend.onrender.com'));
                
                if ($origin && (str_contains($frontendUrl, parse_url($origin, PHP_URL_HOST) ?? ''))) {
                    $response->headers->set('Access-Control-Allow-Origin', $origin);
                } else {
                    $response->headers->set('Access-Control-Allow-Origin', $frontendUrl);
                }
                
                $response->headers->set('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH');
                $response->headers->set('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept, Origin');
                $response->headers->set('Access-Control-Allow-Credentials', 'true');
                
                return $response;
            }
        });
    })->create();
