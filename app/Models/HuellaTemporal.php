<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;

class HuellaTemporal extends Model
{
    protected $table = 'huellas_temporales';

    protected $fillable = [
        'idHuella',
    ];
}
