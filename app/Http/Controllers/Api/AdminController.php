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
        // NO incluir código ni password por seguridad
        $administradores = Administrador::select('id', 'correo', 'telefono_admin')->get();
        
        return response()->json([
            'success' => true,
            'data' => $administradores,
        ]);
    }

    public function show(string $id)
    {
        // NO incluir código ni password por seguridad
        $administrador = Administrador::select('id', 'correo', 'telefono_admin')
            ->findOrFail($id);
        
        return response()->json([
            'success' => true,
            'data' => $administrador,
        ]);
    }

    public function store(Request $request)
    {
        $validated = $request->validate([
            'correo' => 'required|email|max:30|unique:administradores,correo',
            'password' => 'required|string|min:6|regex:/[\W]/',
            'telefono' => 'required|string|size:10|regex:/^[0-9]+$/',
            'codigo' => 'required|integer|unique:administradores,codigo',
        ]);

        $administrador = Administrador::create([
            'correo' => $validated['correo'],
            'password' => Hash::make($validated['password']),
            'telefono_admin' => $validated['telefono'],
            'codigo' => $validated['codigo'],
        ]);

        // NO devolver password ni código en la respuesta
        return response()->json([
            'success' => true,
            'message' => 'Administrador creado exitosamente.',
            'data' => [
                'id' => $administrador->id,
                'correo' => $administrador->correo,
                'telefono_admin' => $administrador->telefono_admin,
            ],
        ], 201);
    }

    public function update(Request $request, string $id)
    {
        $administrador = Administrador::findOrFail($id);
        
        $validated = $request->validate([
            'correo' => 'required|email|max:30|unique:administradores,correo,' . $id,
            'telefono' => 'nullable|string|size:10|regex:/^[0-9]+$/',
            'password' => 'nullable|string|min:6|regex:/[\W]/',
            'codigo' => 'nullable|integer|unique:administradores,codigo,' . $id,
        ]);

        $updateData = [
            'correo' => $validated['correo'],
            'telefono_admin' => $validated['telefono'] ?? null,
        ];

        // Solo actualizar password si se proporciona
        if (isset($validated['password']) && $validated['password']) {
            $updateData['password'] = Hash::make($validated['password']);
        }

        // Solo actualizar código si se proporciona
        if (isset($validated['codigo']) && $validated['codigo']) {
            $updateData['codigo'] = $validated['codigo'];
        }

        $administrador->update($updateData);

        // NO devolver password ni código en la respuesta
        return response()->json([
            'success' => true,
            'message' => 'Administrador actualizado correctamente.',
            'data' => [
                'id' => $administrador->id,
                'correo' => $administrador->correo,
                'telefono_admin' => $administrador->telefono_admin,
            ],
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
