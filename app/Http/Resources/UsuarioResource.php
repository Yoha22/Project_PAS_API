<?php

namespace App\Http\Resources;

use Illuminate\Http\Request;
use Illuminate\Http\Resources\Json\JsonResource;

class UsuarioResource extends JsonResource
{
    /**
     * Transform the resource into an array.
     *
     * @return array<string, mixed>
     */
    public function toArray(Request $request): array
    {
        return [
            'id' => $this->id,
            'nombre' => $this->nombre,
            'huella_digital' => $this->huella_digital,
            'cedula' => $this->cedula,
            'celular' => $this->celular,
            'created_at' => $this->created_at,
            'updated_at' => $this->updated_at,
        ];
    }
}
