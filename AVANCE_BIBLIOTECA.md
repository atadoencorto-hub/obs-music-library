# Primera iteración: biblioteca compacta con metadatos

Se ha modificado **el código original de Exeldro**, conservando sus controles de fuentes OBS.

## Implementado en código
- Panel de biblioteca debajo de los controles existentes, compacto y horizontal.
- Lectura de **título, artista, álbum, año y duración** con TagLib.
- Carga de archivos o carpeta recursiva.
- Búsqueda instantánea, ordenación por columnas y eliminación de entradas.
- Persistencia de rutas en `music-library.json` en la configuración del plugin.
- Doble clic abre el archivo con la aplicación predeterminada del sistema.

## Pendiente (NO implementado aún)
- Conectar el doble clic con la fuente VLC de OBS, en lugar de abrir el archivo externamente.
- Sincronizar lista y cola de reproducción con la playlist VLC.
- Botones para reordenar y gestionar listas independientes.
- Pruebas de compilación Windows y pruebas reales dentro de OBS.

**No es una versión lista para instalar.** Es un primer avance del código fuente. No sustituir la DLL funcional hasta compilar y probar.

Se mantiene el archivo LICENSE y las atribuciones del proyecto original GPL-2.0.
