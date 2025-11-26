<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Alarma;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Log;

class AlarmaController extends Controller
{
    public function index(Request $request)
    {
        try {
            $alarmas = Alarma::orderBy('fecha_hora', 'desc')->paginate(15);
            
            return response()->json([
                'success' => true,
                'data' => $alarmas,
            ]);
        } catch (\Exception $e) {
            Log::error('Error en AlarmaController::index', [
                'message' => $e->getMessage(),
                'file' => $e->getFile(),
                'line' => $e->getLine(),
                'request' => $request->all(),
            ]);
            
            return response()->json([
                'success' => false,
                'message' => 'Error al obtener alarmas',
                'error' => config('app.debug') ? $e->getMessage() : 'Error interno del servidor'
            ], 500);
        }
    }

    public function store(Request $request)
    {
        $validated = $request->validate([
            'descripcion' => 'required|string|max:250',
            'fecha_hora' => 'nullable|date',
        ]);

        $alarma = Alarma::create([
            'descripcion' => $validated['descripcion'],
            'fecha_hora' => $validated['fecha_hora'] ?? now(),
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Alarma creada exitosamente.',
            'data' => $alarma,
        ], 201);
    }

    public function destroy(string $id)
    {
        $alarma = Alarma::findOrFail($id);
        $alarma->delete();

        return response()->json([
            'success' => true,
            'message' => 'Alarma eliminada correctamente.',
        ]);
    }
}
