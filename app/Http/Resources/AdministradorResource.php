<?php

namespace App\Http\Resources;

use Illuminate\Http\Request;
use Illuminate\Http\Resources\Json\JsonResource;

class AdministradorResource extends JsonResource
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
            'correo' => $this->correo,
            'telefono_admin' => $this->telefono_admin,
            'codigo' => $this->codigo,
            'created_at' => $this->created_at,
            'updated_at' => $this->updated_at,
        ];
    }
}
