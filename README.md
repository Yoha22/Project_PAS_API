# Project_PAS_API

<p align="center"><a href="https://laravel.com" target="_blank"><img src="https://raw.githubusercontent.com/laravel/art/master/logo-lockup/5%20SVG/2%20CMYK/1%20Full%20Color/laravel-logolockup-cmyk-red.svg" width="400" alt="Laravel Logo"></a></p>

<p align="center">
<a href="https://github.com/laravel/framework/actions"><img src="https://github.com/laravel/framework/workflows/tests/badge.svg" alt="Build Status"></a>
<a href="https://packagist.org/packages/laravel/framework"><img src="https://img.shields.io/packagist/dt/laravel/framework" alt="Total Downloads"></a>
<a href="https://packagist.org/packages/laravel/framework"><img src="https://img.shields.io/packagist/v/laravel/framework" alt="Latest Stable Version"></a>
<a href="https://packagist.org/packages/laravel/framework"><img src="https://img.shields.io/packagist/l/laravel/framework" alt="License"></a>
</p>

## Despliegue en Render con Docker

Esta API está configurada para desplegarse en Render usando Docker.

### Requisitos Previos

- Cuenta en [Render](https://render.com)
- Repositorio Git (GitHub, GitLab, o Bitbucket) con el código

### Pasos para Desplegar

1. **Conectar el Repositorio**
   - Inicia sesión en Render
   - Ve a Dashboard > New > Web Service
   - Conecta tu repositorio Git

2. **Configuración del Servicio (Despliegue Manual)**
   - **Runtime**: Docker
   - **Dockerfile Path**: `./Dockerfile`
   - **Docker Context**: `.`
   - **Build Command**: (dejar vacío, Render usará el Dockerfile)
   - **Start Command**: (dejar vacío, se usa el CMD del Dockerfile)

3. **Variables de Entorno (CRÍTICO - Configurar Manualmente)**

   Ve a tu servicio web en Render > **Environment** y agrega estas variables:

   **Requeridas:**
   - `APP_NAME`: `Laravel API`
   - `APP_ENV`: `production`
   - `APP_DEBUG`: `false`
   - `APP_KEY`: Genera con `php artisan key:generate` o déjalo que Render lo genere automáticamente
   - `APP_URL`: URL completa de tu servicio (ej: `https://laravel-api.onrender.com`)
     - **Nota**: Si no configuras esta variable, Laravel usará automáticamente `RENDER_EXTERNAL_URL`

   **Base de Datos PostgreSQL (CONFIGURAR MANUALMENTE):**
   
   Obtén estos valores desde tu servicio de base de datos en Render > "Connections":
   
   - `DB_CONNECTION`: `pgsql`
   - `DB_HOST`: `dpg-d4du80mmcj7s73caquso-a` (o el hostname de tu base de datos)
   - `DB_PORT`: `5432`
   - `DB_DATABASE`: `access_db_93sr` (o el nombre de tu base de datos)
   - `DB_USERNAME`: `access_db_93sr_user` (o el usuario de tu base de datos)
   - `DB_PASSWORD`: (copia la contraseña desde la página de conexiones de tu base de datos)
   
   **⚠️ IMPORTANTE**: Copia estos valores exactamente desde la página de conexiones de tu base de datos en Render.

   **Opcionales:**
   - `LOG_CHANNEL`: `stderr` (recomendado para Render)
   - `LOG_LEVEL`: `error`
   - `CACHE_DRIVER`: `file` o `redis` (si usas Redis)
   - `SESSION_DRIVER`: `file` o `redis`
   - `QUEUE_CONNECTION`: `sync` o `database`

4. **Base de Datos PostgreSQL**
   - Asegúrate de que tu base de datos PostgreSQL ya esté creada en Render
   - Las migraciones se ejecutarán automáticamente al iniciar el contenedor

5. **Despliegue**
   - Render construirá la imagen Docker automáticamente
   - El servicio estará disponible en la URL proporcionada

### Solución de Problemas

#### Error: "Connection refused" o "connection to server at 127.0.0.1"

Este error indica que las variables de entorno de la base de datos no están configuradas correctamente.

**Solución:**

1. **Verifica que la base de datos esté creada:**
   - Asegúrate de que la base de datos PostgreSQL esté creada en Render
   - El nombre debe coincidir con el especificado en `render.yaml` (por defecto: `laravel-db`)

2. **Si usas `render.yaml`:**
   - Verifica que el nombre de la base de datos en `render.yaml` coincida con el nombre real de tu base de datos en Render
   - Asegúrate de que el servicio web y la base de datos estén en el mismo "Blueprint" o grupo

3. **Si despliegas manualmente (TU CASO):**
   - Ve a tu servicio web en Render
   - Sección "Environment"
   - **Verifica que estas variables estén configuradas correctamente:**
     - `DB_CONNECTION=pgsql`
     - `DB_HOST=dpg-d4du80mmcj7s73caquso-a` (NO debe ser `127.0.0.1` o `localhost`)
     - `DB_PORT=5432`
     - `DB_DATABASE=access_db_93sr`
     - `DB_USERNAME=access_db_93sr_user`
     - `DB_PASSWORD`: (copia la contraseña desde la página de conexiones de tu base de datos)
   - **Cómo obtener estos valores:**
     1. Ve a tu servicio de base de datos `access_db_93sr` en Render
     2. Haz clic en "Connections" o "Info"
     3. Copia cada valor exactamente como aparece
   - **Verificación rápida**: Los logs del contenedor mostrarán los valores de estas variables al iniciar

4. **Verifica los logs:**
   - Los logs del contenedor mostrarán qué variables de entorno están disponibles
   - Busca mensajes como "DB_HOST: no definida" en los logs

### Construcción Local con Docker

Para probar localmente:

```bash
# Construir la imagen
docker build -t laravel-api .

# Ejecutar el contenedor
docker run -p 8000:8000 \
  -e APP_KEY=base64:tu-clave-aqui \
  -e DB_CONNECTION=pgsql \
  -e DB_HOST=tu-host \
  -e DB_DATABASE=tu-db \
  -e DB_USERNAME=tu-user \
  -e DB_PASSWORD=tu-password \
  laravel-api
```

### Archivos de Configuración

- `Dockerfile`: Configuración de la imagen Docker
- `docker-entrypoint.sh`: Script de inicio que ejecuta migraciones y optimiza cache
- `render.yaml`: Configuración para despliegue automático en Render
- `.dockerignore`: Archivos excluidos del contexto de Docker

### Notas Importantes

- El contenedor ejecuta automáticamente las migraciones al iniciar
- El cache de configuración, rutas y vistas se optimiza automáticamente
- Asegúrate de que `APP_KEY` esté configurado antes del primer despliegue
- Para producción, siempre usa `APP_DEBUG=false`

## About Laravel

Laravel is a web application framework with expressive, elegant syntax. We believe development must be an enjoyable and creative experience to be truly fulfilling. Laravel takes the pain out of development by easing common tasks used in many web projects, such as:

- [Simple, fast routing engine](https://laravel.com/docs/routing).
- [Powerful dependency injection container](https://laravel.com/docs/container).
- Multiple back-ends for [session](https://laravel.com/docs/session) and [cache](https://laravel.com/docs/cache) storage.
- Expressive, intuitive [database ORM](https://laravel.com/docs/eloquent).
- Database agnostic [schema migrations](https://laravel.com/docs/migrations).
- [Robust background job processing](https://laravel.com/docs/queues).
- [Real-time event broadcasting](https://laravel.com/docs/broadcasting).

Laravel is accessible, powerful, and provides tools required for large, robust applications.

## Learning Laravel

Laravel has the most extensive and thorough [documentation](https://laravel.com/docs) and video tutorial library of all modern web application frameworks, making it a breeze to get started with the framework.

You may also try the [Laravel Bootcamp](https://bootcamp.laravel.com), where you will be guided through building a modern Laravel application from scratch.

If you don't feel like reading, [Laracasts](https://laracasts.com) can help. Laracasts contains thousands of video tutorials on a range of topics including Laravel, modern PHP, unit testing, and JavaScript. Boost your skills by digging into our comprehensive video library.

## Laravel Sponsors

We would like to extend our thanks to the following sponsors for funding Laravel development. If you are interested in becoming a sponsor, please visit the [Laravel Partners program](https://partners.laravel.com).

### Premium Partners

- **[Vehikl](https://vehikl.com/)**
- **[Tighten Co.](https://tighten.co)**
- **[WebReinvent](https://webreinvent.com/)**
- **[Kirschbaum Development Group](https://kirschbaumdevelopment.com)**
- **[64 Robots](https://64robots.com)**
- **[Curotec](https://www.curotec.com/services/technologies/laravel/)**
- **[Cyber-Duck](https://cyber-duck.co.uk)**
- **[DevSquad](https://devsquad.com/hire-laravel-developers)**
- **[Jump24](https://jump24.co.uk)**
- **[Redberry](https://redberry.international/laravel/)**
- **[Active Logic](https://activelogic.com)**
- **[byte5](https://byte5.de)**
- **[OP.GG](https://op.gg)**

## Contributing

Thank you for considering contributing to the Laravel framework! The contribution guide can be found in the [Laravel documentation](https://laravel.com/docs/contributions).

## Code of Conduct

In order to ensure that the Laravel community is welcoming to all, please review and abide by the [Code of Conduct](https://laravel.com/docs/contributions#code-of-conduct).

## Security Vulnerabilities

If you discover a security vulnerability within Laravel, please send an e-mail to Taylor Otwell via [taylor@laravel.com](mailto:taylor@laravel.com). All security vulnerabilities will be promptly addressed.

## License

The Laravel framework is open-sourced software licensed under the [MIT license](https://opensource.org/licenses/MIT).
