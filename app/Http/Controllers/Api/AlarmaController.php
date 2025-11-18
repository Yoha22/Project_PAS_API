<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Alarma;
use Illuminate\Http\Request;

class AlarmaController extends Controller
{
    public function index(Request $request)
    {
        $alarmas = Alarma::orderBy('fecha_hora', 'desc')->paginate(15);
        
        return response()->json([
            'success' => true,
            'data' => $alarmas,
        ]);
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
