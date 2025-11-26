<?php

namespace App\Services;

use App\Models\DispositivoEsp32;
use Illuminate\Support\Facades\Http;
use Illuminate\Support\Facades\Log;

class Esp32RoutingService
{
    /**
     * Detectar modo de conexión del dispositivo
     */
    public function detectConnectionMode(int $deviceId): string
    {
        $dispositivo = DispositivoEsp32::findOrFail($deviceId);

        // Si el dispositivo está marcado como online con WebSocket, usar WebSocket
        if ($dispositivo->is_online && $dispositivo->connection_type === 'websocket') {
            return 'websocket';
        }

        // Si tiene IP local, intentar conexión HTTP local
        if ($dispositivo->ip_local) {
            if ($this->checkLocalConnection($dispositivo->ip_local)) {
                return 'http_local';
            }
        }

        // Si está online pero no responde localmente, puede estar en otra red
        if ($dispositivo->is_online) {
            return 'websocket'; // Intentar WebSocket aunque no esté confirmado
        }

        return 'offline';
    }

    /**
     * Verificar si hay conexión local al ESP32
     */
    private function checkLocalConnection(string $ip): bool
    {
        try {
            $response = Http::timeout(2)
                ->connectTimeout(1)
                ->get("http://{$ip}/config");

            return $response->successful();
        } catch (\Exception $e) {
            return false;
        }
    }

    /**
     * Actualizar estado de conexión del dispositivo
     */
    public function updateDeviceConnectionStatus(
        int $deviceId,
        bool $isOnline,
        string $connectionType,
        ?string $websocketId = null
    ): void {
        $dispositivo = DispositivoEsp32::findOrFail($deviceId);
        $dispositivo->updateConnectionStatus($isOnline, $connectionType, $websocketId);

        Log::info('[ESP32-ROUTING] Estado de conexión actualizado', [
            'device_id' => $deviceId,
            'is_online' => $isOnline,
            'connection_type' => $connectionType,
        ]);
    }

    /**
     * Decidir ruta para enviar comando
     */
    public function routeCommand(int $deviceId): array
    {
        $mode = $this->detectConnectionMode($deviceId);
        $dispositivo = DispositivoEsp32::findOrFail($deviceId);

        return [
            'mode' => $mode,
            'device_id' => $deviceId,
            'ip_local' => $dispositivo->ip_local,
            'websocket_id' => $dispositivo->websocket_id,
            'is_online' => $dispositivo->is_online,
        ];
    }

    /**
     * Marcar dispositivo como offline
     */
    public function markDeviceOffline(int $deviceId): void
    {
        $dispositivo = DispositivoEsp32::findOrFail($deviceId);
        $dispositivo->markOffline();

        Log::info('[ESP32-ROUTING] Dispositivo marcado como offline', [
            'device_id' => $deviceId,
        ]);
    }
}

