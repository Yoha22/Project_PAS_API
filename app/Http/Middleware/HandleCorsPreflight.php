<?php

namespace App\Http\Middleware;

use Closure;
use Illuminate\Http\Request;
use Symfony\Component\HttpFoundation\Response;

/**
 * Middleware fallback para manejar peticiones OPTIONS (preflight) en Render
 * cuando el middleware CORS nativo de Laravel no las captura correctamente
 */
class HandleCorsPreflight
{
    /**
     * Handle an incoming request.
     *
     * @param  \Closure(\Illuminate\Http\Request): (\Symfony\Component\HttpFoundation\Response)  $next
     */
    public function handle(Request $request, Closure $next): Response
    {
        // Si es una petición OPTIONS (preflight), responder inmediatamente
        if ($request->getMethod() === 'OPTIONS') {
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
            
            // Si no hay origen o no está permitido, usar el primero de la lista
            $originToUse = $isOriginAllowed ? $origin : ($allowedOrigins[0] ?? '*');
            
            $allowedMethods = config('cors.allowed_methods', ['*']);
            $allowedHeaders = config('cors.allowed_headers', ['*']);
            
            // Si es '*', usar el valor directamente, sino convertir array a string
            $methodsHeader = in_array('*', $allowedMethods) ? '*' : implode(', ', $allowedMethods);
            $headersHeader = in_array('*', $allowedHeaders) ? '*' : implode(', ', $allowedHeaders);
            
            return response('', 204)
                ->header('Access-Control-Allow-Origin', $originToUse)
                ->header('Access-Control-Allow-Methods', $methodsHeader)
                ->header('Access-Control-Allow-Headers', $headersHeader)
                ->header('Access-Control-Allow-Credentials', config('cors.supports_credentials', false) ? 'true' : 'false')
                ->header('Access-Control-Max-Age', (string) config('cors.max_age', 0))
                ->header('Vary', 'Origin');
        }

        return $next($request);
    }
}

