<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Acceso;
use App\Models\Usuario;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\DB;
use Illuminate\Support\Facades\Log;

class AccesoController extends Controller
{
    public function index(Request $request)
    {
        try {
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
        } catch (\Exception $e) {
            Log::error('Error en AccesoController::index', [
                'message' => $e->getMessage(),
                'file' => $e->getFile(),
                'line' => $e->getLine(),
                'request' => $request->all(),
            ]);
            
            return response()->json([
                'success' => false,
                'message' => 'Error al obtener accesos',
                'error' => config('app.debug') ? $e->getMessage() : 'Error interno del servidor'
            ], 500);
        }
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
        try {
            $mesActual = $request->get('mes', date('Y-m'));
            
            // Parsear año y mes para usar métodos portables de Laravel
            $parts = explode('-', $mesActual);
            $year = $parts[0] ?? date('Y');
            $month = $parts[1] ?? date('m');
            
            // Aperturas por día en el mes actual - usar whereYear y whereMonth en lugar de DATE_FORMAT
            $aperturasPorDia = Acceso::where('tipo_acceso', 'Apertura')
                ->whereYear('fecha_hora', $year)
                ->whereMonth('fecha_hora', $month)
                ->select(DB::raw('DATE(fecha_hora) as fecha'), DB::raw('COUNT(*) as cantidad'))
                ->groupBy('fecha')
                ->orderBy('fecha')
                ->get();
            
            // Total de aperturas en el mes actual
            $totalAperturas = Acceso::where('tipo_acceso', 'Apertura')
                ->whereYear('fecha_hora', $year)
                ->whereMonth('fecha_hora', $month)
                ->count();
            
            // Persona con más ingresos (todos los tiempos)
            $personaMasIngresos = Acceso::where('tipo_acceso', 'Apertura')
                ->select('usuario_id', DB::raw('COUNT(*) as cantidad'))
                ->groupBy('usuario_id')
                ->orderBy('cantidad', 'desc')
                ->with('usuario')
                ->first();
            
            // Fecha con más ingresos (todos los tiempos)
            $fechaMasIngresos = Acceso::where('tipo_acceso', 'Apertura')
                ->select(DB::raw('DATE(fecha_hora) as fecha'), DB::raw('COUNT(*) as cantidad'))
                ->groupBy('fecha')
                ->orderBy('cantidad', 'desc')
                ->first();
            
            // Verificar que la relación usuario existe antes de acceder
            $personaMasIngresosData = null;
            if ($personaMasIngresos && $personaMasIngresos->usuario) {
                $personaMasIngresosData = [
                    'nombre' => $personaMasIngresos->usuario->nombre,
                    'cantidad' => $personaMasIngresos->cantidad,
                ];
            }
            
            return response()->json([
                'success' => true,
                'data' => [
                    'aperturas_por_dia' => $aperturasPorDia ?? [],
                    'total_aperturas_mes' => $totalAperturas ?? 0,
                    'persona_mas_ingresos' => $personaMasIngresosData,
                    'fecha_mas_ingresos' => $fechaMasIngresos ? $fechaMasIngresos->fecha : null,
                ],
            ]);
        } catch (\Exception $e) {
            Log::error('Error en AccesoController::stats', [
                'message' => $e->getMessage(),
                'file' => $e->getFile(),
                'line' => $e->getLine(),
                'trace' => $e->getTraceAsString(),
                'request' => $request->all(),
            ]);
            
            return response()->json([
                'success' => false,
                'message' => 'Error al obtener estadísticas',
                'error' => config('app.debug') ? $e->getMessage() : 'Error interno del servidor'
            ], 500);
        }
    }
}
