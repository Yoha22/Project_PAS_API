<?php

namespace App\Http\Requests;

use Illuminate\Foundation\Http\FormRequest;

class RegisterAdminRequest extends FormRequest
{
    /**
     * Determine if the user is authorized to make this request.
     */
    public function authorize(): bool
    {
        return true;
    }

    /**
     * Get the validation rules that apply to the request.
     *
     * @return array<string, \Illuminate\Contracts\Validation\ValidationRule|array<mixed>|string>
     */
    public function rules(): array
    {
        return [
            'correo' => 'required|email|max:30|unique:administradores,correo',
            'password' => 'required|string|min:6|regex:/[\W]/',
            'telefono' => 'required|string|size:10|regex:/^[0-9]+$/',
            'codigo' => 'required|integer',
        ];
    }
}
