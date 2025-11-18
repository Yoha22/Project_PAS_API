<?php

namespace App\Http\Middleware;

use App\Models\DispositivoEsp32;
use Closure;
use Illuminate\Http\Request;
use Symfony\Component\HttpFoundation\Response;

class AuthenticateEsp32
{
    /**
     * Handle an incoming request.
     *
     * @param  \Closure(\Illuminate\Http\Request): (\Symfony\Component\HttpFoundation\Response)  $next
     */
    public function handle(Request $request, Closure $next): Response
    {
        $token = $request->bearerToken() ?? $request->header('X-Device-Token');

        if (!$token) {
            return response()->json([
                'success' => false,
                'error' => 'Token de dispositivo no proporcionado',
            ], 401);
        }

        $dispositivo = DispositivoEsp32::where('token', $token)
            ->where('activo', true)
            ->first();

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Token de dispositivo inválido o dispositivo inactivo',
            ], 401);
        }

        // Actualizar última conexión
        $dispositivo->actualizarConexion($request->ip());

        // Agregar dispositivo a la request para uso en controladores
        $request->merge(['dispositivo' => $dispositivo]);

        return $next($request);
    }
}
