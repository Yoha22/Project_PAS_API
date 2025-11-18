#!/bin/sh
set -e

echo "=== Iniciando contenedor Laravel ==="

# Verificar variables de entorno críticas
echo "Verificando variables de entorno..."
echo "APP_ENV: ${APP_ENV:-no definida}"
echo "DB_CONNECTION: ${DB_CONNECTION:-no definida}"
echo "DB_URL: ${DB_URL:+***definida***}"
echo "DB_HOST: ${DB_HOST:-no definida}"
echo "DB_PORT: ${DB_PORT:-no definida}"
echo "DB_DATABASE: ${DB_DATABASE:-no definida}"
echo "DB_USERNAME: ${DB_USERNAME:-no definida}"
echo "DB_PASSWORD: ${DB_PASSWORD:+***definida***}"

# Si DB_URL está disponible, Laravel la usará automáticamente
if [ -n "$DB_URL" ]; then
  echo "✓ DB_URL encontrada, Laravel la usará para la conexión"
fi

# Verificar que tengamos al menos DB_DATABASE, DB_USERNAME y DB_PASSWORD
if [ -z "$DB_DATABASE" ] || [ -z "$DB_USERNAME" ] || [ -z "$DB_PASSWORD" ]; then
  echo "ERROR: Faltan variables críticas de base de datos"
  echo "DB_DATABASE: ${DB_DATABASE:-vacío}"
  echo "DB_USERNAME: ${DB_USERNAME:-vacío}"
  echo "DB_PASSWORD: ${DB_PASSWORD:+definida}"
  echo ""
  echo "Por favor, verifica que estas variables estén configuradas en Render > Environment"
  exit 1
fi

# Si no tenemos DB_HOST pero tenemos DB_DATABASE, intentar continuar
# (Laravel usará valores por defecto o DB_URL si está disponible)
if [ -z "$DB_HOST" ] || [ "$DB_HOST" = "127.0.0.1" ] || [ "$DB_HOST" = "localhost" ]; then
  if [ -z "$DB_URL" ]; then
    echo "ADVERTENCIA: DB_HOST no está configurado correctamente. Valor: ${DB_HOST:-vacío}"
    echo "Intentando continuar... Si falla, configura DB_HOST o DB_URL en Render"
  else
    echo "DB_HOST no configurado, pero DB_URL está disponible - continuando..."
  fi
fi

# Si DB_PORT no está definido, usar el valor por defecto de PostgreSQL
if [ -z "$DB_PORT" ]; then
  echo "DB_PORT no definido, usando valor por defecto: 5432"
  export DB_PORT=5432
fi

# Si DB_HOST no está definido pero tenemos los otros datos, intentar construir DB_URL
# o usar el hostname de Render si está disponible
if [ -z "$DB_HOST" ] || [ "$DB_HOST" = "127.0.0.1" ] || [ "$DB_HOST" = "localhost" ]; then
  if [ -z "$DB_URL" ] && [ -n "$DB_DATABASE" ] && [ -n "$DB_USERNAME" ] && [ -n "$DB_PASSWORD" ]; then
    # Intentar construir DB_URL si no está disponible
    # Nota: Esto requiere que el usuario configure DB_HOST manualmente
    echo "ERROR: DB_HOST es requerido pero no está configurado"
    echo "Por favor, agrega DB_HOST=dpg-d4du80mmcj7s73caquso-a en Render > Environment"
    echo "O agrega DB_URL con la URL completa de conexión de PostgreSQL"
    exit 1
  fi
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

# Ejecutar seeders (para crear datos iniciales como administradores)
echo "Ejecutando seeders..."
php artisan db:seed --force

# Optimizar cache (después de que todo esté configurado)
echo "Optimizando cache..."
php artisan config:cache
php artisan route:cache
php artisan view:cache

echo "=== Servidor listo para iniciar ==="
# Ejecutar el comando principal
exec "$@"

