#!/bin/sh
set -e

# Esperar a que la base de datos esté lista (opcional, útil para producción)
# Puedes descomentar esto si necesitas esperar a la BD
# until php artisan db:monitor > /dev/null 2>&1; do
#   echo "Esperando conexión a la base de datos..."
#   sleep 2
# done

# Ejecutar migraciones
php artisan migrate --force

# Limpiar y optimizar cache
php artisan config:cache
php artisan route:cache
php artisan view:cache

# Ejecutar el comando principal
exec "$@"

