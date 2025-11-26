<?php

namespace Database\Seeders;

use App\Models\Administrador;
use Illuminate\Database\Seeder;
use Illuminate\Support\Facades\Hash;
use Illuminate\Support\Facades\DB;

class AdministradorSeeder extends Seeder
{
    /**
     * Run the database seeds.
     */
    public function run(): void
    {
        // Buscar si ya existe un administrador con este correo o ID
        $administrador = Administrador::where('correo', 'yodamoju41@gmail.com')
            ->orWhere('id', 1064307964)
            ->first();
        
        if ($administrador) {
            // Actualizar el administrador existente
            $administrador->update([
                'correo' => 'yodamoju41@gmail.com',
                'password' => 'N22y1900#', // Laravel lo hasheará automáticamente por el cast
                'telefono_admin' => '3174661434',
                'codigo' => 7116537,
            ]);
            
            $this->command->info('Administrador actualizado exitosamente!');
            $this->command->info('Correo: yodamoju41@gmail.com');
            $this->command->info('ID: ' . $administrador->id);
        } else {
            // Crear el administrador nuevo
            // Usar DB::table para poder establecer el ID manualmente
            DB::table('administradores')->insert([
                'id' => 1064307964,
                'correo' => 'yodamoju41@gmail.com',
                'password' => Hash::make('N22y1900#'),
                'telefono_admin' => '3174661434',
                'codigo' => 7116537,
                'created_at' => now(),
                'updated_at' => now(),
            ]);
            
            $this->command->info('Administrador creado exitosamente!');
            $this->command->info('Correo: yodamoju41@gmail.com');
            $this->command->info('ID: 1064307964');
        }
    }
}

