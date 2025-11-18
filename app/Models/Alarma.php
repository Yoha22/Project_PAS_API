<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;

class Alarma extends Model
{
    protected $table = 'alarmas';

    protected $fillable = [
        'fecha_hora',
        'descripcion',
    ];

    protected $casts = [
        'fecha_hora' => 'datetime',
    ];
}
