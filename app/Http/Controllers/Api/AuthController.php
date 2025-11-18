<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Administrador;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Hash;
use Illuminate\Validation\ValidationException;

class AuthController extends Controller
{
    public function register(Request $request)
    {
        // Verificar si ya existe un administrador
        $exists = Administrador::count() > 0;
        if ($exists) {
            return response()->json([
                'success' => false,
                'message' => 'Ya existe un administrador registrado.'
            ], 403);
        }

        $validated = $request->validate([
            'correo' => 'required|email|max:30|unique:administradores,correo',
            'password' => 'required|string|min:6|regex:/[\W]/',
            'telefono' => 'required|string|size:10|regex:/^[0-9]+$/',
            'codigo' => 'required|integer',
        ]);

        $administrador = Administrador::create([
            'correo' => $validated['correo'],
            'password' => Hash::make($validated['password']),
            'telefono_admin' => $validated['telefono'],
            'codigo' => $validated['codigo'],
        ]);

        $token = $administrador->createToken('auth_token')->plainTextToken;

        return response()->json([
            'success' => true,
            'message' => 'Administrador registrado exitosamente.',
            'data' => [
                'administrador' => $administrador,
                'token' => $token,
            ],
        ], 201);
    }

    public function login(Request $request)
    {
        $validated = $request->validate([
            'correo' => 'required|email',
            'password' => 'required|string',
        ]);

        $administrador = Administrador::where('correo', $validated['correo'])->first();

        if (!$administrador || !Hash::check($validated['password'], $administrador->password)) {
            throw ValidationException::withMessages([
                'correo' => ['Las credenciales proporcionadas son incorrectas.'],
            ]);
        }

        $token = $administrador->createToken('auth_token')->plainTextToken;

        return response()->json([
            'success' => true,
            'message' => 'Inicio de sesión exitoso',
            'data' => [
                'administrador' => $administrador,
                'token' => $token,
            ],
        ]);
    }

    public function logout(Request $request)
    {
        $request->user()->currentAccessToken()->delete();

        return response()->json([
            'success' => true,
            'message' => 'Sesión cerrada exitosamente',
        ]);
    }

    public function me(Request $request)
    {
        return response()->json([
            'success' => true,
            'data' => $request->user(),
        ]);
    }

    public function checkAdmin()
    {
        $exists = Administrador::count() > 0;
        return response()->json([
            'success' => true,
            'exists' => $exists,
        ]);
    }
}
