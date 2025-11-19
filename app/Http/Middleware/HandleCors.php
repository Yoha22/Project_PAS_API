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
            // Permitir peticiones sin origen (por ejemplo, Postman, curl)
            return true;
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

        // Permitir orígenes de Render (para debugging)
        if (str_contains($origin, 'onrender.com')) {
            return true;
        }

        return false;
    }

    /**
     * Obtener el origen a usar en los headers CORS
     */
    private function getOriginForHeaders(?string $requestOrigin): ?string
    {
        // Si hay un origen en la petición, verificar si está permitido
        if ($requestOrigin) {
            // Si está permitido, usarlo
            if ($this->isOriginAllowed($requestOrigin)) {
                return $requestOrigin;
            }
            
            // Si es de Render, permitirlo (para debugging y compatibilidad)
            if (str_contains($requestOrigin, 'onrender.com')) {
                return $requestOrigin;
            }
        }

        // Si no hay origen o no está permitido, usar el primero de los permitidos
        $allowedOrigins = $this->getAllowedOrigins();
        return $allowedOrigins[0] ?? null;
    }

    /**
     * Agregar headers CORS a una respuesta
     */
    private function addCorsHeaders(Response $response, ?string $requestOrigin = null): Response
    {
        // Determinar el origen a usar en los headers
        $originToUse = null;
        
        if ($requestOrigin) {
            // Si hay un origen en la petición y está permitido, usarlo
            if ($this->isOriginAllowed($requestOrigin)) {
                $originToUse = $requestOrigin;
            } elseif (str_contains($requestOrigin, 'onrender.com')) {
                // Permitir orígenes de Render
                $originToUse = $requestOrigin;
            }
        }
        
        // Si no se determinó un origen, usar el configurado por defecto
        if (!$originToUse) {
            $allowedOrigins = $this->getAllowedOrigins();
            $originToUse = $allowedOrigins[0] ?? 'https://sistema-acceso-frontend.onrender.com';
        }
        
        // Establecer todos los headers CORS
        $response->headers->set('Access-Control-Allow-Origin', $originToUse);
        $response->headers->set('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH');
        $response->headers->set('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept, Origin, X-XSRF-TOKEN');
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

        // Manejar preflight OPTIONS requests - DEBE ser lo primero
        if ($method === 'OPTIONS') {
            Log::info('CORS: Handling preflight OPTIONS request', [
                'origin' => $origin,
                'requested_method' => $request->header('Access-Control-Request-Method'),
                'requested_headers' => $request->header('Access-Control-Request-Headers'),
            ]);
            
            $response = response('', 200);
            
            // Establecer headers CORS para preflight
            if ($origin && $this->isOriginAllowed($origin)) {
                $response->headers->set('Access-Control-Allow-Origin', $origin);
            } else {
                $allowedOrigins = $this->getAllowedOrigins();
                $response->headers->set('Access-Control-Allow-Origin', $allowedOrigins[0] ?? '*');
            }
            
            $response->headers->set('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS, PATCH');
            $response->headers->set('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With, Accept, Origin, X-XSRF-TOKEN');
            $response->headers->set('Access-Control-Allow-Credentials', 'true');
            $response->headers->set('Access-Control-Max-Age', '86400');
            
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

