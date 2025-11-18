<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\HasMany;

class Usuario extends Model
{
    protected $table = 'usuarios';

    protected $fillable = [
        'nombre',
        'huella_digital',
        'cedula',
        'celular',
    ];

    public function accesos(): HasMany
    {
        return $this->hasMany(Acceso::class, 'usuario_id');
    }
}
