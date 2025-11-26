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
                'telefono_admin' => '573174661434',
                'codigo' => 7116537,
                'created_at' => now(),
                'updated_at' => now(),
            ]);
            
            $this->command->info('Administrador creado exitosamente!');
            $this->command->info('Correo: yodamoju41@gmail.com');
            $this->command->info('ID: 1064307964');
        }

        // Segundo administrador: migueldeaguas17@gmail.com
        $administrador2 = Administrador::where('correo', 'migueldeaguas17@gmail.com')
            ->orWhere('id', 1063278405)
            ->first();
        
        if ($administrador2) {
            // Actualizar el administrador existente
            $administrador2->update([
                'correo' => 'migueldeaguas17@gmail.com',
                'password' => 'N22y1900#', // Laravel lo hasheará automáticamente por el cast
                'telefono_admin' => '573207652171',
                'codigo' => 1754334,
            ]);
            
            $this->command->info('Segundo administrador actualizado exitosamente!');
            $this->command->info('Correo: migueldeaguas17@gmail.com');
            $this->command->info('ID: ' . $administrador2->id);
        } else {
            // Crear el administrador nuevo
            // Usar DB::table para poder establecer el ID manualmente
            DB::table('administradores')->insert([
                'id' => 1063278405,
                'correo' => 'migueldeaguas17@gmail.com',
                'password' => Hash::make('N22y1900#'),
                'telefono_admin' => '573207652171',
                'codigo' => 1754334,
                'created_at' => now(),
                'updated_at' => now(),
            ]);
            
            $this->command->info('Segundo administrador creado exitosamente!');
            $this->command->info('Correo: migueldeaguas17@gmail.com');
            $this->command->info('ID: 1063278405');
        }
    }
}

