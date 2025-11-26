<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Support\Str;

class Esp32Command extends Model
{
    protected $table = 'esp32_commands';

    protected $fillable = [
        'dispositivo_id',
        'command',
        'payload',
        'status',
        'priority',
        'retry_count',
        'message_id',
        'response',
        'error_message',
        'sent_at',
        'completed_at',
    ];

    protected $casts = [
        'payload' => 'array',
        'response' => 'array',
        'sent_at' => 'datetime',
        'completed_at' => 'datetime',
        'priority' => 'integer',
        'retry_count' => 'integer',
    ];

    /**
     * Relación con DispositivoEsp32
     */
    public function dispositivo(): BelongsTo
    {
        return $this->belongsTo(DispositivoEsp32::class, 'dispositivo_id');
    }

    /**
     * Generar un message_id único
     */
    public static function generateMessageId(): string
    {
        return (string) Str::uuid();
    }

    /**
     * Marcar comando como enviado
     */
    public function markAsSent(): void
    {
        $this->update([
            'status' => 'sent',
            'sent_at' => now(),
        ]);
    }

    /**
     * Marcar comando como completado
     */
    public function markAsCompleted(?array $response = null): void
    {
        $this->update([
            'status' => 'completed',
            'response' => $response,
            'completed_at' => now(),
        ]);
    }

    /**
     * Marcar comando como fallido
     */
    public function markAsFailed(string $errorMessage): void
    {
        $this->update([
            'status' => 'failed',
            'error_message' => $errorMessage,
            'retry_count' => $this->retry_count + 1,
        ]);
    }

    /**
     * Scope para comandos pendientes
     */
    public function scopePending($query)
    {
        return $query->where('status', 'pending');
    }

    /**
     * Scope para comandos por dispositivo
     */
    public function scopeForDevice($query, int $deviceId)
    {
        return $query->where('dispositivo_id', $deviceId);
    }

    /**
     * Scope para ordenar por prioridad
     */
    public function scopeOrderedByPriority($query)
    {
        return $query->orderBy('priority', 'desc')->orderBy('created_at', 'asc');
    }
}

