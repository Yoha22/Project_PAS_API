<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\HuellaTemporal;
use App\Models\Usuario;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Log;

class UsuarioController extends Controller
{
    public function index(Request $request)
    {
        try {
            $perPage = $request->get('per_page', 5);
            $usuarios = Usuario::paginate($perPage);
            
            return response()->json([
                'success' => true,
                'data' => $usuarios,
            ]);
        } catch (\Exception $e) {
            Log::error('Error en UsuarioController::index', [
                'message' => $e->getMessage(),
                'file' => $e->getFile(),
                'line' => $e->getLine(),
                'request' => $request->all(),
            ]);
            
            return response()->json([
                'success' => false,
                'message' => 'Error al obtener usuarios',
                'error' => config('app.debug') ? $e->getMessage() : 'Error interno del servidor'
            ], 500);
        }
    }

    public function store(Request $request)
    {
        $validated = $request->validate([
            'nombre' => 'required|string|min:2|max:50',
            'cedula' => 'required|string|regex:/^[0-9]+$/|unique:usuarios,cedula',
            'celular' => 'required|string|regex:/^[0-9]+$/',
            'huella_digital' => 'nullable|integer',
            'clave' => 'nullable|string|max:10|regex:/^[0-9]+$/',
        ]);

        // Si no se proporciona huella, intentar obtenerla de huellas_temporales
        if (empty($validated['huella_digital'])) {
            $huellaTemporal = HuellaTemporal::orderBy('id', 'desc')->first();
            $validated['huella_digital'] = $huellaTemporal ? $huellaTemporal->idHuella : 0;
        }

        $usuario = Usuario::create([
            'nombre' => $validated['nombre'],
            'cedula' => $validated['cedula'],
            'celular' => $validated['celular'],
            'huella_digital' => $validated['huella_digital'],
            'clave' => $validated['clave'] ?? null,
        ]);

        // Si se usó una huella temporal, eliminarla
        if ($validated['huella_digital'] != 0 && isset($huellaTemporal)) {
            $huellaTemporal->delete();
        }

        return response()->json([
            'success' => true,
            'message' => 'Usuario agregado exitosamente.',
            'data' => $usuario,
        ], 201);
    }

    public function show(string $id)
    {
        $usuario = Usuario::findOrFail($id);
        
        return response()->json([
            'success' => true,
            'data' => $usuario,
        ]);
    }

    public function update(Request $request, string $id)
    {
        $usuario = Usuario::findOrFail($id);
        
        $validated = $request->validate([
            'nombre' => 'required|string|min:2|max:50',
            'cedula' => 'required|string|regex:/^[0-9]+$/|unique:usuarios,cedula,' . $id,
            'celular' => 'required|string|regex:/^[0-9]+$/',
            'clave' => 'nullable|string|max:10|regex:/^[0-9]+$/',
        ]);

        $usuario->update($validated);

        return response()->json([
            'success' => true,
            'message' => 'Usuario actualizado correctamente.',
            'data' => $usuario->fresh(),
        ]);
    }

    public function destroy(string $id)
    {
        $usuario = Usuario::findOrFail($id);
        $usuario->delete();

        return response()->json([
            'success' => true,
            'message' => 'Usuario eliminado correctamente.',
        ]);
    }
}
