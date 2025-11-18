<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Acceso;
use App\Models\Usuario;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\DB;

class AccesoController extends Controller
{
    public function index(Request $request)
    {
        $query = Acceso::with('usuario');
        
        // Filtros opcionales
        if ($request->has('usuario_id')) {
            $query->where('usuario_id', $request->usuario_id);
        }
        
        if ($request->has('tipo_acceso')) {
            $query->where('tipo_acceso', $request->tipo_acceso);
        }
        
        if ($request->has('fecha_desde')) {
            $query->whereDate('fecha_hora', '>=', $request->fecha_desde);
        }
        
        if ($request->has('fecha_hasta')) {
            $query->whereDate('fecha_hora', '<=', $request->fecha_hasta);
        }
        
        $accesos = $query->orderBy('fecha_hora', 'desc')->paginate(15);
        
        return response()->json([
            'success' => true,
            'data' => $accesos,
        ]);
    }

    public function store(Request $request)
    {
        $validated = $request->validate([
            'usuario_id' => 'required|exists:usuarios,id',
            'tipo_acceso' => 'required|in:Apertura,cierre',
            'fecha_hora' => 'nullable|date',
        ]);

        $acceso = Acceso::create([
            'usuario_id' => $validated['usuario_id'],
            'tipo_acceso' => $validated['tipo_acceso'],
            'fecha_hora' => $validated['fecha_hora'] ?? now(),
        ]);

        return response()->json([
            'success' => true,
            'message' => 'Acceso registrado exitosamente.',
            'data' => $acceso->load('usuario'),
        ], 201);
    }

    public function stats(Request $request)
    {
        $mesActual = $request->get('mes', date('Y-m'));
        
        // Aperturas por día en el mes actual
        $aperturasPorDia = Acceso::where('tipo_acceso', 'Apertura')
            ->whereRaw("DATE_FORMAT(fecha_hora, '%Y-%m') = ?", [$mesActual])
            ->select(DB::raw('DATE(fecha_hora) as fecha'), DB::raw('COUNT(*) as cantidad'))
            ->groupBy('fecha')
            ->orderBy('fecha')
            ->get();
        
        // Total de aperturas en el mes actual
        $totalAperturas = Acceso::where('tipo_acceso', 'Apertura')
            ->whereRaw("DATE_FORMAT(fecha_hora, '%Y-%m') = ?", [$mesActual])
            ->count();
        
        // Persona con más ingresos
        $personaMasIngresos = Acceso::where('tipo_acceso', 'Apertura')
            ->select('usuario_id', DB::raw('COUNT(*) as cantidad'))
            ->groupBy('usuario_id')
            ->orderBy('cantidad', 'desc')
            ->with('usuario')
            ->first();
        
        // Fecha con más ingresos
        $fechaMasIngresos = Acceso::where('tipo_acceso', 'Apertura')
            ->select(DB::raw('DATE(fecha_hora) as fecha'), DB::raw('COUNT(*) as cantidad'))
            ->groupBy('fecha')
            ->orderBy('cantidad', 'desc')
            ->first();
        
        return response()->json([
            'success' => true,
            'data' => [
                'aperturas_por_dia' => $aperturasPorDia,
                'total_aperturas_mes' => $totalAperturas,
                'persona_mas_ingresos' => $personaMasIngresos ? [
                    'nombre' => $personaMasIngresos->usuario->nombre,
                    'cantidad' => $personaMasIngresos->cantidad,
                ] : null,
                'fecha_mas_ingresos' => $fechaMasIngresos ? $fechaMasIngresos->fecha : null,
            ],
        ]);
    }
}
