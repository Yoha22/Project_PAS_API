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
        Schema::table('dispositivos_esp32', function (Blueprint $table) {
            $table->boolean('is_online')->default(false)->after('activo');
            $table->enum('connection_type', ['websocket', 'http_local', 'offline'])->default('offline')->after('is_online');
            $table->timestamp('last_heartbeat')->nullable()->after('connection_type');
            $table->string('websocket_id', 100)->nullable()->after('last_heartbeat'); // ID de conexión WebSocket
        });
    }

    /**
     * Reverse the migrations.
     */
    public function down(): void
    {
        Schema::table('dispositivos_esp32', function (Blueprint $table) {
            $table->dropColumn(['is_online', 'connection_type', 'last_heartbeat', 'websocket_id']);
        });
    }
};

