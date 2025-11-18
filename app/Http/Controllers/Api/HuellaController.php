<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\HuellaTemporal;
use Illuminate\Http\Request;

class HuellaController extends Controller
{
    public function store(Request $request)
    {
        $validated = $request->validate([
            'idHuella' => 'required|integer',
        ]);

        $huella = HuellaTemporal::create([
            'idHuella' => $validated['idHuella'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Huella temporal registrada exitosamente.',
            'data' => $huella,
        ], 201);
    }

    public function getTemporal()
    {
        $huella = HuellaTemporal::orderBy('id', 'desc')->first();
        
        if (!$huella) {
            return response()->json([
                'success' => false,
                'message' => 'No hay huellas temporales disponibles.',
            ], 404);
        }

        return response()->json([
            'success' => true,
            'data' => $huella,
        ]);
    }
}
