<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Http\Requests\Esp32AccesoRequest;
use App\Http\Requests\Esp32AlarmaRequest;
use App\Http\Requests\Esp32HuellaRequest;
use App\Http\Requests\Esp32RegisterRequest;
use App\Models\Acceso;
use App\Models\Administrador;
use App\Models\Alarma;
use App\Models\DispositivoEsp32;
use App\Models\HuellaTemporal;
use App\Models\Usuario;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Hash;

class Esp32Controller extends Controller
{
    /**
     * Registrar un nuevo dispositivo ESP32
     * POST /api/esp32/register
     */
    public function register(Esp32RegisterRequest $request)
    {
        $validated = $request->validated();

        // Verificar código del administrador
        $administrador = Administrador::where('codigo', $validated['codigo'])->first();

        if (!$administrador) {
            return response()->json([
                'success' => false,
                'error' => 'Código de registro inválido',
            ], 401);
        }

        // Crear dispositivo
        $dispositivo = DispositivoEsp32::create([
            'nombre' => $validated['nombre'],
            'token' => DispositivoEsp32::generarToken(),
            'administrador_id' => $administrador->id,
            'activo' => true,
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo registrado exitosamente',
            'data' => [
                'id' => $dispositivo->id,
                'nombre' => $dispositivo->nombre,
                'token' => $dispositivo->token, // Solo se envía una vez
            ],
        ], 201);
    }

    /**
     * Obtener configuración del dispositivo
     * GET /api/esp32/config
     */
    public function config(Request $request)
    {
        $dispositivo = $request->get('dispositivo');

        return response()->json([
            'success' => true,
            'data' => [
                'id' => $dispositivo->id,
                'nombre' => $dispositivo->nombre,
                'activo' => $dispositivo->activo,
            ],
        ]);
    }

    /**
     * Registrar huella temporal
     * POST /api/esp32/huella
     */
    public function huella(Esp32HuellaRequest $request)
    {
        $validated = $request->validated();

        $huella = HuellaTemporal::create([
            'idHuella' => $validated['idHuella'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Huella temporal registrada exitosamente',
            'data' => $huella,
        ], 201);
    }

    /**
     * Obtener información del usuario por ID de huella
     * GET /api/esp32/usuario/{idHuella}
     */
    public function getUsuarioByHuella(Request $request, int $idHuella)
    {
        $usuario = Usuario::where('huella_digital', $idHuella)->first();

        if (!$usuario) {
            return response()->json([
                'nombre' => '',
                'idUsuario' => 0,
            ]);
        }

        return response()->json([
            'nombre' => $usuario->nombre,
            'idUsuario' => $usuario->id,
        ]);
    }

    /**
     * Registrar acceso de usuario
     * POST /api/esp32/acceso
     */
    public function acceso(Esp32AccesoRequest $request)
    {
        $validated = $request->validated();

        $acceso = Acceso::create([
            'usuario_id' => $validated['idUsuario'],
            'fecha_hora' => now(),
            'tipo_acceso' => $validated['tipo_acceso'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Acceso registrado exitosamente',
            'data' => $acceso,
        ], 201);
    }

    /**
     * Registrar alarma
     * POST /api/esp32/alarma
     */
    public function alarma(Esp32AlarmaRequest $request)
    {
        $validated = $request->validated();

        $alarma = Alarma::create([
            'fecha_hora' => now(),
            'descripcion' => $validated['descripcion'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Alarma registrada exitosamente',
            'data' => $alarma,
        ], 201);
    }

    /**
     * Obtener teléfono del administrador
     * GET /api/esp32/admin/telefono
     */
    public function getAdminTelefono(Request $request)
    {
        $dispositivo = $request->get('dispositivo');

        if (!$dispositivo->administrador_id) {
            return response()->json([
                'telefono_admin' => '',
            ]);
        }

        $administrador = Administrador::find($dispositivo->administrador_id);

        if (!$administrador) {
            return response()->json([
                'telefono_admin' => '',
            ]);
        }

        return response()->json([
            'telefono_admin' => $administrador->telefono_admin ?? '',
        ]);
    }

    /**
     * Obtener código/API Key del administrador
     * GET /api/esp32/admin/codigo
     */
    public function getAdminCodigo(Request $request)
    {
        $dispositivo = $request->get('dispositivo');

        if (!$dispositivo->administrador_id) {
            return response()->json([
                'codigo' => '',
            ]);
        }

        $administrador = Administrador::find($dispositivo->administrador_id);

        if (!$administrador) {
            return response()->json([
                'codigo' => '',
            ]);
        }

        return response()->json([
            'codigo' => (string)($administrador->codigo ?? ''),
        ]);
    }

    /**
     * Listar todos los dispositivos (para panel web)
     * GET /api/dispositivos
     */
    public function index(Request $request)
    {
        $dispositivos = DispositivoEsp32::with('administrador')
            ->orderBy('created_at', 'desc')
            ->get();

        return response()->json([
            'success' => true,
            'data' => $dispositivos,
        ]);
    }

    /**
     * Obtener un dispositivo por ID
     * GET /api/dispositivos/{id}
     */
    public function show($id)
    {
        $dispositivo = DispositivoEsp32::with('administrador')->find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        return response()->json([
            'success' => true,
            'data' => $dispositivo,
        ]);
    }

    /**
     * Crear un nuevo dispositivo (desde panel web)
     * POST /api/dispositivos
     */
    public function store(Request $request)
    {
        $validated = $request->validate([
            'nombre' => 'required|string|max:100',
            'codigo' => 'required|string',
        ]);

        // Verificar código del administrador
        $administrador = Administrador::where('codigo', $validated['codigo'])->first();

        if (!$administrador) {
            return response()->json([
                'success' => false,
                'error' => 'Código de registro inválido',
            ], 401);
        }

        $dispositivo = DispositivoEsp32::create([
            'nombre' => $validated['nombre'],
            'token' => DispositivoEsp32::generarToken(),
            'administrador_id' => $administrador->id,
            'activo' => true,
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo creado exitosamente',
            'data' => $dispositivo,
        ], 201);
    }

    /**
     * Actualizar dispositivo
     * PUT /api/dispositivos/{id}
     */
    public function update(Request $request, $id)
    {
        $validated = $request->validate([
            'nombre' => 'required|string|max:100',
        ]);

        $dispositivo = DispositivoEsp32::find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        $dispositivo->update([
            'nombre' => $validated['nombre'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo actualizado exitosamente',
            'data' => $dispositivo,
        ]);
    }

    /**
     * Eliminar dispositivo
     * DELETE /api/dispositivos/{id}
     */
    public function destroy($id)
    {
        $dispositivo = DispositivoEsp32::find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        $dispositivo->delete();

        return response()->json([
            'success' => true,
            'message' => 'Dispositivo eliminado exitosamente',
        ]);
    }

    /**
     * Generar código de registro
     * POST /api/dispositivos/generate-code
     */
    public function generateCode(Request $request)
    {
        $administrador = Administrador::first();

        if (!$administrador || !$administrador->codigo) {
            return response()->json([
                'success' => false,
                'error' => 'No hay administrador con código configurado',
            ], 404);
        }

        return response()->json([
            'success' => true,
            'data' => [
                'codigo' => (string)$administrador->codigo,
            ],
        ]);
    }

    /**
     * Revocar token de dispositivo
     * POST /api/dispositivos/{id}/revoke-token
     */
    public function revokeToken($id)
    {
        $dispositivo = DispositivoEsp32::find($id);

        if (!$dispositivo) {
            return response()->json([
                'success' => false,
                'error' => 'Dispositivo no encontrado',
            ], 404);
        }

        $dispositivo->update([
            'activo' => false,
            'token' => DispositivoEsp32::generarToken(), // Generar nuevo token (el antiguo ya no funcionará)
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Token revocado exitosamente. El dispositivo quedó inactivo.',
            'data' => $dispositivo,
        ]);
    }
}
