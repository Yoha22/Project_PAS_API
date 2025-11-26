<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Database\Eloquent\Relations\HasMany;
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
        'is_online',
        'connection_type',
        'last_heartbeat',
        'websocket_id',
    ];

    protected $casts = [
        'ultima_conexion' => 'datetime',
        'activo' => 'boolean',
        'is_online' => 'boolean',
        'last_heartbeat' => 'datetime',
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
     * Relación con Comandos ESP32
     */
    public function comandos(): HasMany
    {
        return $this->hasMany(Esp32Command::class, 'dispositivo_id');
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

    /**
     * Actualizar estado de conexión
     */
    public function updateConnectionStatus(bool $isOnline, string $connectionType, ?string $websocketId = null): void
    {
        $this->update([
            'is_online' => $isOnline,
            'connection_type' => $connectionType,
            'last_heartbeat' => $isOnline ? now() : $this->last_heartbeat,
            'websocket_id' => $websocketId ?? $this->websocket_id,
        ]);
    }

    /**
     * Marcar dispositivo como desconectado
     */
    public function markOffline(): void
    {
        $this->update([
            'is_online' => false,
            'connection_type' => 'offline',
            'websocket_id' => null,
        ]);
    }
}
