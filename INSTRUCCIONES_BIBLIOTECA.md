# Biblioteca musical — integración VLC (versión candidata)

Proyecto basado en Exeldro Media Controls. Se conservan sus licencias y atribuciones.

## Funciones incluidas
- Lectura de etiquetas de archivos con TagLib: título, artista, álbum, año y duración.
- Importación de archivos y carpetas, búsqueda, ordenación, eliminación y persistencia.
- Panel de biblioteca bajo los controles de Media Controls.
- Selector de una fuente VLC existente en OBS; doble clic envía la cola a VLC, comenzando por la canción elegida.
- Avance/retroceso desde los controles originales de Exeldro.

## Uso
1. Instala VLC de 64 bits compatible con OBS y habilita 'Fuente de vídeo VLC'.
2. Crea una Fuente de vídeo VLC en OBS y hazla visible en una escena.
3. En Media Controls, pulsa ↻ y selecciona esa fuente.
4. Añade música con +Archivos o +Carpeta y haz doble clic en una canción.

## Compilación
El repositorio incluye los workflows originales de GitHub Actions. TagLib 2.1.1 se descarga durante la configuración CMake (requiere acceso a GitHub). Ejecuta el workflow de compilación de Windows del repositorio.

## Limitaciones pendientes de validar
No se ha ejecutado una compilación Windows ni una prueba dentro de OBS en este entorno. La reproducción aleatoria se desactiva al cargar la cola para mantener su orden. El doble clic reemplaza la playlist de la fuente VLC elegida. No se incluye aún gestión de múltiples listas independientes, arrastrar para reordenar, ni sincronización en vivo de la fila resaltada. Esta es una candidata de compilación, NO una versión certificada como terminada.
