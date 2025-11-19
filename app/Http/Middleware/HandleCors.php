<?php

namespace App\Http\Middleware;

use Closure;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Log;
use Symfony\Component\HttpFoundation\Response;

class HandleCors
{
    /**
     * Obtener orígenes permitidos desde la configuración
     */
    private function getAllowedOrigins(): array
    {
        $frontendUrl = config('app.frontend_url', env('FRONTEND_URL', 'https://sistema-acceso-frontend.onrender.com'));
        
        // Convertir a array si es string
        if (is_string($frontendUrl)) {
            $frontendUrl = explode(',', $frontendUrl);
        }
        
        // Limpiar espacios en blanco
        return array_map('trim', $frontendUrl);
    }

    /**
     * Verificar si el origen está permitido
     */
    private function isOriginAllowed(?string $origin): bool
    {
        if (!$origin) {
            return false;
        }

        $allowedOrigins = $this->getAllowedOrigins();
        
        // Verificar coincidencia exacta
        if (in_array($origin, $allowedOrigins)) {
            return true;
        }

        // Verificar coincidencia por dominio (sin protocolo)
        foreach ($allowedOrigins as $allowed) {
            $allowedDomain = parse_url($allowed, PHP_URL_HOST);
            $originDomain = parse_url($origin, PHP_URL_HOST);
            
            if ($allowedDomain && $originDomain && $allowedDomain === $originDomain) {
                return true;
            }
        }

        return false;
    }

    /**
     * Obtener el origen a usar en los headers CORS
     */
    private function getOriginForHeaders(?string $requestOrigin): ?string
    {
        if (!$requestOrigin) {
            $allowedOrigins = $this->getAllowedOrigins();
            return $allowedOrigins[0] ?? null;
        }

        if ($this->isOriginAllowed($requestOrigin)) {
            return $requestOrigin;
        }

        $allowedOrigins = $this->getAllowedOrigins();
        return $allowedOrigins[0] ?? null;
    }

    /**
     * Agregar headers CORS a una respuesta
     */
    private function addCorsHeaders(Response $response, ?string $requestOrigin = null): Response
    {
        $origin = $this->getOriginForHeaders($requestOrigin);
        
        if ($origin) {
            $response->headers->set('Access-Control-Allow-Origin', $origin);
        }
        
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
        
        // Logging para debugging
        Log::info('CORS Middleware', [
            'method' => $method,
            'path' => $request->path(),
            'origin' => $origin,
            'allowed_origins' => $this->getAllowedOrigins(),
            'is_allowed' => $this->isOriginAllowed($origin),
        ]);

        // Manejar preflight OPTIONS requests
        if ($method === 'OPTIONS') {
            Log::info('CORS: Handling preflight OPTIONS request');
            
            $response = response('', 200);
            $this->addCorsHeaders($response, $origin);
            
            return $response;
        }

        try {
            // Procesar la petición
            $response = $next($request);
            
            // Agregar headers CORS a la respuesta exitosa
            $this->addCorsHeaders($response, $origin);
            
            Log::info('CORS: Headers agregados a respuesta exitosa', [
                'status' => $response->getStatusCode(),
                'origin' => $origin,
            ]);
            
            return $response;
        } catch (\Exception $e) {
            // Capturar excepciones y asegurar que los headers CORS se agreguen incluso en errores
            Log::error('CORS: Excepción capturada, agregando headers CORS', [
                'exception' => get_class($e),
                'message' => $e->getMessage(),
                'origin' => $origin,
            ]);
            
            // Intentar obtener la respuesta del handler de excepciones de Laravel
            // Si no hay respuesta, crear una respuesta de error con headers CORS
            $statusCode = method_exists($e, 'getStatusCode') ? $e->getStatusCode() : 500;
            $response = response()->json([
                'success' => false,
                'message' => $e->getMessage(),
            ], $statusCode);
            
            // Agregar headers CORS a la respuesta de error
            $this->addCorsHeaders($response, $origin);
            
            return $response;
        }
    }
}

