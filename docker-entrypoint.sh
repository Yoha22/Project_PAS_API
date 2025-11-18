#!/bin/sh
set -e

echo "=== Iniciando contenedor Laravel ==="

# Verificar variables de entorno críticas
echo "Verificando variables de entorno..."
echo "APP_ENV: ${APP_ENV:-no definida}"
echo "DB_CONNECTION: ${DB_CONNECTION:-no definida}"
echo "DB_HOST: ${DB_HOST:-no definida}"
echo "DB_PORT: ${DB_PORT:-no definida}"
echo "DB_DATABASE: ${DB_DATABASE:-no definida}"
echo "DB_USERNAME: ${DB_USERNAME:-no definida}"
echo "DB_PASSWORD: ${DB_PASSWORD:+***definida***}"

# Verificar que las variables críticas estén definidas
if [ -z "$DB_HOST" ] || [ "$DB_HOST" = "127.0.0.1" ] || [ "$DB_HOST" = "localhost" ]; then
  echo "ERROR: DB_HOST no está configurado correctamente. Valor actual: ${DB_HOST:-vacío}"
  echo "Por favor, verifica que las variables de entorno de la base de datos estén configuradas en Render."
  echo "Si estás usando render.yaml, asegúrate de que la base de datos 'laravel-db' esté creada."
  exit 1
fi

# Limpiar cache anterior si existe (importante antes de verificar conexión)
echo "Limpiando cache anterior..."
php artisan config:clear || true
php artisan route:clear || true
php artisan view:clear || true

# Esperar a que la base de datos esté lista
echo "Esperando conexión a la base de datos..."
max_attempts=30
attempt=0
connected=false

while [ $attempt -lt $max_attempts ]; do
  # Intentar una consulta simple para verificar la conexión
  if php artisan tinker --execute="DB::select('SELECT 1');" > /dev/null 2>&1; then
    echo "✓ Conexión a la base de datos exitosa!"
    connected=true
    break
  fi
  
  attempt=$((attempt + 1))
  echo "Intento $attempt/$max_attempts: Esperando conexión a la base de datos..."
  sleep 2
done

if [ "$connected" = "false" ]; then
  echo "ERROR: No se pudo conectar a la base de datos después de $max_attempts intentos"
  echo "Variables de entorno actuales:"
  echo "  DB_CONNECTION: ${DB_CONNECTION}"
  echo "  DB_HOST: ${DB_HOST}"
  echo "  DB_PORT: ${DB_PORT}"
  echo "  DB_DATABASE: ${DB_DATABASE}"
  echo "  DB_USERNAME: ${DB_USERNAME}"
  exit 1
fi

# Ejecutar migraciones
echo "Ejecutando migraciones..."
php artisan migrate --force

# Optimizar cache (después de que todo esté configurado)
echo "Optimizando cache..."
php artisan config:cache
php artisan route:cache
php artisan view:cache

echo "=== Servidor listo para iniciar ==="
# Ejecutar el comando principal
exec "$@"

