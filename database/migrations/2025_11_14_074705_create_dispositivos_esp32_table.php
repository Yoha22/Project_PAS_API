<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    /**
     * Run the migrations.
     */
    public function up(): void
    {
        Schema::create('dispositivos_esp32', function (Blueprint $table) {
            $table->id();
            $table->string('nombre', 100);
            $table->string('token', 64)->unique();
            $table->string('ip_local', 45)->nullable(); // IPv4 o IPv6
            $table->timestamp('ultima_conexion')->nullable();
            $table->boolean('activo')->default(true);
            $table->foreignId('administrador_id')->nullable()->constrained('administradores')->onDelete('set null');
            $table->timestamps();
            
            $table->index('token');
            $table->index('activo');
        });
    }

    /**
     * Reverse the migrations.
     */
    public function down(): void
    {
        Schema::dropIfExists('dispositivos_esp32');
    }
};
