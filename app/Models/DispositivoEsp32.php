<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Support\Str;

class DispositivoEsp32 extends Model
{
    protected $table = 'dispositivos_esp32';

    protected $fillable = [
        'nombre',
        'token',
        'ip_local',
        'ultima_conexion',
        'activo',
        'administrador_id',
    ];

    protected $casts = [
        'ultima_conexion' => 'datetime',
        'activo' => 'boolean',
    ];

    // Token no se oculta por defecto (se necesita mostrar al crear)
    // Se puede ocultar manualmente cuando sea necesario

    /**
     * Relación con Administrador
     */
    public function administrador(): BelongsTo
    {
        return $this->belongsTo(Administrador::class, 'administrador_id');
    }

    /**
     * Generar un token único para el dispositivo
     */
    public static function generarToken(): string
    {
        do {
            $token = Str::random(64);
        } while (self::where('token', $token)->exists());

        return $token;
    }

    /**
     * Actualizar última conexión
     */
    public function actualizarConexion(?string $ip = null): void
    {
        $this->update([
            'ultima_conexion' => now(),
            'ip_local' => $ip ?? $this->ip_local,
        ]);
    }
}
