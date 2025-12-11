#ifndef IO_UTILS_H
#define IO_UTILS_H

/* ======================================================================
 *  IO_UTILS.H — Utilidades de E/S de archivos y manejo de rutas
 * ====================================================================== */

#include "common.h"

/* --- Contenedor dinámico para paths --- */
typedef struct
{
    char **paths;
    size_t len, cap;
} strvec_t;

/* --- Gestión de strvec_t --- */

void sv_init(strvec_t *v);                  /* se inicializa vacío */
void sv_push(strvec_t *v, const char *s);   /* agrega una copia del string */
void sv_free(strvec_t *v);                  /* libera todos los strings y el vector */

/* --- E/S de archivos completos en modo binario --- */
int read_file_text(const char *path, char **out_buf, size_t *out_len);   /* lee el archivo a memoria y agrega '\0' */
int write_file_text(const char *path, const char *buf, size_t len);      /* escribe exactamente 'len' bytes */

/* --- Descubrimiento de archivos en un directorio --- */
int list_files_with_suffix(const char *dir, const char *suffix, strvec_t *out);   /* llena 'out' con rutas */

/* --- Helpers de rutas --- */
void replace_extension(const char *in, const char *new_ext, char out[PATH_MAX]);  /* cambia la extensión */
void join_path(const char *dir, const char *name, char out[PATH_MAX]);            /* dir + '/' + name */

#endif
