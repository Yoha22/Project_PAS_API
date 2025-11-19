<?php

namespace App\Http\Middleware;

use Closure;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Log;
use Symfony\Component\HttpFoundation\Response;

class HandleCors
{
    /**
     * Obtener el origen permitido desde la configuración
     */
    private function getAllowedOrigin(): string
    {
        return env('FRONTEND_URL', 'https://sistema-acceso-frontend.onrender.com');
    }

    /**
     * Agregar headers CORS a una respuesta
     */
    private function addCorsHeaders(Response $response): Response
    {
        $origin = $this->getAllowedOrigin();
        
        $response->headers->set('Access-Control-Allow-Origin', $origin);
        $response->headers->set('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH');
        $response->headers->set('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept, Origin');
        $response->headers->set('Access-Control-Allow-Credentials', 'true');
        $response->headers->set('Access-Control-Max-Age', '86400');
        
        return $response;
    }

    /**
     * Handle an incoming request.
     *
     * @param  \Closure(\Illuminate\Http\Request): (\Symfony\Component\HttpFoundation\Response)  $next
     */
    public function handle(Request $request, Closure $next): Response
    {
        $origin = $request->header('Origin');
        $method = $request->getMethod();
        
        // Logging temporal para debugging
        Log::info('CORS Middleware', [
            'method' => $method,
            'path' => $request->path(),
            'origin' => $origin,
            'allowed_origin' => $this->getAllowedOrigin(),
        ]);

        // Manejar preflight OPTIONS requests
        if ($method === 'OPTIONS') {
            Log::info('CORS: Handling preflight OPTIONS request');
            return response('', 200)
                ->header('Access-Control-Allow-Origin', $this->getAllowedOrigin())
                ->header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH')
                ->header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept, Origin')
                ->header('Access-Control-Allow-Credentials', 'true')
                ->header('Access-Control-Max-Age', '86400');
        }

        try {
            // Procesar la petición
            $response = $next($request);
            
            // Agregar headers CORS a la respuesta exitosa
            $this->addCorsHeaders($response);
            
            Log::info('CORS: Headers agregados a respuesta exitosa', [
                'status' => $response->getStatusCode(),
            ]);
            
            return $response;
        } catch (\Exception $e) {
            // Capturar excepciones y asegurar que los headers CORS se agreguen incluso en errores
            Log::error('CORS: Excepción capturada, agregando headers CORS', [
                'exception' => get_class($e),
                'message' => $e->getMessage(),
            ]);
            
            // Intentar obtener la respuesta del handler de excepciones de Laravel
            // Si no hay respuesta, crear una respuesta de error con headers CORS
            $statusCode = method_exists($e, 'getStatusCode') ? $e->getStatusCode() : 500;
            $response = response()->json([
                'success' => false,
                'message' => $e->getMessage(),
            ], $statusCode);
            
            // Agregar headers CORS a la respuesta de error
            $this->addCorsHeaders($response);
            
            return $response;
        }
    }
}

