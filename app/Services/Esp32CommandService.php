<?php

namespace App\Services;

use App\Models\DispositivoEsp32;
use App\Models\Esp32Command;
use Illuminate\Support\Facades\Log;
use Illuminate\Support\Str;

class Esp32CommandService
{
    /**
     * Agregar comando a la cola
     */
    public function queueCommand(int $deviceId, string $command, array $payload = [], int $priority = 5): Esp32Command
    {
        $dispositivo = DispositivoEsp32::findOrFail($deviceId);

        $esp32Command = Esp32Command::create([
            'dispositivo_id' => $deviceId,
            'command' => $command,
            'payload' => $payload,
            'status' => 'pending',
            'priority' => $priority,
            'message_id' => Esp32Command::generateMessageId(),
        ]);

        Log::info('[ESP32-COMMAND] Comando encolado', [
            'device_id' => $deviceId,
            'command' => $command,
            'message_id' => $esp32Command->message_id,
        ]);

        return $esp32Command;
    }

    /**
     * Obtener comandos pendientes para un dispositivo
     */
    public function getPendingCommands(int $deviceId, int $limit = 10): \Illuminate\Database\Eloquent\Collection
    {
        return Esp32Command::forDevice($deviceId)
            ->pending()
            ->orderedByPriority()
            ->limit($limit)
            ->get();
    }

    /**
     * Marcar comando como completado
     */
    public function markCommandCompleted(string $messageId, ?array $response = null): bool
    {
        $command = Esp32Command::where('message_id', $messageId)->first();

        if (!$command) {
            Log::warning('[ESP32-COMMAND] Comando no encontrado para completar', [
                'message_id' => $messageId,
            ]);
            return false;
        }

        $command->markAsCompleted($response);

        Log::info('[ESP32-COMMAND] Comando completado', [
            'message_id' => $messageId,
            'device_id' => $command->dispositivo_id,
        ]);

        return true;
    }

    /**
     * Marcar comando como fallido
     */
    public function markCommandFailed(string $messageId, string $errorMessage): bool
    {
        $command = Esp32Command::where('message_id', $messageId)->first();

        if (!$command) {
            Log::warning('[ESP32-COMMAND] Comando no encontrado para marcar como fallido', [
                'message_id' => $messageId,
            ]);
            return false;
        }

        $command->markAsFailed($errorMessage);

        Log::error('[ESP32-COMMAND] Comando fallido', [
            'message_id' => $messageId,
            'device_id' => $command->dispositivo_id,
            'error' => $errorMessage,
        ]);

        return true;
    }

    /**
     * Reintentar comandos fallidos
     */
    public function retryFailedCommands(int $maxRetries = 3): int
    {
        $failedCommands = Esp32Command::where('status', 'failed')
            ->where('retry_count', '<', $maxRetries)
            ->where('created_at', '>', now()->subHours(24)) // Solo comandos de las últimas 24 horas
            ->get();

        $retried = 0;

        foreach ($failedCommands as $command) {
            $command->update([
                'status' => 'pending',
                'error_message' => null,
            ]);
            $retried++;
        }

        if ($retried > 0) {
            Log::info('[ESP32-COMMAND] Comandos reintentados', [
                'count' => $retried,
            ]);
        }

        return $retried;
    }

    /**
     * Limpiar comandos antiguos
     */
    public function cleanupOldCommands(int $daysOld = 7): int
    {
        $deleted = Esp32Command::where('created_at', '<', now()->subDays($daysOld))
            ->whereIn('status', ['completed', 'failed'])
            ->delete();

        if ($deleted > 0) {
            Log::info('[ESP32-COMMAND] Comandos antiguos eliminados', [
                'count' => $deleted,
            ]);
        }

        return $deleted;
    }
}

