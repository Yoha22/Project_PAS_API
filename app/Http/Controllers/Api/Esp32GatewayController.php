<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\DispositivoEsp32;
use App\Models\Esp32Command;
use App\Services\Esp32CommandService;
use App\Services\Esp32RoutingService;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Log;
use Illuminate\Support\Facades\Http;

class Esp32GatewayController extends Controller
{
    protected Esp32CommandService $commandService;
    protected Esp32RoutingService $routingService;

    public function __construct(
        Esp32CommandService $commandService,
        Esp32RoutingService $routingService
    ) {
        $this->commandService = $commandService;
        $this->routingService = $routingService;
    }

    /**
     * Enviar comando a ESP32
     * POST /api/esp32/{deviceId}/command
     */
    public function sendCommand(Request $request, int $deviceId)
    {
        $validated = $request->validate([
            'command' => 'required|string|in:config,fingerprint_add,fingerprint_delete,status,control,sync',
            'payload' => 'nullable|array',
            'priority' => 'nullable|integer|min:1|max:10',
        ]);

        $dispositivo = DispositivoEsp32::findOrFail($deviceId);

        // Verificar que el dispositivo existe y está activo
        if (!$dispositivo->activo) {
            return response()->json([
                'success' => false,
                'error' => 'El dispositivo no está activo',
            ], 400);
        }

        $priority = $validated['priority'] ?? 5;
        $command = $this->commandService->queueCommand(
            $deviceId,
            $validated['command'],
            $validated['payload'] ?? [],
            $priority
        );

        // Intentar enviar el comando inmediatamente si el dispositivo está online
        $route = $this->routingService->routeCommand($deviceId);
        
        if ($route['is_online']) {
            $sent = $this->sendCommandImmediately($command, $route);
            
            if ($sent) {
                return response()->json([
                    'success' => true,
                    'message' => 'Comando enviado exitosamente',
                    'data' => [
                        'command_id' => $command->id,
                        'message_id' => $command->message_id,
                        'status' => $command->fresh()->status,
                    ],
                ]);
            }
        }

        return response()->json([
            'success' => true,
            'message' => 'Comando encolado. Se enviará cuando el dispositivo esté disponible.',
            'data' => [
                'command_id' => $command->id,
                'message_id' => $command->message_id,
                'status' => $command->status,
                'device_status' => $route['mode'],
            ],
        ]);
    }

    /**
     * Obtener estado del ESP32
     * GET /api/esp32/{deviceId}/status
     */
    public function getStatus(int $deviceId)
    {
        $dispositivo = DispositivoEsp32::findOrFail($deviceId);
        $route = $this->routingService->routeCommand($deviceId);

        // Si está en modo local, intentar obtener estado directamente
        if ($route['mode'] === 'http_local' && $dispositivo->ip_local) {
            try {
                $response = Http::timeout(5)
                    ->get("http://{$dispositivo->ip_local}/status");

                if ($response->successful()) {
                    return response()->json([
                        'success' => true,
                        'data' => array_merge([
                            'device_id' => $dispositivo->id,
                            'nombre' => $dispositivo->nombre,
                            'is_online' => true,
                            'connection_type' => 'http_local',
                        ], $response->json()),
                    ]);
                }
            } catch (\Exception $e) {
                Log::warning('[ESP32-GATEWAY] Error obteniendo estado local', [
                    'device_id' => $deviceId,
                    'error' => $e->getMessage(),
                ]);
            }
        }

        // Retornar estado desde base de datos
        return response()->json([
            'success' => true,
            'data' => [
                'device_id' => $dispositivo->id,
                'nombre' => $dispositivo->nombre,
                'is_online' => $dispositivo->is_online,
                'connection_type' => $dispositivo->connection_type,
                'last_heartbeat' => $dispositivo->last_heartbeat?->toIso8601String(),
                'ip_local' => $dispositivo->ip_local,
                'ultima_conexion' => $dispositivo->ultima_conexion?->toIso8601String(),
            ],
        ]);
    }

    /**
     * Listar comandos pendientes
     * GET /api/esp32/{deviceId}/commands
     */
    public function getCommands(Request $request, int $deviceId)
    {
        $dispositivo = DispositivoEsp32::findOrFail($deviceId);
        
        $status = $request->query('status', 'pending');
        $limit = $request->query('limit', 50);

        $query = Esp32Command::forDevice($deviceId);

        if ($status !== 'all') {
            $query->where('status', $status);
        }

        $comandos = $query->orderedByPriority()
            ->limit($limit)
            ->get();

        return response()->json([
            'success' => true,
            'data' => $comandos,
        ]);
    }

    /**
     * Configurar dispositivo remotamente
     * POST /api/esp32/{deviceId}/config
     */
    public function configureDevice(Request $request, int $deviceId)
    {
        $validated = $request->validate([
            'ssid' => 'nullable|string|max:100',
            'password' => 'nullable|string|max:100',
            'serverUrl' => 'nullable|string|url|max:255',
            'useHTTPS' => 'nullable|boolean',
        ]);

        return $this->sendCommand($request->merge([
            'command' => 'config',
            'payload' => $validated,
        ]), $deviceId);
    }

    /**
     * Agregar huella remotamente
     * POST /api/esp32/{deviceId}/fingerprint/add
     */
    public function addFingerprint(Request $request, int $deviceId)
    {
        $validated = $request->validate([
            'user_id' => 'required|integer|exists:usuarios,id',
        ]);

        return $this->sendCommand($request->merge([
            'command' => 'fingerprint_add',
            'payload' => $validated,
        ]), $deviceId);
    }

    /**
     * Eliminar huella remotamente
     * POST /api/esp32/{deviceId}/fingerprint/delete
     */
    public function deleteFingerprint(Request $request, int $deviceId)
    {
        $validated = $request->validate([
            'fingerprint_id' => 'required|integer',
        ]);

        return $this->sendCommand($request->merge([
            'command' => 'fingerprint_delete',
            'payload' => $validated,
        ]), $deviceId);
    }

    /**
     * Control remoto del dispositivo
     * POST /api/esp32/{deviceId}/control
     */
    public function controlDevice(Request $request, int $deviceId)
    {
        $validated = $request->validate([
            'action' => 'required|string|in:relay_on,relay_off,restart,reset_wifi',
        ]);

        return $this->sendCommand($request->merge([
            'command' => 'control',
            'payload' => $validated,
        ]), $deviceId);
    }

    /**
     * Enviar comando inmediatamente según el modo de conexión
     */
    private function sendCommandImmediately(Esp32Command $command, array $route): bool
    {
        try {
            if ($route['mode'] === 'websocket') {
                // Enviar vía WebSocket (se implementará en el WebSocket handler)
                Log::info('[ESP32-GATEWAY] Comando enviado vía WebSocket', [
                    'message_id' => $command->message_id,
                ]);
                // TODO: Implementar envío vía WebSocket
                return true;
            } elseif ($route['mode'] === 'http_local' && $route['ip_local']) {
                // Enviar vía HTTP local
                $response = Http::timeout(10)
                    ->post("http://{$route['ip_local']}/command", [
                        'message_id' => $command->message_id,
                        'command' => $command->command,
                        'payload' => $command->payload,
                    ]);

                if ($response->successful()) {
                    $command->markAsSent();
                    return true;
                }
            }
        } catch (\Exception $e) {
            Log::error('[ESP32-GATEWAY] Error enviando comando', [
                'message_id' => $command->message_id,
                'error' => $e->getMessage(),
            ]);
        }

        return false;
    }

    /**
     * Obtener comandos pendientes para el dispositivo conectado
     * GET /api/esp32/commands/pending
     */
    public function getPendingCommands(Request $request)
    {
        $dispositivo = $request->get('dispositivo');
        $limit = $request->query('limit', 10);

        $comandos = $this->commandService->getPendingCommands($dispositivo->id, $limit);

        return response()->json([
            'success' => true,
            'data' => $comandos->map(function ($comando) {
                return [
                    'message_id' => $comando->message_id,
                    'command' => $comando->command,
                    'payload' => $comando->payload,
                    'priority' => $comando->priority,
                    'created_at' => $comando->created_at->toIso8601String(),
                ];
            }),
        ]);
    }

    /**
     * Confirmar ejecución de comando
     * POST /api/esp32/commands/{messageId}/complete
     */
    public function completeCommand(Request $request, string $messageId)
    {
        $dispositivo = $request->get('dispositivo');
        $validated = $request->validate([
            'response' => 'nullable|array',
            'success' => 'required|boolean',
            'error' => 'nullable|string',
        ]);

        $comando = Esp32Command::where('message_id', $messageId)
            ->where('dispositivo_id', $dispositivo->id)
            ->first();

        if (!$comando) {
            return response()->json([
                'success' => false,
                'error' => 'Comando no encontrado',
            ], 404);
        }

        if ($validated['success']) {
            $this->commandService->markCommandCompleted($messageId, $validated['response'] ?? null);
        } else {
            $this->commandService->markCommandFailed($messageId, $validated['error'] ?? 'Error desconocido');
        }

        return response()->json([
            'success' => true,
            'message' => 'Comando procesado',
        ]);
    }
}

