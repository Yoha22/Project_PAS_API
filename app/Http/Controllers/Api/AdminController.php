<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Administrador;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Hash;

class AdminController extends Controller
{
    public function index()
    {
        $administradores = Administrador::select('id', 'correo', 'telefono_admin', 'codigo')->get();
        
        return response()->json([
            'success' => true,
            'data' => $administradores,
        ]);
    }

    public function show(string $id)
    {
        $administrador = Administrador::select('id', 'correo', 'telefono_admin', 'codigo')
            ->findOrFail($id);
        
        return response()->json([
            'success' => true,
            'data' => $administrador,
        ]);
    }

    public function update(Request $request, string $id)
    {
        $administrador = Administrador::findOrFail($id);
        
        $validated = $request->validate([
            'correo' => 'required|email|max:30|unique:administradores,correo,' . $id,
            'telefono' => 'nullable|string|size:10|regex:/^[0-9]+$/',
            'codigo' => 'required|integer',
        ]);

        $administrador->update([
            'correo' => $validated['correo'],
            'telefono_admin' => $validated['telefono'] ?? null,
            'codigo' => $validated['codigo'],
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Administrador actualizado correctamente.',
            'data' => $administrador->fresh(['id', 'correo', 'telefono_admin', 'codigo']),
        ]);
    }

    public function destroy(string $id)
    {
        $administrador = Administrador::findOrFail($id);
        $administrador->delete();

        return response()->json([
            'success' => true,
            'message' => 'Administrador eliminado correctamente.',
        ]);
    }
}
