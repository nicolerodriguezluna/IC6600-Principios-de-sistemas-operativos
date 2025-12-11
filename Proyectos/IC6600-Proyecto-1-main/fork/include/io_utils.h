#ifndef FORK_IO_UTILS_H
#define FORK_IO_UTILS_H

/* ============================================================================
 *  IO_UTILS.H — Utilidades de E/S y manejo de rutas
 * ============================================================================ */

#include "common.h"

typedef struct {
    char  **paths;
    size_t  len;
    size_t  cap;
} strvec_t;

void sv_init(strvec_t *v);
void sv_push(strvec_t *v, const char *s);
void sv_free(strvec_t *v);

int  read_file_text(const char *path, char **out_buf, size_t *out_len);
int  write_file_text(const char *path, const char *buf, size_t len);

int  list_files_with_suffix(const char *dir, const char *suffix, strvec_t *out);

void replace_extension(const char *in, const char *new_ext, char out[PATH_MAX]);
void join_path(const char *dir, const char *name, char out[PATH_MAX]);

#endif /* FORK_IO_UTILS_H */
