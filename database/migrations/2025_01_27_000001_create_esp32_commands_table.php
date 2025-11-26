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
        Schema::create('esp32_commands', function (Blueprint $table) {
            $table->id();
            $table->foreignId('dispositivo_id')->constrained('dispositivos_esp32')->onDelete('cascade');
            $table->string('command', 50); // config, fingerprint_add, fingerprint_delete, status, control, sync
            $table->json('payload')->nullable();
            $table->enum('status', ['pending', 'sent', 'completed', 'failed'])->default('pending');
            $table->tinyInteger('priority')->default(5); // 1-10, mayor = más prioritario
            $table->unsignedInteger('retry_count')->default(0);
            $table->string('message_id', 100)->nullable()->unique(); // UUID del mensaje
            $table->json('response')->nullable(); // Respuesta del ESP32
            $table->text('error_message')->nullable();
            $table->timestamp('sent_at')->nullable();
            $table->timestamp('completed_at')->nullable();
            $table->timestamps();
            
            $table->index(['dispositivo_id', 'status']);
            $table->index(['status', 'priority', 'created_at']);
            $table->index('message_id');
        });
    }

    /**
     * Reverse the migrations.
     */
    public function down(): void
    {
        Schema::dropIfExists('esp32_commands');
    }
};

